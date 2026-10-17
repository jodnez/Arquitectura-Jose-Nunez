#pragma once
#include <cstddef>
#include <list>
#include <unordered_map>
#include <vector>

namespace seismic {

// ============================================================================
//  Prefetcher: caché LRU + carga anticipada de trazas vecinas.
// ----------------------------------------------------------------------------
//  Modela el comportamiento de una caché de trazas: cuando se accede a la
//  traza i, se cuentan hits/misses y, opcionalmente, se cargan (prefetch)
//  las trazas "vecinas" en el espacio sísmico (i+1, i+2, ...), de modo que
//  el siguiente acceso sea un hit.
//
//  Uso típico en sísmicamente real: al interpretar un horizonte, el usuario
//  salta entre trazas contiguas (secuencial en inline/crossline). Prefetch
//  ayuda a que el coste de E/S se solape con el cálculo.
// ============================================================================
class Prefetcher {
public:
    Prefetcher(size_t capacity, size_t depth)
        : capacity_(capacity), depth_(depth) {}

    void reset() {
        lru_.clear();
        cache_.clear();
        hits_ = misses_ = prefetches_ = 0;
    }

    // Simula un acceso al índice `idx`.
    // successorProvider(idx) devuelve los índices "vecinos" que se prefetchean.
    template <typename F>
    bool access(size_t idx, F&& successorProvider) {
        // --- 1) Contabilizar hit/miss del acceso actual ---
        const bool hit = cache_.count(idx) > 0;
        if (hit) { ++hits_; touch(idx); }
        else     { ++misses_; insertIntoCache(idx); }

        // --- 2) Prefetch de vecinos (si la profundidad > 0) ---
        if (depth_ > 0) {
            auto succ = successorProvider(idx);
            size_t n = 0;
            for (size_t s : succ) {
                if (n >= depth_) break;
                // Solo prefetcheamos lo que no esté ya en caché para no
                // contaminar la LRU con duplicados.
                if (!cache_.count(s)) {
                    insertIntoCache(s);
                    ++prefetches_;
                }
                ++n;
            }
        }
        return hit;
    }

    size_t hits()        const { return hits_; }
    size_t misses()      const { return misses_; }
    size_t prefetches()  const { return prefetches_; }
    size_t accesses()    const { return hits_ + misses_; }
    double hitRatio()    const {
        const size_t t = accesses();
        return t ? double(hits_) / double(t) : 0.0;
    }

private:
    // Mueve idx al frente de la LRU (más recientemente usado).
    void touch(size_t idx) {
        auto it = cache_.find(idx);
        if (it == cache_.end()) return;
        lru_.erase(it->second);
        lru_.push_front(idx);
        it->second = lru_.begin();
    }

    // Inserta idx en la caché. Si ya existía, primero lo quita de la LRU.
    // Si al insertar se supera la capacidad, expulsa el último de la LRU
    // (el menos recientemente usado).
    void insertIntoCache(size_t idx) {
        auto it = cache_.find(idx);
        if (it != cache_.end()) lru_.erase(it->second);
        lru_.push_front(idx);
        cache_[idx] = lru_.begin();

        if (cache_.size() > capacity_) {
            cache_.erase(lru_.back());
            lru_.pop_back();
        }
    }

    size_t capacity_;   // nº máximo de trazas en caché
    size_t depth_;      // nº de vecinos a prefetchear por acceso
    std::list<size_t> lru_;                                       // orden MRU→LRU
    std::unordered_map<size_t, std::list<size_t>::iterator> cache_; // O(1) lookup
    size_t hits_ = 0, misses_ = 0, prefetches_ = 0;
};

} // namespace seismic