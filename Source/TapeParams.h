// Chènevières Tape — paramètres (partagés par le moteur, le plugin et les tests)
#pragma once
#include <array>
#include <cmath>

namespace tape
{
enum P : int
{
    pMachine = 0, // CH-80, CH-16, CH-440
    pSpeed,       // 7.5, 15, 30 ips
    pFormula,     // bande : Standard, +6, +9
    pEq,          // NAB, CCIR
    pInput,       // dB
    pBias,        // -100..+100 (sous/sur-polarisation)
    pOutput,      // dB
    pWow,         // %
    pFlutter,     // %
    pHiss,        // %
    pMix,         // %
    pAutoGain,
    pBypass,
    numParams
};

enum class Kind { Float, Bool, Choice };

struct ParamSpec
{
    const char* id;
    const char* name;
    Kind kind;
    float min, max, def;
    const char* choices; // séparés par '|'
    const char* unit;
};

inline const ParamSpec& spec (int p)
{
    static const std::array<ParamSpec, numParams> t {{
        { "machine",  "Machine",   Kind::Choice, 0, 2, 1, "CH-80|CH-16|CH-440", "" },
        { "speed",    "Speed",     Kind::Choice, 0, 2, 1, "7.5 ips|15 ips|30 ips", "" },
        { "formula",  "Tape",      Kind::Choice, 0, 2, 1, "Standard|+6 dB|+9 dB", "" },
        { "eq",       "EQ",        Kind::Choice, 0, 1, 0, "NAB|CCIR", "" },
        { "input",    "Input",     Kind::Float, -18.0f, 18.0f, 0.0f, nullptr, "dB" },
        { "bias",     "Bias",      Kind::Float, -100.0f, 100.0f, 0.0f, nullptr, "%" },
        { "output",   "Output",    Kind::Float, -18.0f, 18.0f, 0.0f, nullptr, "dB" },
        { "wow",      "Wow",       Kind::Float, 0.0f, 100.0f, 15.0f, nullptr, "%" },
        { "flutter",  "Flutter",   Kind::Float, 0.0f, 100.0f, 15.0f, nullptr, "%" },
        { "hiss",     "Hiss",      Kind::Float, 0.0f, 100.0f, 0.0f, nullptr, "%" },
        { "mix",      "Mix",       Kind::Float, 0.0f, 100.0f, 100.0f, nullptr, "%" },
        { "autogain", "Auto gain", Kind::Bool, 0, 1, 1, nullptr, "" },
        { "bypass",   "Bypass",    Kind::Bool, 0, 1, 0, nullptr, "" },
    }};
    return t[(size_t) p];
}

// Valeurs brutes de tous les paramètres (unités de ParamSpec)
struct Values
{
    std::array<float, numParams> v {};
    Values() { for (int i = 0; i < numParams; ++i) v[(size_t) i] = spec (i).def; }
    float operator[] (int p) const { return v[(size_t) p]; }
    float& operator[] (int p) { return v[(size_t) p]; }
    int choice (int p) const { return (int) std::lround (v[(size_t) p]); }
    bool flag (int p) const { return v[(size_t) p] >= 0.5f; }
};
} // namespace tape
