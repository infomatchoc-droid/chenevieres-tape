// Chènevières Tape — moteur d'émulation de bande
#include "TapeEngine.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace tape
{
namespace
{
constexpr double pi = 3.14159265358979323846;
inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
}

// ---------------------------------------------------------------- filtres
void Biquad::setLowPass (double fs, double f, double q)
{
    const double w = 2 * pi * std::min (f, 0.49 * fs) / fs, cw = std::cos (w), al = std::sin (w) / (2 * q);
    const double a0 = 1 + al;
    b0 = (1 - cw) / 2 / a0; b1 = (1 - cw) / a0; b2 = b0;
    a1 = -2 * cw / a0; a2 = (1 - al) / a0;
}

void Biquad::setHighPass (double fs, double f, double q)
{
    const double w = 2 * pi * f / fs, cw = std::cos (w), al = std::sin (w) / (2 * q);
    const double a0 = 1 + al;
    b0 = (1 + cw) / 2 / a0; b1 = -(1 + cw) / a0; b2 = b0;
    a1 = -2 * cw / a0; a2 = (1 - al) / a0;
}

void Biquad::setPeak (double fs, double f, double q, double gainDb)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w = 2 * pi * f / fs, cw = std::cos (w), al = std::sin (w) / (2 * q);
    const double a0 = 1 + al / A;
    b0 = (1 + al * A) / a0; b1 = -2 * cw / a0; b2 = (1 - al * A) / a0;
    a1 = -2 * cw / a0; a2 = (1 - al / A) / a0;
}

void Biquad::setFirstOrder (double fs, double n0, double n1, double d0, double d1)
{
    const double K = 2 * fs;
    const double a0 = d0 + d1 * K;
    b0 = (n0 + n1 * K) / a0; b1 = (n0 - n1 * K) / a0; b2 = 0;
    a1 = (d0 - d1 * K) / a0; a2 = 0;
}

// ---------------------------------------------------------------- hystérésis
void Hysteresis::set (double sat, double drive, double width)
{
    Ms = 0.5 + 1.5 * (1.0 - sat);
    a = Ms / (0.01 + 6.0 * drive);
    c = std::sqrt (std::max (0.0, 1.0 - width)) - 0.01;
    c = std::clamp (c, 0.01, 0.99);
}

namespace
{
inline double langevin (double x)
{
    if (std::abs (x) < 1e-3) return x / 3.0 - x * x * x / 45.0;
    return 1.0 / std::tanh (x) - 1.0 / x;
}
inline double langevinD (double x)
{
    if (std::abs (x) < 1e-3) return 1.0 / 3.0 - x * x / 15.0;
    const double s = std::sinh (x);
    return 1.0 / (x * x) - 1.0 / (s * s);
}
}

double Hysteresis::dMdt (double H, double dH, double Mv) const
{
    const double Q = (H + alpha * Mv) / a;
    const double Man = Ms * langevin (Q);
    const double dMan = Ms / a * langevinD (Q);
    const double delta = dH >= 0 ? 1.0 : -1.0;
    const double dM = Man - Mv;
    const double deltaM = ((delta > 0) == (dM > 0)) ? 1.0 : 0.0;
    double den1 = (1 - c) * delta * k - alpha * dM;
    if (std::abs (den1) < 1e-9) den1 = den1 < 0 ? -1e-9 : 1e-9;
    const double irr = (1 - c) * deltaM * dM / den1;
    double den = 1 - c * alpha * dMan;
    if (std::abs (den) < 1e-9) den = 1e-9;
    return dH * (irr + c * dMan) / den;
}

double Hysteresis::process (double H, double T)
{
    const double dH = (H - Hprev) / T;
    const double k1 = T * dMdt (Hprev, dHprev, M);
    const double k2 = T * dMdt (0.5 * (H + Hprev), 0.5 * (dH + dHprev), M + 0.5 * k1);
    double Mn = M + k2;
    if (! std::isfinite (Mn) || std::abs (Mn) > 4 * Ms) Mn = std::clamp (std::isfinite (Mn) ? Mn : 0.0, -Ms, Ms);
    M = Mn;
    Hprev = H;
    dHprev = dH;
    return M;
}

double Engine::measureGain (Hysteresis h, double amp)
{
    // sinus à 100 Hz échantillonné à 96 kHz (le modèle ne dépend pas de la fréquence) :
    // deux périodes de mise en route, puis deux de mesure
    h.reset();
    const double fsm = 96000.0, T = 1.0 / fsm, f = 100.0;
    const int per = (int) (fsm / f);
    double num = 0, den = 0;
    for (int i = 0; i < 4 * per; ++i)
    {
        const double x = amp * std::sin (2 * pi * f * i * T);
        const double y = h.process (x, T);
        if (i >= 2 * per) { num += y * x; den += x * x; }
    }
    return den > 0 && num > 0 ? num / den : 1.0;
}

// ---------------------------------------------------------------- machines
const Machine& machine (int m)
{
    //                       sat   width drive  bump  hf    asym   wow  flut  hiss
    static const Machine t[3] = {
        { "CH-80",  0.40, 0.30, 0.40, 1.8, 1.15, 0.000, 0.6, 0.6, -1.0 },  // précise, propre
        { "CH-16",  0.50, 0.45, 0.50, 3.2, 1.00, 0.012, 1.0, 1.0,  0.0 },  // gros multipiste 2" : bas gras, punch, colle
        { "CH-440", 0.60, 0.60, 0.55, 3.0, 0.62, 0.028, 1.8, 1.6,  2.0 },  // vintage : chaude, douce
    };
    return t[std::clamp (m, 0, 2)];
}

// ---------------------------------------------------------------- moteur
void Engine::prepare (double sampleRate, double baseRate, int maxChannels)
{
    fs = sampleRate;
    fsBase = baseRate;
    maxCh = std::clamp (maxChannels, 1, 2);
    smooth = 1.0 - std::exp (-1.0 / (0.02 * fs)); // 20 ms

    // retard fixe de la ligne de pleurage : 3 ms, arrondi à un nombre entier d'échantillons du DAW
    latencyBase = (int) std::lround (0.003 * fsBase);
    const int ratio = (int) std::lround (fs / fsBase);
    baseDelay = (double) latencyBase * ratio;
    int size = 1;
    while (size < (int) (2 * baseDelay) + 16) size <<= 1;
    delayMask = size - 1;
    dryDelay = (int) baseDelay;
    for (int c = 0; c < 2; ++c)
    {
        delayBuf[c].assign ((size_t) size, 0.0f);
        dryBuf[c].assign ((size_t) size, 0.0f);
    }
    dirty = true;
    reset();
}

void Engine::reset()
{
    for (auto& h : hyst) h.reset();
    for (auto* b : { &preHf, &preLf, &deHf, &deLf, &bump, &dip, &hp, &lp, &dcBlock, &hissTilt, &hfSense }) b->reset();
    for (int c = 0; c < 2; ++c) compEnv[c] = hfEnv[c] = dynLp[c] = gainRnd[c] = azRnd[c] = 0;
    gainCommon = scrLo = scrHi = 0;
    for (int c = 0; c < 2; ++c)
    {
        std::fill (delayBuf[c].begin(), delayBuf[c].end(), 0.0f);
        std::fill (dryBuf[c].begin(), dryBuf[c].end(), 0.0f);
    }
    writePos = 0;
    update();
    curIn = tgtIn; curOut = tgtOut; curMix = tgtMix;
}

void Engine::update()
{
    dirty = false;
    const Machine& m = machine (vals.choice (pMachine));
    const int speed = vals.choice (pSpeed);          // 0 = 7.5, 1 = 15, 2 = 30 ips
    const int formula = vals.choice (pFormula);
    const bool nab = vals.choice (pEq) == 0;
    const double bias = std::clamp (vals[pBias] / 100.0, -1.0, 1.0);
    const double ips = speed == 0 ? 7.5 : speed == 1 ? 15.0 : 30.0;

    // --- hystérésis : la polarisation (bias) resserre le cycle, la sous-polarisation l'élargit
    const double width = std::clamp (m.width - 0.28 * bias, 0.05, 0.95);
    const double drive = std::clamp (m.drive * std::pow (2.0, -0.35 * bias), 0.05, 1.0);
    for (auto& h : hyst) h.set (m.sat, drive, width);

    // niveau sur la bande : 0 VU = -18 dBFS ; une bande « +6 » encaisse 6 dB de plus
    static const double headroom[3] = { 1.0, 0.5, 0.355 };
    hScale = 5.5 * headroom[std::clamp (formula, 0, 2)];
    asym = 4.5 * m.asym;
    const double g = measureGain (hyst[0], hScale * dbToGain (-18.0));
    outNorm = 1.0 / (hScale * g);

    // --- préaccentuation / désaccentuation (constantes de temps normalisées)
    double tauHf = 50e-6, tauLf = 0;
    if (speed == 0) tauHf = nab ? 50e-6 : 70e-6;
    else if (speed == 1) tauHf = nab ? 50e-6 : 35e-6;
    else tauHf = 17.5e-6;
    if (nab && speed < 2) tauLf = 3180e-6;
    const double A = 1.6;   // accentuation des aigus limitée (+4 dB) : au-delà, l'aigu saturé se replie en intermodulation dure ; la saturation des aigus passe par l'auto-effacement
    preHf.setFirstOrder (fs, 1, tauHf, 1, tauHf / A);
    deHf.setFirstOrder (fs, 1, tauHf / A, 1, tauHf);
    if (tauLf > 0)
    {
        const double B = 2.5; // graves enregistrés moins fort (NAB), remontés à la lecture
        preLf.setFirstOrder (fs, 1.0 / B, tauLf, 1, tauLf);
        deLf.setFirstOrder (fs, 1, tauLf, 1.0 / B, tauLf);
    }
    else
    {
        preLf.setIdentity();
        deLf.setIdentity();
    }

    // --- tête de lecture
    const double fBump = speed == 0 ? 32.0 : speed == 1 ? 52.0 : 88.0;
    bump.setPeak (fs, fBump, 1.1, m.bumpDb);
    dip.setPeak (fs, fBump * 2.3, 1.6, -0.45 * m.bumpDb);
    hp.setHighPass (fs, speed == 0 ? 20.0 : speed == 1 ? 16.0 : 24.0, 0.62);
    double fHf = (speed == 0 ? 11500.0 : speed == 1 ? 19000.0 : 26000.0) * m.hfMult * std::pow (2.0, -0.55 * bias);
    fHf = std::min (fHf, 0.45 * fs);
    lp.setLowPass (fs, fHf, 0.62);
    dcBlock.setFirstOrder (fs, 0, 1, 2 * pi * 5.0, 1); // passe-haut 5 Hz (asymétrie)

    // --- souffle : bruit sur la bande, mis en forme par la désaccentuation
    hissTilt.setHighPass (fs, 150.0, 0.5);
    static const double formulaHiss[3] = { 3.0, 0.0, -3.0 };
    const double speedHiss = speed == 0 ? 4.0 : speed == 1 ? 0.0 : -3.0;
    const double hissPct = std::clamp (vals[pHiss] / 100.0, 0.0, 1.0);
    // à 100 % : environ -46 dBFS RMS (CH-16, 15 ips, bande +6)
    hissGain = hissPct <= 0 ? 0.0
             : dbToGain (-36.0 + 30.0 * std::log10 (hissPct) + m.hissDb + formulaHiss[std::clamp (formula, 0, 2)] + speedHiss);

    // --- compression de bande : la bande « mange » les crêtes, d'autant plus qu'elles sont aiguës
    //     (elles arrivent préaccentuées). Seuil lié à la marge de la bande, attaque 0,3 ms, retour 70 ms.
    const int mi = std::clamp (vals.choice (pMachine), 0, 2);
    static const double compAmount[3] = { 0.55, 1.0, 1.2 }, eraseAmount[3] = { 0.7, 1.0, 1.25 };
    compThr = 1.25 / hScale * std::pow (2.0, 0.3 * bias);
    compExp = 0.5 * 0.42 * compAmount[mi];          // ≈ taux 1,7:1 au-dessus du seuil (genou très doux)
    compAtk = 1.0 - std::exp (-1.0 / (0.0003 * fs));
    compRel = 1.0 - std::exp (-1.0 / (0.070 * fs));
    // auto-effacement : les aigus forts s'effacent eux-mêmes sur la bande (passe-bas qui se ferme)
    hfSense.setHighPass (fs, 5000.0, 0.7);
    erasure = 1.6 * eraseAmount[mi] * (hScale / 2.75) * std::pow (2.0, -0.5 * bias) * (speed == 0 ? 1.3 : speed == 1 ? 1.0 : 0.6);
    eraseAtk = 1.0 - std::exp (-1.0 / (0.0002 * fs));
    eraseRel = 1.0 - std::exp (-1.0 / (0.030 * fs));
    fErase = std::min (0.45 * fs, 24000.0);

    // --- pleurage / scintillement (en échantillons de retard crête)
    const double wowPct = vals[pWow] / 100.0, flPct = vals[pFlutter] / 100.0;
    wowRate = 0.55 * ips / 15.0 + 0.25;
    flutterRate = 7.0 * ips / 15.0;
    const double wowDev = 0.004 * wowPct * m.wowMult * std::sqrt (15.0 / ips);   // écart de vitesse crête (0,12 % à 30 %)
    const double flDev = 0.0025 * flPct * m.flutterMult;
    wowDepth = std::min (wowDev / (2 * pi * wowRate) * fs, 0.92 * baseDelay);
    flutterDepth = std::min (flDev / (2 * pi * flutterRate) * fs, 0.05 * baseDelay);
    // vie de la bande : contact tête/bande (niveau ±0,2 dB à 100 %), azimut qui respire entre canaux,
    // scrape flutter (frottement, 1–3 kHz) et bruit de modulation (grain qui suit le signal)
    const double life = 0.5 * (wowPct + flPct);
    wobble = 0.023 * life * m.wowMult;
    azDepth = (0.08 + 0.5 * flPct) * m.flutterMult * fs / 96000.0;           // échantillons (quelques µs)
    scrapeDepth = 0.0009 * flPct * m.flutterMult / (2 * pi * 1800.0) * fs;
    modNoise = (0.0012 + 0.006 * std::clamp (vals[pHiss] / 100.0, 0.0, 1.0)) * dbToGain (m.hissDb);

    // --- gains
    tgtIn = dbToGain (vals[pInput]);
    tgtOut = dbToGain (vals[pOutput]) * (vals.flag (pAutoGain) ? 1.0 / tgtIn : 1.0);
    tgtMix = vals.flag (pBypass) ? 0.0 : std::clamp (vals[pMix] / 100.0, 0.0, 1.0);
}

double Engine::nextNoise()
{
    // somme de deux uniformes : bruit à peu près gaussien, sans appel de bibliothèque
    seed = seed * 1664525u + 1013904223u;
    const double u1 = (double) (seed >> 8) / 16777216.0;
    seed = seed * 1664525u + 1013904223u;
    const double u2 = (double) (seed >> 8) / 16777216.0;
    return (u1 + u2 - 1.0) * 2.449; // variance ≈ 1
}

void Engine::process (float* const* ch, int numCh, int n)
{
    if (dirty) update();
    numCh = std::min (numCh, maxCh);
    const double T = 1.0 / fs;
    const double rndA = 1.0 - std::exp (-2 * pi * 0.4 / fs), rndB = 1.0 - std::exp (-2 * pi * 25.0 / fs);
    const double scrA = 1.0 - std::exp (-2 * pi * 3000.0 / fs), scrB = 1.0 - std::exp (-2 * pi * 800.0 / fs);
    const double rndC = 1.0 - std::exp (-2 * pi * 0.5 / fs), rndG = 1.0 - std::exp (-2 * pi * 3.0 / fs);
    const double rndAz = 1.0 - std::exp (-2 * pi * 1.5 / fs);
    // un bruit blanc filtré par un passe-bas à un pôle de coefficient a a pour écart-type sqrt (a / (2 - a)) : on ramène à 1
    const auto unit = [] (double a) { return std::sqrt ((2.0 - a) / a); };
    const double nA = unit (rndA), nB = unit (rndB) * 1.4, nC = unit (rndC), nG = unit (rndG), nAz = unit (rndAz);
    double sumIn[2] {}, sumOut[2] {};

    for (int i = 0; i < n; ++i)
    {
        curIn += (tgtIn - curIn) * smooth;
        curOut += (tgtOut - curOut) * smooth;
        curMix += (tgtMix - curMix) * smooth;
        curWow += (wowDepth - curWow) * smooth;
        curFl += (flutterDepth - curFl) * smooth;
        curHiss += (hissGain - curHiss) * smooth;

        // modulation de vitesse commune (même bande pour tous les canaux)
        phW1 += wowRate * T; phW2 += wowRate * 1.63 * T;
        phF1 += flutterRate * T; phF2 += flutterRate * 1.51 * T;
        if (phW1 >= 1) phW1 -= 1;
        if (phW2 >= 1) phW2 -= 1;
        if (phF1 >= 1) phF1 -= 1;
        if (phF2 >= 1) phF2 -= 1;
        rndWow += (nextNoise() - rndWow) * rndA;
        rndFl += (nextNoise() - rndFl) * rndB;
        rndFl2 += (rndFl - rndFl2) * rndB;
        const double wow = 0.55 * std::sin (2 * pi * phW1) + 0.3 * std::sin (2 * pi * phW2) + 0.6 * rndWow * nA;
        const double fl = 0.5 * std::sin (2 * pi * phF1) + 0.25 * std::sin (2 * pi * phF2) + 0.8 * rndFl2 * nB;
        // scrape flutter : bruit 0,8–3 kHz
        const double nz = nextNoise();
        scrLo += (nz - scrLo) * scrA; scrHi += (nz - scrHi) * scrB;
        const double scrape = (scrLo - scrHi) * 2.0;
        double mod = std::clamp (curWow * wow + curFl * fl + scrapeDepth * scrape, -0.9 * baseDelay, 0.9 * baseDelay);
        gainCommon += (nextNoise() - gainCommon) * rndC;

        for (int c = 0; c < numCh; ++c)
        {
            const double dry = ch[c][i];
            double x = dry * curIn;
            sumIn[c] += x * x;

            // enregistrement
            x = preLf.process (preHf.process (x, c), c);

            // compression de bande sur les crêtes (genou doux)
            {
                const double a = std::abs (x);
                compEnv[c] += (a - compEnv[c]) * (a > compEnv[c] ? compAtk : compRel);
                const double r = compEnv[c] / compThr;
                if (r > 0.05) x *= std::pow (1.0 + r * r * r * r, -0.25 * compExp);
            }
            // asymétrie de la machine (harmoniques paires), nulle au repos : pas de bruit de fond à l'arrêt
            const double xa = x + asym * x * x / (1.0 + 0.1 * x * x);
            double y = hyst[c].process (xa * hScale, T) * outNorm;
            if (asym != 0) y = dcBlock.process (y, c);

            // auto-effacement des aigus : passe-bas qui se ferme quand l'aigu enregistré est fort
            {
                const double h = std::abs (hfSense.process (y, c));
                hfEnv[c] += (h - hfEnv[c]) * (h > hfEnv[c] ? eraseAtk : eraseRel);
                const double e = erasure * hfEnv[c];
                const double fc = fErase / (1.0 + e * e);
                const double w = 2 * pi * fc / fs;
                dynLp[c] += (y - dynLp[c]) * (w / (1.0 + w));
                y = dynLp[c];
            }

            // souffle et bruit de modulation (le grain de la bande suit le signal)
            {
                const double nzc = hissTilt.process (nextNoise(), c);
                y += nzc * (curHiss + modNoise * std::abs (y));
            }

            // lecture
            y = deHf.process (deLf.process (y, c), c);
            y = lp.process (hp.process (dip.process (bump.process (y, c), c), c), c);

            // fluctuation de contact tête/bande
            gainRnd[c] += (nextNoise() - gainRnd[c]) * rndG;
            y *= 1.0 + wobble * (0.7 * gainCommon * nC + 0.5 * gainRnd[c] * nG);

            // pleurage : écriture puis lecture interpolée (Hermite), azimut propre à chaque canal
            azRnd[c] += (nextNoise() - azRnd[c]) * rndAz;
            const double readPos = (double) writePos - (baseDelay + mod + azDepth * azRnd[c] * nAz);
            delayBuf[c][(size_t) (writePos & delayMask)] = (float) y;
            const double fl0 = std::floor (readPos);
            const double t = readPos - fl0;
            const int p0 = (int) fl0;
            const auto& b = delayBuf[c];
            const double xm1 = b[(size_t) ((p0 - 1) & delayMask)], x0 = b[(size_t) (p0 & delayMask)];
            const double x1 = b[(size_t) ((p0 + 1) & delayMask)], x2 = b[(size_t) ((p0 + 2) & delayMask)];
            const double c1 = 0.5 * (x1 - xm1), c2 = xm1 - 2.5 * x0 + 2 * x1 - 0.5 * x2, c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
            const double wet = ((c3 * t + c2) * t + c1) * t + x0;

            // direct retardé d'autant (mix parallèle aligné)
            dryBuf[c][(size_t) (writePos & delayMask)] = (float) dry;
            const double dryD = dryBuf[c][(size_t) ((writePos - dryDelay) & delayMask)];

            double out = wet * curOut * curMix + dryD * (1.0 - curMix);
            if (! std::isfinite (out)) { out = 0; hyst[c].reset(); }
            sumOut[c] += out * out;
            ch[c][i] = (float) out;
        }
        if (++writePos >= (1 << 30)) writePos -= (1 << 30); // la taille du tampon divise 2^30
    }

    for (int c = 0; c < 2; ++c)
    {
        const int src = std::min (c, numCh - 1);
        meterIn[c].store ((float) std::sqrt ((src == c ? sumIn[c] : sumIn[src]) / std::max (1, n)));
        meterOut[c].store ((float) std::sqrt ((src == c ? sumOut[c] : sumOut[src]) / std::max (1, n)));
    }
}
} // namespace tape
