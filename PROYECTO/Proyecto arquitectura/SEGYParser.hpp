#pragma once
#include "SeismicTrace.hpp"
#include <fstream>
#include <functional>
#include <string>

namespace seismic {

// ============================================================================
//  Cabecera binaria SEG-Y (400 bytes, tras 3200 bytes de cabecera textual).
// ----------------------------------------------------------------------------
//  Aquí solo guardamos lo indispensable para iterar el archivo:
//    - sampleInterval: dt en microsegundos (informativo).
//    - numSamples   : nº de muestras por defecto si la traza no lo redefine.
//    - dataFormat   : código de formato según SEG-Y:
//                       1 = IBM 4-byte float
//                       2 = int32 big-endian
//                       3 = int16 big-endian
//                       5 = IEEE 4-byte float big-endian
//                       8 = int8
//    - numTracesPerEnsemble: no lo usamos, pero lo dejamos por completitud.
// ============================================================================
struct SEGYBinaryHeader {
    int16_t sampleInterval        = 0;
    int16_t numSamples            = 0;
    int16_t dataFormat            = 0;
    int16_t numTracesPerEnsemble  = 0;
};

class SEGYParser {
public:
    // Callback invocado una vez por traza. Devuelve `false` para detener.
    // Pasamos la traza por rvalue (std::move) para evitar copias en el backend.
    using TraceCallback = std::function<bool(SeismicTrace&&, size_t)>;

    explicit SEGYParser(const std::string& path);

    // Abre el archivo y lee la cabecera binaria. Devuelve false si el
    // archivo es más pequeño que la cabecera textual+binaria.
    bool open();

    const SEGYBinaryHeader& binaryHeader() const { return binaryHeader_; }
    uint64_t fileSize() const { return fileSize_; }

    // Recorre todo el archivo, invocando el callback por cada traza.
    // Devuelve el número de trazas efectivamente leídas.
    size_t parse(const TraceCallback& cb);

    // Escribe un SEG-Y sintético con la forma de onda Ricker, formato IEEE float.
    static void writeSynthetic(const std::string& path,
                               int nInlines, int nCrosslines, int nSamples);

private:
    std::string      path_;
    std::ifstream    in_;
    SEGYBinaryHeader binaryHeader_;
    uint64_t         fileSize_ = 0;

    // Tamaños fijos definidos por el estándar SEG-Y:
    static constexpr size_t kTextualHeader = 3200;  // EBCDIC/ASCII, no lo usamos
    static constexpr size_t kBinaryHeader  = 400;
    static constexpr size_t kTraceHeader   = 240;

    void readBinaryHeader();
    bool readTraceHeader(TraceHeader& th);
    bool readSamples(std::vector<float>& out, int16_t n);

    // --- Utilidades de bajo nivel, todas privadas y estáticas ---
    // IBM float usa un formato de 32 bits con signo+exponente base 16+mantisa.
    // Es distinto de IEEE-754, así que hay que reescalarlo a mano.
    static float   ibmToIeee(uint32_t ibm);

    // SEG-Y siempre es big-endian (network byte order). En x86 no podemos
    // reinterpretar directamente, hay que ensamblar los bytes uno por uno.
    static int32_t readI32BE(const uint8_t* b);
    static int16_t readI16BE(const uint8_t* b);
};

} // namespace seismic