#include "SEGYParser.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace seismic {

SEGYParser::SEGYParser(const std::string& path) : path_(path) {}

// ----------------------------------------------------------------------------
//  Apertura del archivo y lectura de la cabecera binaria.
//  Guardamos el tamaño total en bytes para reportarlo en el benchmark.
// ----------------------------------------------------------------------------
bool SEGYParser::open() {
    in_.open(path_, std::ios::binary);
    if (!in_) return false;

    // Determinamos el tamaño y volvemos al inicio.
    in_.seekg(0, std::ios::end);
    fileSize_ = (uint64_t)in_.tellg();
    in_.seekg(0, std::ios::beg);

    // Un SEG-Y válido tiene al menos cabecera textual + binaria.
    if (fileSize_ < kTextualHeader + kBinaryHeader) return false;

    // Saltamos la cabecera textual (3200 bytes) que no vamos a usar.
    in_.seekg(kTextualHeader, std::ios::beg);
    readBinaryHeader();
    return true;
}

// ----------------------------------------------------------------------------
//  Lectura big-endian de enteros.
//  Ensamblamos byte por byte para ser portables (no hacemos memcpy + ntohl
//  porque eso asume <arpa/inet.h>, que no siempre existe).
// ----------------------------------------------------------------------------
int32_t SEGYParser::readI32BE(const uint8_t* b) {
    return (int32_t)((uint32_t)b[0] << 24 | (uint32_t)b[1] << 16 |
                     (uint32_t)b[2] << 8  | (uint32_t)b[3]);
}
int16_t SEGYParser::readI16BE(const uint8_t* b) {
    return (int16_t)(((uint16_t)b[0] << 8) | (uint16_t)b[1]);
}

// ----------------------------------------------------------------------------
//  Conversión IBM float (formato SEG-Y 1) → IEEE-754 float.
//
//  Layout IBM 360 (32 bits):
//      bit  31    : signo
//      bits 24-30 : exponente en base 16, con sesgo 64
//      bits  0-23 : fracción, interpretada como 0.f en base 16
//
//  IEEE-754 float:
//      bit  31    : signo
//      bits 23-30 : exponente en base 2, con sesgo 127
//      bits  0-22 : mantisa normalizada con bit implícito 1
//
//  La conversión directa:   valor = signo * (fracción / 2^24) * 16^(exp - 64)
//  Con `std::pow(16, exp-64)` hacemos el cambio de base del exponente.
// ----------------------------------------------------------------------------
float SEGYParser::ibmToIeee(uint32_t ibm) {
    // Caso especial: cero (exponente y fracción nulos).
    if ((ibm & 0x00FFFFFFu) == 0 && (ibm & 0x7F000000u) == 0) return 0.0f;

    const int      sign = (ibm >> 31) & 0x1;
    const int      exp  = (ibm >> 24) & 0x7F;
    const uint32_t frac = ibm & 0x00FFFFFFu;

    // (frac / 2^24) da un valor en [0,1) que representa 0.fracción en base 16.
    // 16^(exp - 64) reescala al rango real. Usamos double para no perder
    // precisión durante la operación intermedia.
    double v = (double)frac / (double)(1u << 24) * std::pow(16.0, exp - 64);
    return sign ? -(float)v : (float)v;
}

// ----------------------------------------------------------------------------
//  Cabecera binaria: leemos solo los campos que nos interesan.
//  Los offsets (16, 20, 24, 12) están definidos en el estándar SEG-Y rev1.
// ----------------------------------------------------------------------------
void SEGYParser::readBinaryHeader() {
    uint8_t buf[kBinaryHeader];
    in_.read((char*)buf, kBinaryHeader);
    binaryHeader_.sampleInterval       = readI16BE(buf + 16);
    binaryHeader_.numSamples           = readI16BE(buf + 20);
    binaryHeader_.dataFormat           = readI16BE(buf + 24);
    binaryHeader_.numTracesPerEnsemble = readI16BE(buf + 12);
}

// ----------------------------------------------------------------------------
//  Cabecera de traza: 240 bytes. Extraemos los campos que usamos.
//  Muchos campos no se leen (dejamos los bytes "en el limbo" una vez que
//  avanzamos el cursor del ifstream sobre toda la cabecera).
// ----------------------------------------------------------------------------
bool SEGYParser::readTraceHeader(TraceHeader& th) {
    uint8_t b[kTraceHeader];
    in_.read((char*)b, kTraceHeader);

    // gcount() nos dice cuántos bytes se leyeron realmente. Si es menos,
    // significa que alcanzamos el EOF: la traza está truncada.
    if (in_.gcount() != (std::streamsize)kTraceHeader) return false;

    th.traceSeqLine      = readI32BE(b + 0);
    th.traceSeqFile      = readI32BE(b + 4);
    th.fieldRecord       = readI32BE(b + 8);
    th.traceNumber       = readI32BE(b + 12);
    th.energySourcePoint = readI32BE(b + 16);
    th.ensembleNo        = readI32BE(b + 20);
    th.sourceX           = readI32BE(b + 72);
    th.sourceY           = readI32BE(b + 76);
    th.groupX            = readI32BE(b + 80);
    th.groupY            = readI32BE(b + 84);
    th.numSamples        = readI16BE(b + 114);
    th.sampleInterval    = readI16BE(b + 116);
    th.inlineNo          = readI32BE(b + 188);   // convención de volúmenes 3D
    th.crosslineNo       = readI32BE(b + 192);
    return true;
}

// ----------------------------------------------------------------------------
//  Lectura de las muestras según el dataFormat declarado en la cabecera.
//  Cada caso maneja su propio tamaño de muestra y su conversión de endianness.
// ----------------------------------------------------------------------------
bool SEGYParser::readSamples(std::vector<float>& out, int16_t n) {
    if (n <= 0) return false;
    out.resize(n);

    switch (binaryHeader_.dataFormat) {

        // ---------------- Formato 1: IBM float (4 bytes BE) ----------------
        case 1: {
            std::vector<uint32_t> raw(n);
            in_.read((char*)raw.data(), n * 4);
            if (in_.gcount() != (std::streamsize)(n * 4)) return false;

            for (int i = 0; i < n; ++i) {
                // Reinterpretamos cada uint32 como los 4 bytes big-endian que
                // acabamos de leer. En x86 el ifstream ya nos dio los bytes
                // en orden de memoria [b0,b1,b2,b3], pero al leerlos como
                // uint32 nativo quedan "al revés". Los reordenamos para
                // reconstruir el valor BE correcto antes de convertirlo.
                uint32_t b0 = (raw[i] >>  0) & 0xFF;
                uint32_t b1 = (raw[i] >>  8) & 0xFF;
                uint32_t b2 = (raw[i] >> 16) & 0xFF;
                uint32_t b3 = (raw[i] >> 24) & 0xFF;
                uint32_t v  = (b3 << 24) | (b2 << 16) | (b1 << 8) | b0;
                out[i] = ibmToIeee(v);
            }
        } break;

        // ---------------- Formato 5: IEEE float (4 bytes BE) ---------------
        case 5: {
            std::vector<uint32_t> raw(n);
            in_.read((char*)raw.data(), n * 4);
            if (in_.gcount() != (std::streamsize)(n * 4)) return false;

            for (int i = 0; i < n; ++i) {
                // Swap explícito de bytes: queremos que el uint32 "v" quede
                // con el mismo bit-pattern que IEEE-754 espera en memoria.
                uint32_t v = ((raw[i] & 0x000000FFu) << 24) |
                             ((raw[i] & 0x0000FF00u) << 8)  |
                             ((raw[i] & 0x00FF0000u) >> 8)  |
                             ((raw[i] & 0xFF000000u) >> 24);
                // memcpy en vez de reinterpret_cast<float*>: evita
                // aliasing UB (strict aliasing).
                std::memcpy(&out[i], &v, 4);
            }
        } break;

        // ---------------- Formato 2: int32 (4 bytes BE) --------------------
        case 2: {
            std::vector<uint8_t> raw(n * 4);
            in_.read((char*)raw.data(), n * 4);
            if (in_.gcount() != (std::streamsize)(n * 4)) return false;
            for (int i = 0; i < n; ++i)
                out[i] = (float)readI32BE(raw.data() + i * 4);
        } break;

        // ---------------- Formato 3: int16 (2 bytes BE) --------------------
        case 3: {
            std::vector<uint8_t> raw(n * 2);
            in_.read((char*)raw.data(), n * 2);
            if (in_.gcount() != (std::streamsize)(n * 2)) return false;
            for (int i = 0; i < n; ++i)
                out[i] = (float)readI16BE(raw.data() + i * 2);
        } break;

        // ---------------- Formato 8: int8 --------------------------------
        case 8: {
            std::vector<int8_t> raw(n);
            in_.read((char*)raw.data(), n);
            if (in_.gcount() != (std::streamsize)n) return false;
            for (int i = 0; i < n; ++i) out[i] = (float)raw[i];
        } break;

        default:
            throw std::runtime_error("Formato SEG-Y no soportado: " +
                                     std::to_string(binaryHeader_.dataFormat));
    }
    return true;
}

// ----------------------------------------------------------------------------
//  Bucle principal de parsing.
//  Reposicionamos el cursor justo después de las cabeceras (textual+binaria)
//  y vamos leyendo trazas hasta agotar el archivo o hasta que el callback
//  devuelva false (lo cual permite "early exit").
// ----------------------------------------------------------------------------
size_t SEGYParser::parse(const TraceCallback& cb) {
    in_.clear();  // limpiamos flags de error por si el ifstream quedó en EOF
    in_.seekg(kTextualHeader + kBinaryHeader, std::ios::beg);

    size_t n = 0;
    SeismicTrace t;
    while (true) {
        if (!readTraceHeader(t.header)) break;

        // Algunos archivos reales no redefinen numSamples por traza.
        // En ese caso usamos el valor por defecto de la cabecera binaria.
        int16_t ns = t.header.numSamples > 0 ? t.header.numSamples
                                             : binaryHeader_.numSamples;
        if (!readSamples(t.samples, ns)) break;

        // Pasamos la traza como rvalue: el callback puede moverla al backend.
        if (!cb(std::move(t), n)) break;

        // Tras el move, `t.samples` queda en estado válido pero vacío;
        // limpiamos por si el move no dejó el vector en estado predecible.
        t.samples.clear();
        ++n;
    }
    return n;
}

// ----------------------------------------------------------------------------
//  Escritura de un archivo SEG-Y sintético.
//  Usamos formato 5 (IEEE float big-endian) porque es el más simple de generar
//  y el más portable: no requiere conversión a IBM.
//  La forma de onda es una Ricker (segunda derivada de una gaussiana) con una
//  modulación senoidal dependiente de (inline+crossline) para que distintos
//  planos del cubo tengan amplitudes distintas y se note el efecto del
//  prefetching secuencial.
// ----------------------------------------------------------------------------
static void writeI32BE(std::ofstream& o, int32_t v) {
    uint8_t b[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16),
                     (uint8_t)(v >> 8),  (uint8_t)(v) };
    o.write((char*)b, 4);
}
static void writeI16BE(std::ofstream& o, int16_t v) {
    uint8_t b[2] = { (uint8_t)(v >> 8), (uint8_t)(v) };
    o.write((char*)b, 2);
}

void SEGYParser::writeSynthetic(const std::string& path,
                                int nInlines, int nCrosslines, int nSamples) {
    std::ofstream o(path, std::ios::binary);
    if (!o) throw std::runtime_error("No se pudo crear " + path);

    // --- 3200 bytes de cabecera textual (EBCDIC/ASCII en el estándar) -----
    // No necesitamos respetar el contenido; con rellenar con espacios basta
    // para que el parser reconozca que ya pasó ese bloque.
    std::vector<char> textual(kTextualHeader, ' ');
    const char* banner = "C 1 SYNTHETIC SEG-Y FILE - PROYECTO 17";
    std::memcpy(textual.data(), banner, std::strlen(banner));
    o.write(textual.data(), textual.size());

    // --- 400 bytes de cabecera binaria ------------------------------------
    std::vector<uint8_t> bh(kBinaryHeader, 0);
    auto put16 = [&](size_t off, int16_t v) {
        bh[off]   = (uint8_t)(v >> 8);
        bh[off+1] = (uint8_t)(v);
    };
    put16(12, 0);                  // trazas por ensemble: no aplica
    put16(16, 4000);               // sample interval = 4 ms
    put16(20, (int16_t)nSamples);  // muestras por traza (por defecto)
    put16(24, 5);                  // formato IEEE float
    o.write((char*)bh.data(), bh.size());

    const int16_t ns = (int16_t)nSamples;
    SeismicTrace t;
    t.samples.resize(ns);
    size_t idx = 0;

    // --- Bucle sobre todas las trazas ------------------------------------
    for (int il = 0; il < nInlines; ++il) {
        for (int xl = 0; xl < nCrosslines; ++xl, ++idx) {

            // Generación de la onda: Ricker centrada en ns/2, ancho ~ ns/6,
            // modulada por sin(0.15*(il+xl)). Así cada traza tiene una firma
            // única pero correlacionada con sus vecinas (útil para probar
            // el prefetch espacial).
            for (int s = 0; s < ns; ++s) {
                double tt = (s - ns / 2.0) / (double)(ns / 6.0);
                double ricker = (1.0 - tt * tt) * std::exp(-0.5 * tt * tt);
                double mod = std::sin(0.15 * (il + xl)) * 0.3;
                t.samples[s] = (float)(ricker + mod);
            }

            // Cabecera de traza (240 bytes). Solo rellenamos los campos que
            // el parser va a leer; el resto queda a cero.
            std::vector<uint8_t> th(kTraceHeader, 0);
            auto puti32 = [&](size_t off, int32_t v) {
                th[off]   = (uint8_t)(v >> 24);
                th[off+1] = (uint8_t)(v >> 16);
                th[off+2] = (uint8_t)(v >> 8);
                th[off+3] = (uint8_t)(v);
            };
            puti32(0,   (int32_t)idx + 1);
            puti32(4,   (int32_t)idx + 1);
            puti32(8,   1);
            puti32(12,  (int32_t)idx);
            puti32(72,  il * 25);   // SourceX: separación 25 m
            puti32(76,  xl * 25);   // SourceY
            puti32(80,  il * 25);
            puti32(84,  xl * 25);
            puti32(188, il);        // Inline
            puti32(192, xl);        // Crossline
            th[114] = (uint8_t)(ns >> 8);
            th[115] = (uint8_t)(ns);
            th[116] = (uint8_t)(4000 >> 8);
            th[117] = (uint8_t)(4000);
            o.write((char*)th.data(), th.size());

            // Muestras IEEE float big-endian: reordenamos bytes manualmente.
            std::vector<uint8_t> buf(ns * 4);
            for (int s = 0; s < ns; ++s) {
                uint32_t v;
                std::memcpy(&v, &t.samples[s], 4);   // bit-copy float→uint32
                buf[s*4+0] = (uint8_t)(v >> 24);
                buf[s*4+1] = (uint8_t)(v >> 16);
                buf[s*4+2] = (uint8_t)(v >> 8);
                buf[s*4+3] = (uint8_t)(v);
            }
            o.write((char*)buf.data(), buf.size());
        }
    }
}

} // namespace seismic