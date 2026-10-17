#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace seismic {
// ============================================================================
//  Cabecera de traza SEG-Y
// ----------------------------------------------------------------------------
//  En SEG-Y toda traza lleva 240 bytes de cabecera antes de las muestras.
//  Solo extraemos los campos que realmente vamos a usar. Los offsets (en bytes,
//  contados desde el inicio de la cabecera de traza) están documentados en el
//  estándar SEG-Y rev1 y se aplican más abajo, en SEGYParser::readTraceHeader.
// ============================================================================
struct TraceHeader {
    int32_t traceSeqLine      = 0;  // offset   0
    int32_t traceSeqFile      = 0;  // offset   4
    int32_t fieldRecord       = 0;  // offset   8
    int32_t traceNumber       = 0;  // offset  12
    int32_t energySourcePoint = 0;  // offset  16
    int32_t ensembleNo        = 0;  // offset  20  (CDP)
    int32_t sourceX           = 0;  // offset  72
    int32_t sourceY           = 0;  // offset  76
    int32_t groupX            = 0;  // offset  80
    int32_t groupY            = 0;  // offset  84
    int16_t numSamples        = 0;  // offset 114
    int16_t sampleInterval    = 0;  // offset 116
    int32_t inlineNo          = 0;  // offset 188  (no está en el estándar
                                    //              original; lo usan los
                                    //              volúmenes 3D post-stack)
    int32_t crosslineNo       = 0;  // offset 192
};

// ============================================================================
//  Traza completa: cabecera + muestras.
// ----------------------------------------------------------------------------
//  Usamos std::vector<float> porque:
//     * el tamaño de muestras varía por traza (numSamples),
//     * queremos almacenamiento contiguo para que el backend STL vector sea
//       realmente un bloque plano de floats en memoria,
//     * `float` es lo que devuelven los formatos 1/5 y da 4 bytes por muestra.
// ============================================================================
struct SeismicTrace {
    TraceHeader header;
    std::vector<float> samples;
};

// ============================================================================
//  Clave de indexación: par (inline, crossline).
// ----------------------------------------------------------------------------
//  Esta es la "coordenada lógica" con la que los intérpretes buscan una traza
//  dentro de un cubo 3D. Definimos operator< porque los índices basados en
//  árboles (std::map, flat_map, btree_map) requieren orden total.
// ============================================================================
struct TraceKey {
    int32_t inlineNo    = 0;
    int32_t crosslineNo = 0;

    bool operator<(const TraceKey& o) const {
        // Orden lexicográfico: primero inline, luego crossline.
        // Es el orden natural "línea por línea" del cubo sísmico.
        if (inlineNo != o.inlineNo) return inlineNo < o.inlineNo;
        return crosslineNo < o.crosslineNo;
    }
    bool operator==(const TraceKey& o) const {
        return inlineNo == o.inlineNo && crosslineNo == o.crosslineNo;
    }
};

// ============================================================================
//  Hash para usar TraceKey en unordered_map.
// ----------------------------------------------------------------------------
//  Aplicamos el algoritmo FNV-1a a los dos enteros. No es criptográfico,
//  pero es rápido y reparte bien para valores pequeños como inlines/crosslines.
// ============================================================================
struct TraceKeyHash {
    size_t operator()(const TraceKey& k) const {
        uint64_t h = 1469598103934665603ull;              // offset basis FNV-64
        h ^= (uint32_t)k.inlineNo;    h *= 1099511628211ull;
        h ^= (uint32_t)k.crosslineNo; h *= 1099511628211ull;
        return (size_t)h;
    }
};

} // namespace seismic