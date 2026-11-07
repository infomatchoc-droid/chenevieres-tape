// Rendu hors ligne à travers le plugin : TapeRender entree.wav sortie.wav [param=valeur ...]
#include "../Source/PluginProcessor.h"
#include <cstdio>
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3) { std::printf ("TapeRender entree.wav sortie.wav [param=valeur ...]\n"); return 1; }
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])));
    if (! rd) { std::printf ("lecture impossible\n"); return 1; }
    const int n = (int) rd->lengthInSamples, chs = 2; const double fs = rd->sampleRate;
    juce::AudioBuffer<float> in (chs, n); rd->read (&in, 0, n, 0, true, true);
    std::unique_ptr<TapeProcessor> p (static_cast<TapeProcessor*> (createPluginFilter()));
    p->setPlayConfigDetails (chs, chs, fs, 512);
    p->prepareToPlay (fs, 512);
    for (int i = 3; i < argc; ++i)
    {
        juce::String o (argv[i]); auto k = o.upToFirstOccurrenceOf ("=", false, false);
        const float v = o.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
        for (int q = 0; q < tape::numParams; ++q) if (k == tape::spec (q).id) p->setValue (q, v);
    }
    p->prepareToPlay (fs, 512);
    const int lat = p->getLatencySamples();
    juce::AudioBuffer<float> out (chs, n), blk (chs, 512); juce::MidiBuffer m;
    for (int pos = 0; pos < n + lat; pos += 512)
    {
        blk.clear();
        for (int c = 0; c < chs; ++c)
            for (int i = 0; i < 512; ++i) if (pos + i < n) blk.setSample (c, i, in.getSample (c, pos + i));
        p->processBlock (blk, m);
        for (int c = 0; c < chs; ++c)
            for (int i = 0; i < 512; ++i) { const int d = pos + i - lat; if (d >= 0 && d < n) out.setSample (c, d, blk.getSample (c, i)); }
    }
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2])); f.deleteFile();
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream().release());
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.release(), fs, (unsigned) chs, 24, {}, 0));
    w->writeFromAudioSampleBuffer (out, 0, n);
    std::printf ("%s ecrit (latence %d)\n", argv[2], lat);
}
