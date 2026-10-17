#include "Prefetcher.hpp"
#include "SEGYParser.hpp"
#include "StorageBackend.hpp"
#include "TraceIndex.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <sys/stat.h>

using namespace seismic;

// Alias para el reloj de alta resolución (monotónico) que usaremos en todos
// los benchmarks. NO usamos system_clock porque puede retroceder con NTP.
using Clock = std::chrono::high_resolution_clock;

// Devuelve milisegundos entre dos time_points.
static double ms(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

// Formatea bytes con la unidad adecuada (KB, MB, GB...).
static std::string humanBytes(size_t b) {
    const char* u[] = {"B","KB","MB","GB","TB"};
    int i = 0; double x = (double)b;
    while (x >= 1024.0 && i < 4) { x /= 1024.0; ++i; }
    std::ostringstream o; o << std::fixed << std::setprecision(2) << x << " " << u[i];
    return o.str();
}

static bool fileExists(const std::string& p) {
    struct stat st;
    return ::stat(p.c_str(), &st) == 0;
}

// ============================================================================
//  Patrones de acceso al dataset
// ----------------------------------------------------------------------------
//  * Sequential: el patrón ideal (todo hits si hay prefetching secuencial).
//  * Reverse   : mismo dataset recorrido al revés; el prefetch secuencial
//                hacia adelante NO ayuda, así que sirve de contraste.
//  * Random    : peor caso: ningún patrón predecible.
//  * Strided   : accesos con paso k (aquí k=7); patrón "con salto" típico
//                en barridos multidimensionales.
// ============================================================================
enum class Pattern { Sequential, Reverse, Random, Strided };

static std::vector<size_t> makeAccessSequence(size_t n, Pattern p, int stride = 7) {
    std::vector<size_t> v(n);
    std::iota(v.begin(), v.end(), 0);   // [0,1,2,...,n-1]

    switch (p) {
        case Pattern::Sequential: break;
        case Pattern::Reverse:    std::reverse(v.begin(), v.end()); break;
        case Pattern::Random: {
            // Semilla fija = resultados reproducibles entre corridas.
            std::mt19937_64 g(42);
            std::shuffle(v.begin(), v.end(), g);
        } break;
        case Pattern::Strided: {
            // (i*stride) % n recorre todos los índices si gcd(stride,n)=1.
            // Usamos n potencia de 2 y stride impar para garantizarlo.
            for (size_t i = 0; i < n; ++i) v[i] = (i * stride) % n;
        } break;
    }
    return v;
}

static const char* patternName(Pattern p) {
    switch (p) {
        case Pattern::Sequential: return "Secuencial";
        case Pattern::Reverse:    return "Inverso";
        case Pattern::Random:     return "Aleatorio";
        case Pattern::Strided:    return "Con salto";
    }
    return "?";
}

// ============================================================================
//  Resultado agregado de un benchmark de patrón.
// ============================================================================
struct BenchResult {
    double totalMs  = 0;   // tiempo total del bucle de accesos
    double hitRatio = 0;   // hits / (hits + misses)
    size_t loads    = 0;   // nº de veces que hubo que llamar a backend->load
};

// ============================================================================
//  Ejecuta un patrón completo contra un backend.
//  * usePrefetch = false ⇒ profundidad 0 (solo LRU).
//  * El "successorProvider" devuelve hasta 8 vecinos consecutivos. En un caso
//    real se sustituiría por la geometría del cubo (inlines vecinos, etc.).
// ============================================================================
static BenchResult runPattern(StorageBackend& st, Pattern p,
                              bool usePrefetch, size_t cacheCap, size_t depth) {
    BenchResult r;
    const size_t N = st.size();
    auto seq = makeAccessSequence(N, p);

    Prefetcher pf(cacheCap, usePrefetch ? depth : 0);
    SeismicTrace buf;   // reutilizamos el buffer para evitar allocs

    auto successor = [N](size_t idx) {
        std::vector<size_t> s;
        for (int k = 1; k <= 8 && idx + k < N; ++k) s.push_back(idx + k);
        return s;
    };

    auto t0 = Clock::now();
    for (size_t i : seq) {
        bool hit = pf.access(i, successor);
        // Solo tocamos el backend cuando hay miss: así medimos el coste real
        // del sistema (LRU + prefetch + E/S).
        if (!hit) {
            st.load(i, buf);
            ++r.loads;
        }
    }
    auto t1 = Clock::now();

    r.totalMs  = ms(t0, t1);
    r.hitRatio = pf.hitRatio();
    return r;
}

// ============================================================================
//  main
// ============================================================================
int main(int argc, char** argv) {
    std::string inputPath;
    std::string genPath = "data/synthetic.sgy";
    int  genInl = 0, genXl = 0, genNs = 0;
    size_t cacheCap = 64;   // capacidad por defecto de la caché de trazas
    size_t depth    = 4;    // profundidad por defecto de prefetch

    // ---------- Parseo de argumentos ----------
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "-i" || a == "--input")  inputPath = argv[++i];
        else if (a == "--gen") {
            genInl = std::atoi(argv[++i]);
            genXl  = std::atoi(argv[++i]);
            genNs  = std::atoi(argv[++i]);
        }
        else if (a == "-o" || a == "--output") genPath = argv[++i];
        else if (a == "--cache-size")          cacheCap = std::stoul(argv[++i]);
        else if (a == "--prefetch-depth")      depth = std::stoul(argv[++i]);
        else if (a == "-h" || a == "--help") {
            std::cout << "Uso: seismic_sim [-i file.sgy] [--gen NINL NXL NS]\n"
                         "                  [-o path] [--cache-size N] [--prefetch-depth N]\n";
            return 0;
        }
    }

    // ---------- Generación de SEG-Y sintético si hace falta ----------
    if (inputPath.empty()) {
        if (genInl <= 0) { genInl = 64; genXl = 64; genNs = 256; }
        if (!fileExists(genPath)) {
            std::cout << "[+] Generando SEG-Y sintetico (" << genInl << "x"
                      << genXl << " x " << genNs << " muestras) en "
                      << genPath << " ...\n";
            
            // Creación de directorio multiplataforma robusta sin -p en Windows
#if defined(_WIN32)
            ::system("mkdir data 2>nul");
#else
            ::system("mkdir -p data");
#endif

            SEGYParser::writeSynthetic(genPath, genInl, genXl, genNs);
        }
        inputPath = genPath;
    }

    std::cout << "\n============================================================\n"
              << "  PROYECTO 17 - SIMULADOR DE DATOS SISMICOS (SEG-Y)\n"
              << "============================================================\n\n";

    // ---------- 1) Abrir el archivo y leer la cabecera binaria ----------
    SEGYParser parser(inputPath);
    if (!parser.open()) {
        std::cerr << "[-] No se pudo abrir " << inputPath << "\n";
        return 1;
    }
    const auto& bh = parser.binaryHeader();
    std::cout << "Archivo : " << inputPath << "\n"
              << "Tamano  : " << humanBytes(parser.fileSize()) << "\n"
              << "Formato : " << bh.dataFormat
              << "   |   Muestras/traza: " << bh.numSamples
              << "   |   dt: " << bh.sampleInterval << " us\n\n";

    // ---------- 2) Cargar el archivo en cada backend disponible ----------
    // Usamos unique_ptr para poder tener distintos backends en un vector.
    std::vector<std::unique_ptr<StorageBackend>> backends;
    backends.emplace_back(new VectorStorage());
    backends.emplace_back(new DequeStorage());
#ifdef USE_STXXL
    backends.emplace_back(new STXXLVectorStorage());
    std::cout << "[i] STXXL detectado: backend en memoria externa habilitado.\n";
#else
    std::cout << "[i] STXXL no disponible (compilado sin -DUSE_STXXL).\n";
#endif

    std::cout << "\n--- Carga del archivo en los backends ---\n";
    for (auto& b : backends) {
        // Reabrimos el parser para cada backend: parse() deja el ifstream
        // al final del archivo, así que open() lo reinicia.
        parser.open();

        auto t0 = Clock::now();
        size_t count = parser.parse([&](SeismicTrace&& t, size_t) {
            b->append(std::move(t));   // move: sin copia de las muestras
            return true;
        });
        auto t1 = Clock::now();

        std::cout << std::left << std::setw(26) << b->name()
                  << " | trazas: " << std::setw(8) << count
                  << " | tiempo: " << std::setw(8) << std::fixed << std::setprecision(1)
                  << ms(t0, t1) << " ms"
                  << " | RAM aprox: " << humanBytes(b->memoryUsageBytes()) << "\n";
    }

    // ---------- 3) Construcción de índices ----------
    // Para cada índice disponible, construimos uno nuevo desde cero y medimos
    // tiempo de construcción + memoria estimada del índice.
    std::cout << "\n--- Construccion de indices (clave inline,crossline) ---\n";
    struct IndexResult { std::string name; double buildMs; size_t bytes; };
    std::vector<IndexResult> idxRes;

    // makeAllIndexes() devuelve "protótipos" — los usamos solo para saber
    // qué clases están compiladas y con qué nombre. Luego creamos una copia
    // nueva por cada corrida para no arrastrar estado.
    for (auto& proto : makeAllIndexes()) {
        auto idxs = makeAllIndexes();
        std::unique_ptr<TraceIndex> idx;
        for (auto& cand : idxs) {
            if (cand->name() == proto->name()) { idx = std::move(cand); break; }
        }
        if (!idx) continue;

        auto t0 = Clock::now();
        const auto& base = backends[0];   // indexamos a partir del vector
        SeismicTrace buf;
        for (size_t i = 0; i < base->size(); ++i) {
            base->load(i, buf);
            idx->insert({buf.header.inlineNo, buf.header.crosslineNo}, i);
        }
        auto t1 = Clock::now();

        idxRes.push_back({ idx->name(), ms(t0, t1), idx->memoryBytes() });
    }
    for (auto& r : idxRes)
        std::cout << std::left << std::setw(24) << r.name
                  << " | build: " << std::setw(8) << std::fixed << std::setprecision(1)
                  << r.buildMs << " ms"
                  << " | RAM indice: " << humanBytes(r.bytes) << "\n";

    // ---------- 4) Benchmarks de patrones de acceso ----------
    // Probamos 4 patrones × cada backend, SIN prefetch (solo LRU), para ver
    // la línea base de cada estructura.
    std::cout << "\n--- Benchmarks de patrones de acceso (sin prefetch) ---\n";
    std::cout << std::left
              << std::setw(22) << "Backend"
              << std::setw(14) << "Patron"
              << std::setw(14) << "Tiempo (ms)"
              << std::setw(14) << "Hit ratio"
              << "Cargas\n";
    for (auto& b : backends) {
        for (Pattern p : {Pattern::Sequential, Pattern::Reverse,
                          Pattern::Random, Pattern::Strided}) {
            auto r = runPattern(*b, p, /*usePrefetch=*/false, cacheCap, depth);
            std::cout << std::left << std::setw(22) << b->name()
                      << std::setw(14) << patternName(p)
                      << std::setw(14) << std::fixed << std::setprecision(1) << r.totalMs
                      << std::setw(14) << std::setprecision(3) << r.hitRatio
                      << r.loads << "\n";
        }
        std::cout << "--------------------------------------------------------------\n";
    }

    // ---------- 5) Impacto del prefetching ----------
    // Comparamos distintas profundidades de prefetch (0=off, 2, 4, 8) usando
    // el backend vector y el patrón secuencial (donde el prefetch ayuda más).
    std::cout << "\n--- Impacto del prefetching (backend vector, patron secuencial) ---\n";
    std::cout << std::left
              << std::setw(14) << "Prefetch"
              << std::setw(14) << "Tiempo (ms)"
              << std::setw(14) << "Hit ratio"
              << std::setw(14) << "Cargas"
              << "Prefetches\n";
    for (size_t d : {size_t(0), size_t(2), size_t(4), size_t(8)}) {
        auto r = runPattern(*backends[0], Pattern::Sequential, d > 0, cacheCap, d);
        std::cout << std::left << std::setw(14) << (d == 0 ? "off" : std::to_string(d))
                  << std::setw(14) << std::fixed << std::setprecision(1) << r.totalMs
                  << std::setw(14) << std::setprecision(3) << r.hitRatio
                  << std::setw(14) << r.loads
                  << "-\n";
    }

    std::cout << "\n[OK] Simulacion completa.\n";
    return 0;
}