// Tests du moteur (ctest) : pas de NaN, niveau unitaire à 0 VU, mix 0 % = direct retardé, silence
#include "../Source/TapeEngine.h"
#include <cmath>
#include <cstdio>
#include <vector>
using namespace tape;
static int fails = 0;
#define CHECK(c, ...) do { if (! (c)) { ++fails; std::printf ("ECHEC : " __VA_ARGS__); std::printf ("\n"); } } while (0)
static const double pi = 3.14159265358979323846;

static void render (Engine& e, std::vector<float>& L, std::vector<float>& R)
{
    for (size_t i = 0; i < L.size(); i += 256)
    {
        float* p[2] = { L.data() + i, R.data() + i };
        e.process (p, 2, (int) std::min<size_t> (256, L.size() - i));
    }
}

int main()
{
    const double fs = 96000, base = 48000;
    // 1. toutes les combinaisons, signal fort : pas de NaN, pas d'explosion
    for (int m = 0; m < 3; ++m)
        for (int s = 0; s < 3; ++s)
            for (int f = 0; f < 3; ++f)
                for (int q = 0; q < 2; ++q)
                    for (float b : { -100.0f, 0.0f, 100.0f })
                    {
                        Values v; v[pMachine] = (float) m; v[pSpeed] = (float) s; v[pFormula] = (float) f; v[pEq] = (float) q;
                        v[pBias] = b; v[pInput] = 18; v[pWow] = 100; v[pFlutter] = 100; v[pHiss] = 100; v[pAutoGain] = 0;
                        Engine e; e.setParams (v); e.prepare (fs, base, 2);
                        std::vector<float> L (9600), R (9600);
                        for (size_t i = 0; i < L.size(); ++i) { L[i] = (float) std::sin (2 * pi * 60 * i / fs); R[i] = (float) ((i / 37) % 2 ? 1 : -1); }
                        render (e, L, R);
                        bool ok = true; float mx = 0;
                        for (size_t i = 0; i < L.size(); ++i) { ok &= std::isfinite (L[i]) && std::isfinite (R[i]); mx = std::max (mx, std::max (std::abs (L[i]), std::abs (R[i]))); }
                        CHECK (ok && mx < 30, "machine %d vitesse %d bande %d eq %d bias %.0f : max %.2f", m, s, f, q, b, mx);
                    }

    // 2. 1 kHz à -18 dBFS (0 VU) : gain proche de 0 dB pour chaque machine
    for (int m = 0; m < 3; ++m)
    {
        Values v; v[pMachine] = (float) m; v[pWow] = 0; v[pFlutter] = 0;
        Engine e; e.setParams (v); e.prepare (fs, base, 2);
        std::vector<float> L (48000), R (48000);
        const double a = std::pow (10, -18 / 20.0);
        for (size_t i = 0; i < L.size(); ++i) L[i] = R[i] = (float) (a * std::sin (2 * pi * 1000 * i / fs));
        render (e, L, R);
        double s = 0; for (size_t i = 24000; i < L.size(); ++i) s += L[i] * L[i];
        const double db = 10 * std::log10 (s / 24000) - 20 * std::log10 (a / std::sqrt (2.0));
        CHECK (std::abs (db) < 1.0, "machine %d : gain a 0 VU %.2f dB", m, db);
    }

    // 3. mix 0 % : direct retardé, à l'identique
    {
        Values v; v[pMix] = 0;
        Engine e; e.setParams (v); e.prepare (fs, base, 2);
        const int lat = e.latencyBaseSamples() * 2;
        std::vector<float> L (20000), R (20000), ref (20000);
        for (size_t i = 0; i < L.size(); ++i) ref[i] = L[i] = R[i] = (float) std::sin (0.01 * i) * 0.5f;
        render (e, L, R);
        double err = 0; for (size_t i = (size_t) lat; i < L.size(); ++i) err = std::max (err, (double) std::abs (L[i] - ref[i - (size_t) lat]));
        CHECK (err < 1e-6, "mix 0 %% : ecart %.2g", err);
    }

    // 4. silence sans souffle : silence
    {
        Values v; v[pHiss] = 0; v[pMachine] = 2;
        Engine e; e.setParams (v); e.prepare (fs, base, 2);
        std::vector<float> L (20000, 0.0f), R (20000, 0.0f);
        render (e, L, R);
        float mx = 0; for (float x : L) mx = std::max (mx, std::abs (x));
        CHECK (mx < 1e-5, "silence : %.2g", mx);
    }

    std::printf (fails ? "%d echec(s)\n" : "OK\n", fails);
    return fails ? 1 : 0;
}
