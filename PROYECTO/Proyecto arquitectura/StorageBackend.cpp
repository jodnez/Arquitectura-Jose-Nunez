#include "StorageBackend.hpp"
#include <algorithm>

namespace seismic {

// ----------------------------------------------------------------------------
//  Estimación de memoria para vector:
//    * la capacidad del vector de trazas (no el size),
//    * más la capacidad de cada vector interno de muestras.
//  Usamos capacity() y no size() porque el vector reserva espacio extra.
// ----------------------------------------------------------------------------
size_t VectorStorage::memoryUsageBytes() const {
    size_t bytes = data_.capacity() * sizeof(SeismicTrace);
    for (const auto& t : data_)
        bytes += t.samples.capacity() * sizeof(float);
    return bytes;
}

// ----------------------------------------------------------------------------
//  Para deque: no hay un único bloque contiguo; aproximamos sumando por
//  cada traza el coste de su objeto + su buffer de muestras.
// ----------------------------------------------------------------------------
size_t DequeStorage::memoryUsageBytes() const {
    size_t bytes = 0;
    for (const auto& t : data_)
        bytes += sizeof(SeismicTrace) + t.samples.capacity() * sizeof(float);
    return bytes;
}

#ifdef USE_STXXL
// ----------------------------------------------------------------------------
//  STXXL: al construir, creamos el vector externo (vacío). STXXL abrirá
//  automáticamente el fichero configurado en ~/.stxxl (o el indicado por
//  la variable de entorno STXXL_CONFIG).
// ----------------------------------------------------------------------------
STXXLVectorStorage::STXXLVectorStorage() {
    samples_.reset(new stxxl::vector<float>());
}
STXXLVectorStorage::~STXXLVectorStorage() = default;

// ----------------------------------------------------------------------------
//  Inserción en STXXL:
//  Como no podemos asumir que el vector externo soporte "insertar rango"
//  de forma barata, hacemos push_back muestra a muestra. STXXL se encarga
//  de empaquetar en bloques de disco.
//  El push_back inicial y resize son un truco para forzar la creación del
//  primer bloque antes de añadir muestras reales.
// ----------------------------------------------------------------------------
void STXXLVectorStorage::append(SeismicTrace&& t) {
    Meta m;
    m.h         = t.header;
    m.offset    = samples_->size();
    m.nsamples  = (uint32_t)t.samples.size();

    // Forzamos escritura de al menos un bloque para que STXXL materialice
    // el buffer interno y no se quede "flotando" en RAM.
    samples_->push_back(0.0f);
    samples_->resize(m.offset);

    // Ahora sí, push_back real de las muestras.
    for (float v : t.samples) samples_->push_back(v);

    meta_.push_back(m);
}

// ----------------------------------------------------------------------------
//  Carga desde STXXL: cada acceso indexado puede generar una lectura de
//  bloque desde disco si no está en caché interna de STXXL. Por eso es
//  importante tener el prefetching (en main.cpp) para reducir misses.
// ----------------------------------------------------------------------------
bool STXXLVectorStorage::load(size_t i, SeismicTrace& out) const {
    if (i >= meta_.size()) return false;
    const Meta& m = meta_[i];
    out.header = m.h;
    out.samples.resize(m.nsamples);
    for (uint32_t k = 0; k < m.nsamples; ++k)
        out.samples[k] = (*samples_)[m.offset + k];
    return true;
}

// ----------------------------------------------------------------------------
//  La RAM "real" de este backend es solo la metadata (las muestras viven
//  en disco). Reportamos ese valor para que el benchmark muestre la
//  diferencia frente a vector/deque.
// ----------------------------------------------------------------------------
size_t STXXLVectorStorage::memoryUsageBytes() const {
    return meta_.capacity() * sizeof(Meta);
}
#endif

} // namespace seismic