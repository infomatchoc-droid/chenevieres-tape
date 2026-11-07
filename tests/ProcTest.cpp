#include "../Source/PluginProcessor.h"
#include <cstdio>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    std::unique_ptr<TapeProcessor> p (static_cast<TapeProcessor*> (createPluginFilter()));
    const double fs = getenv ("SR") ? atof (getenv ("SR")) : 48000; p->setPlayConfigDetails (2, 2, fs, 512); p->prepareToPlay (fs, 512);
    float level = -12, f = 1000;
    for (int i = 1; i < argc; ++i) { juce::String o (argv[i]); auto k = o.upToFirstOccurrenceOf ("=", false, false); float v = o.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
        if (k == "level") level = v; else if (k == "f") f = v; else for (int q = 0; q < tape::numParams; ++q) if (k == tape::spec (q).id) p->setValue (q, v); }
    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer m; std::vector<float> out;
    const float a = std::pow (10.f, level / 20.f);
    for (int b = 0; b < 200; ++b) { for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) buf.setSample (c, i, a * std::sin (2 * 3.14159265f * f * (float) (b * 512 + i) / (float) fs));
        p->processBlock (buf, m); if (b >= 100) for (int i = 0; i < 512; ++i) out.push_back (buf.getSample (0, i)); }
    auto amp = [&] (double fr) { double re = 0, im = 0; for (size_t i = 0; i < out.size(); ++i) { re += out[i] * std::cos (2 * M_PI * fr * i / fs); im += out[i] * std::sin (2 * M_PI * fr * i / fs); } return 2 * std::sqrt (re * re + im * im) / out.size(); };
    double f1 = amp (f), h = 0; for (int k = 2; k <= 9 && k * f < fs / 2; ++k) h += amp (k * f) * amp (k * f);
    std::printf ("in %.1f dBFS -> out %.2f dB, THD %.2f %%\n", level, 20 * std::log10 (f1 / a), 100 * std::sqrt (h) / f1);
}
