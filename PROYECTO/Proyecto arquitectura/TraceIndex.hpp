#pragma once
#include "SeismicTrace.hpp"
#include <map>
#include <memory>
#include <string>

#ifdef USE_BOOST
#include <boost/container/flat_map.hpp>
#include <boost/container/btree_map.hpp>
#endif

namespace seismic {

// ============================================================================
//  Interfaz común a los índices.
//  Mapea (inline, crossline) → índice posicional en el StorageBackend.
// ============================================================================
class TraceIndex {
public:
    virtual ~TraceIndex() = default;
    virtual void insert(const TraceKey& k, size_t idx) = 0;
    virtual bool find(const TraceKey& k, size_t& out) const = 0;
    virtual size_t size() const = 0;
    virtual size_t memoryBytes() const = 0;
    virtual std::string name() const = 0;
};

// ----------------------------------------------------------------------------
//  std::map (árbol rojo-negro).
//  Es la opción por defecto en la STL. Ventajas: inserción y borrado O(log n)
//  amortizados, iteración ordenada. Desventaja: cada nodo es una asignación
//  individual → mala localidad y overhead por punteros.
// ----------------------------------------------------------------------------
class StdMapIndex : public TraceIndex {
    std::map<TraceKey, size_t> m_;
public:
    void insert(const TraceKey& k, size_t i) override { m_[k] = i; }
    bool find(const TraceKey& k, size_t& out) const override {
        auto it = m_.find(k);
        if (it == m_.end()) return false;
        out = it->second;
        return true;
    }
    size_t size() const override { return m_.size(); }
    size_t memoryBytes() const override {
        // Cada nodo RB-tree ≈ 48 bytes (3 punteros + color + payload + padding).
        // Es una aproximación; sirve para comparar órdenes de magnitud.
        return m_.size() * 48;
    }
    std::string name() const override { return "std::map (RB-tree)"; }
};

#ifdef USE_BOOST
// ----------------------------------------------------------------------------
//  boost::container::flat_map: vector ordenado + búsqueda binaria.
//  Ventajas: localidad de caché excelente, construcción rápida en bulk.
//  Desventajas: inserción/borrado O(n) por desplazamiento; sin embargo,
//  si construimos el índice de una vez (como en este benchmark), es
//  la estructura más rápida y con menos memoria por entrada.
// ----------------------------------------------------------------------------
class BoostFlatMapIndex : public TraceIndex {
    boost::container::flat_map<TraceKey, size_t> m_;
public:
    void insert(const TraceKey& k, size_t i) override { m_[k] = i; }
    bool find(const TraceKey& k, size_t& out) const override {
        auto it = m_.find(k);
        if (it == m_.end()) return false;
        out = it->second;
        return true;
    }
    size_t size() const override { return m_.size(); }
    size_t memoryBytes() const override {
        // Un flat_map solo guarda los pares en un vector → overhead mínimo.
        return m_.capacity() * sizeof(std::pair<TraceKey, size_t>);
    }
    std::string name() const override { return "Boost flat_map"; }
};

// ----------------------------------------------------------------------------
//  boost::container::btree_map: B-tree con nodos multi-clave.
//  Ventajas: mejor localidad que un RB-tree (cada nodo contiene varias
//  claves, aprovechando la caché), inserción/borrado logarítmicos.
//  Desventajas: peor que flat_map para construcción bulk, pero mejor
//  si el índice se modifica dinámicamente.
// ----------------------------------------------------------------------------
class BoostBTreeIndex : public TraceIndex {
    boost::container::btree_map<TraceKey, size_t> m_;
public:
    void insert(const TraceKey& k, size_t i) override { m_[k] = i; }
    bool find(const TraceKey& k, size_t& out) const override {
        auto it = m_.find(k);
        if (it == m_.end()) return false;
        out = it->second;
        return true;
    }
    size_t size() const override { return m_.size(); }
    size_t memoryBytes() const override { return m_.size() * 40; }
    std::string name() const override { return "Boost btree_map"; }
};
#endif

// ----------------------------------------------------------------------------
//  Fábrica: devuelve todos los índices compilados en este binario.
//  Si Boost no estaba disponible al compilar, solo devuelve std::map.
// ----------------------------------------------------------------------------
inline std::vector<std::unique_ptr<TraceIndex>> makeAllIndexes() {
    std::vector<std::unique_ptr<TraceIndex>> v;
    v.emplace_back(new StdMapIndex());
#ifdef USE_BOOST
    v.emplace_back(new BoostFlatMapIndex());
    v.emplace_back(new BoostBTreeIndex());
#endif
    return v;
}

} // namespace seismic