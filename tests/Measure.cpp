// Mesures du moteur : THD selon le niveau, réponse en fréquence, souffle
// Measure [machine] [speed] [formula] [eq] [bias]
#include "../Source/TapeEngine.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>
using namespace tape;
static const double pi = 3.14159265358979323846;

static Values base (int argc, char** argv)
{
    Values v;
    v[pWow] = 0; v[pFlutter] = 0; v[pHiss] = 0;
    if (argc > 1) v[pMachine] = (float) atof (argv[1]);
    if (argc > 2) v[pSpeed] = (float) atof (argv[2]);
    if (argc > 3) v[pFormula] = (float) atof (argv[3]);
    if (argc > 4) v[pEq] = (float) atof (argv[4]);
    if (argc > 5) v[pBias] = (float) atof (argv[5]);
    return v;
}

static double gFs = 192000;
static std::vector<float> run (const Values& v, const std::vector<float>& in, double fs = gFs)
{
    Engine e; e.setParams (v); e.prepare (fs, fs / 4, 1);
    std::vector<float> buf = in;
    for (size_t i = 0; i < buf.size(); i += 512)
    {
        float* p = buf.data() + i;
        e.process (&p, 1, (int) std::min<size_t> (512, buf.size() - i));
    }
    return buf;
}

static double amp (const std::vector<float>& y, double f, double fs, size_t from)
{
    double re = 0, im = 0; size_t n = 0;
    for (size_t i = from; i < y.size(); ++i, ++n) { re += y[i] * std::cos (2 * pi * f * i / fs); im += y[i] * std::sin (2 * pi * f * i / fs); }
    return 2 * std::sqrt (re * re + im * im) / (double) n;
}

int main (int argc, char** argv)
{
    const double fs = getenv ("FS") ? atof (getenv ("FS")) : 192000;
    gFs = fs;
    Values v = base (argc, argv);
    const size_t N = (size_t) (fs * 0.5), from = (size_t) (fs * 0.25);
    std::printf ("THD a 1 kHz (niveau d'entree dBFS : gain dB, H2 %%, H3 %%, THD %%)\n");
    for (double lv = -36; lv <= 6.01; lv += 3)
    {
        std::vector<float> x (N);
        const double a = std::pow (10, lv / 20);
        // 1 kHz arrondi pour tomber juste sur la fenêtre
        for (size_t i = 0; i < N; ++i) x[i] = (float) (a * std::sin (2 * pi * 1000 * i / fs));
        auto y = run (v, x);
        const double f1 = amp (y, 1000, fs, from);
        double h[6] {};
        double thd = 0;
        for (int k = 2; k <= 7; ++k) { h[k - 2] = amp (y, 1000 * k, fs, from); thd += h[k - 2] * h[k - 2]; }
        std::printf ("%6.1f : %6.2f  %6.3f  %6.3f  %6.3f\n", lv, 20 * std::log10 (f1 / a), 100 * h[0] / f1, 100 * h[1] / f1, 100 * std::sqrt (thd) / f1);
    }
    std::printf ("Reponse a -30 dBFS (Hz : dB)\n");
    for (double f : { 20.0, 30.0, 40.0, 50.0, 60.0, 80.0, 100.0, 150.0, 200.0, 500.0, 1000.0, 3000.0, 6000.0, 10000.0, 15000.0, 20000.0 })
    {
        std::vector<float> x (N);
        const double a = std::pow (10, -30 / 20.0);
        for (size_t i = 0; i < N; ++i) x[i] = (float) (a * std::sin (2 * pi * f * i / fs));
        auto y = run (v, x);
        std::printf ("%7.0f : %6.2f\n", f, 20 * std::log10 (amp (y, f, fs, from) / a));
    }
    std::printf ("Reponse a -6 dBFS (Hz : dB)\n");
    for (double f : { 50.0, 100.0, 1000.0, 5000.0, 10000.0, 15000.0 })
    {
        std::vector<float> x (N);
        const double a = std::pow (10, -6 / 20.0);
        for (size_t i = 0; i < N; ++i) x[i] = (float) (a * std::sin (2 * pi * f * i / fs));
        auto y = run (v, x);
        std::printf ("%7.0f : %6.2f\n", f, 20 * std::log10 (amp (y, f, fs, from) / a));
    }
    {
        Values h = v; h[pHiss] = 100;
        std::vector<float> x (N, 0.0f);
        auto y = run (h, x);
        double s = 0; for (size_t i = from; i < N; ++i) s += y[i] * y[i];
        std::printf ("Souffle 100%% : %.1f dBFS RMS\n", 10 * std::log10 (s / (N - from)));
    }
    return 0;
}
