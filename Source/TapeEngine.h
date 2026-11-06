// Chènevières Tape — moteur d'émulation de bande (C++ pur, sans JUCE : testable seul)
//
// Chaîne, par canal, à la fréquence suréchantillonnée :
//   gain d'entrée → préaccentuation d'enregistrement (NAB / CCIR / AES selon la vitesse)
//   → hystérésis magnétique (modèle de Jiles-Atherton, résolu en RK2) → souffle de bande
//   → désaccentuation de lecture → tête de lecture (head bump, pertes HF, coupe-bas)
//   → pleurage et scintillement (ligne à retard modulée, commune aux deux canaux)
//   → mix parallèle (direct retardé d'autant), gain de sortie.
#pragma once
#include "TapeParams.h"
#include <atomic>
#include <cstdint>
#include <vector>

namespace tape
{
// Filtre biquad en double (les filtres très graves à 192 kHz l'exigent)
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    double z1[2] {}, z2[2] {};
    void reset() { z1[0] = z1[1] = z2[0] = z2[1] = 0; }
    double process (double x, int c)
    {
        const double y = b0 * x + z1[c];
        z1[c] = b1 * x - a1 * y + z2[c];
        z2[c] = b2 * x - a2 * y;
        return y;
    }
    void setIdentity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    void setLowPass (double fs, double f, double q);
    void setHighPass (double fs, double f, double q);
    void setPeak (double fs, double f, double q, double gainDb);
    // Premier ordre analogique (n0 + n1·s) / (d0 + d1·s), transformée bilinéaire
    void setFirstOrder (double fs, double n0, double n1, double d0, double d1);
};

// Hystérésis de Jiles-Atherton (valeurs normalisées)
struct Hysteresis
{
    double Ms = 1, a = 1, alpha = 1.6e-3, k = 0.47875, c = 0.5;
    double M = 0, Hprev = 0, dHprev = 0;
    void reset() { M = Hprev = dHprev = 0; }
    void set (double sat, double drive, double width);
    double process (double H, double T); // T = période d'échantillonnage
private:
    double dMdt (double H, double dH, double Mv) const;
};

struct Machine
{
    const char* name;
    double sat, width, drive;   // hystérésis
    double bumpDb, hfMult;      // tête de lecture
    double asym;                // harmoniques paires
    double wowMult, flutterMult, hissDb;
};
const Machine& machine (int m);

class Engine
{
public:
    // sampleRate = fréquence de travail (suréchantillonnée), baseRate = celle du DAW
    void prepare (double sampleRate, double baseRate, int maxChannels);
    void reset();
    void setParams (const Values& v) { vals = v; dirty = true; }

    void process (float* const* ch, int numCh, int n);

    // Retard fixe ajouté par la ligne de pleurage (échantillons à la fréquence du DAW)
    int latencyBaseSamples() const { return latencyBase; }

    // Mesures du dernier bloc (RMS linéaire) pour les vu-mètres : niveau envoyé sur la bande / sortie
    float tapeLevel (int c) const { return meterIn[c & 1].load(); }
    float outLevel (int c) const { return meterOut[c & 1].load(); }

    // Gain de l'hystérésis pour un sinus d'amplitude amp (sert à normaliser au niveau 0 VU)
    static double measureGain (Hysteresis h, double amp);

private:
    void update();
    double nextNoise();

    Values vals;
    bool dirty = true;
    double fs = 192000.0, fsBase = 48000.0;
    int maxCh = 2;

    Hysteresis hyst[2];
    Biquad preHf, preLf, deHf, deLf, bump, dip, hp, lp, dcBlock, hissTilt;
    double hScale = 1, outNorm = 1, asym = 0, hissGain = 0;

    // pleurage / scintillement
    std::vector<float> delayBuf[2];
    int delayMask = 0, writePos = 0;
    double baseDelay = 0;         // échantillons (fréquence de travail)
    double wowDepth = 0, flutterDepth = 0, wowRate = 0.6, flutterRate = 7.5;
    double phW1 = 0, phW2 = 0.3, phF1 = 0, phF2 = 0.7, rndWow = 0, rndFl = 0, rndFl2 = 0;
    int latencyBase = 0;
    std::vector<float> dryBuf[2];
    int dryDelay = 0;

    uint32_t seed = 0x1234567u;

    // lissages
    double curIn = 1, curOut = 1, curMix = 1, curWow = 0, curFl = 0, curHiss = 0;
    double tgtIn = 1, tgtOut = 1, tgtMix = 1;
    double smooth = 0.001;

    std::atomic<float> meterIn[2] {}, meterOut[2] {};
};
} // namespace tape
