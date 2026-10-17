#pragma once
#include "SeismicTrace.hpp"
#include <deque>
#include <memory>
#include <string>
#include <vector>

#ifdef USE_STXXL
#include <stxxl/vector>
#endif

namespace seismic {

// ============================================================================
//  Interfaz común a todos los backends de almacenamiento de trazas.
// ----------------------------------------------------------------------------
//  El benchmark (main.cpp) solo conoce esta interfaz: así podemos intercambiar
//  STL vector / STL deque / STXXL vector sin tocar la lógica de benchmark.
// ============================================================================
class StorageBackend {
public:
    virtual ~StorageBackend() = default;

    // Añade una traza. Tomamos rvalue para permitir mover las muestras.
    virtual void append(SeismicTrace&& t) = 0;

    // Copia la traza i en `out`. Devuelve false si i está fuera de rango.
    virtual bool load(size_t i, SeismicTrace& out) const = 0;

    virtual size_t size() const = 0;

    // Estimación de cuánta RAM ocupa el backend (no cuenta STXXL en disco).
    virtual size_t memoryUsageBytes() const = 0;

    virtual std::string name() const = 0;
};

// ----------------------------------------------------------------------------
//  Backend #1: STL vector.
//  Ventaja: acceso aleatorio O(1), layout contiguo, iterable rápido.
//  Desventaja: reallocation al crecer → picos de memoria y copias.
// ----------------------------------------------------------------------------
class VectorStorage : public StorageBackend {
    std::vector<SeismicTrace> data_;
public:
    void append(SeismicTrace&& t) override { data_.push_back(std::move(t)); }
    bool load(size_t i, SeismicTrace& out) const override {
        if (i >= data_.size()) return false;
        out = data_[i];               // copia (el caller no debe modificar)
        return true;
    }
    size_t size() const override { return data_.size(); }
    size_t memoryUsageBytes() const override;
    std::string name() const override { return "STL vector"; }
};

// ----------------------------------------------------------------------------
//  Backend #2: STL deque.
//  Ventaja: crecimiento sin reallocation global (bloques encadenados),
//           push_back muy estable.
//  Desventaja: peor localidad de caché que vector, iteración algo más lenta.
//  Compararlo con vector es interesante cuando el dataset es grande.
// ----------------------------------------------------------------------------
class DequeStorage : public StorageBackend {
    std::deque<SeismicTrace> data_;
public:
    void append(SeismicTrace&& t) override { data_.push_back(std::move(t)); }
    bool load(size_t i, SeismicTrace& out) const override {
        if (i >= data_.size()) return false;
        out = data_[i];
        return true;
    }
    size_t size() const override { return data_.size(); }
    size_t memoryUsageBytes() const override;
    std::string name() const override { return "STL deque"; }
};

// ----------------------------------------------------------------------------
//  Backend #3: STXXL vector (memoria externa).
//  STXXL gestiona sus propios bloques en disco (ficheros .stxxl) y los
//  mueve a RAM bajo demanda. El vector que ve el usuario se comporta como
//  std::vector pero las operaciones son de E/S.
//
//  Estrategia: guardamos en RAM solo la metadata de cada traza (cabecera +
//  offset + tamaño) y ponemos las muestras en un stxxl::vector<float>.
//  Esto reduce la RAM de O(#trazas × #muestras) a O(#trazas).
// ----------------------------------------------------------------------------
#ifdef USE_STXXL
class STXXLVectorStorage : public StorageBackend {
    struct Meta {
        TraceHeader h;      // copia de la cabecera (240 bytes → ~40 útiles)
        uint64_t    offset; // posición inicial en samples_
        uint32_t    nsamples;
    };
    std::vector<Meta> meta_;                            // metadata en RAM
    std::unique_ptr<stxxl::vector<float>> samples_;     // muestras en disco

public:
    STXXLVectorStorage();
    ~STXXLVectorStorage() override;

    void append(SeismicTrace&& t) override;
    bool load(size_t i, SeismicTrace& out) const override;
    size_t size() const override { return meta_.size(); }
    size_t memoryUsageBytes() const override;
    std::string name() const override { return "STXXL vector (externo)"; }
};
#endif

} // namespace seismic