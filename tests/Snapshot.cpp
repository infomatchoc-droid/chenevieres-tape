// Capture de l'interface : TapeSnapshot sortie.png [scale=2] [param=valeur ...]
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_gui_basics/juce_gui_basics.h>

#if JUCE_LINUX
 #include <dlfcn.h>
 static int ignoreXError (void*, void*) { return 0; }
 static void tolerateXErrors()
 {
     if (auto* lib = dlopen ("libX11.so.6", RTLD_LAZY | RTLD_GLOBAL))
         if (auto fn = (void* (*) (int (*) (void*, void*))) dlsym (lib, "XSetErrorHandler"))
             fn (ignoreXError);
 }
#else
 static void tolerateXErrors() {}
#endif

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::Desktop::getInstance();
    tolerateXErrors();
    std::unique_ptr<TapeProcessor> p (static_cast<TapeProcessor*> (createPluginFilter()));
    const double fs = 48000.0;
    p->setPlayConfigDetails (2, 2, fs, 512);
    p->prepareToPlay (fs, 512);

    float scale = 2.0f, level = -16.0f;
    for (int i = 2; i < argc; ++i)
    {
        const juce::String o (argv[i]);
        const auto key = o.upToFirstOccurrenceOf ("=", false, false), val = o.fromFirstOccurrenceOf ("=", false, false);
        if (key == "scale") { scale = val.getFloatValue(); continue; }
        if (key == "level") { level = val.getFloatValue(); continue; }
        for (int k = 0; k < tape::numParams; ++k)
            if (key == tape::spec (k).id) p->setValue (k, val.getFloatValue());
    }

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    const float a = std::pow (10.0f, level / 20.0f);
    for (int b = 0; b < 100; ++b)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                buf.setSample (c, i, a * (c ? 0.8f : 1.0f) * std::sin (2.0f * 3.14159265f * 220.0f * (float) (b * 512 + i) / (float) fs));
        p->processBlock (buf, midi);
    }

    std::unique_ptr<TapeEditor> ed (static_cast<TapeEditor*> (p->createEditor()));
    ed->applyScale (1.0f);
    ed->setVisible (true);
    ed->getContent().settle (0.6f);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
    juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat().writeImageToStream (img, os);
    std::printf ("%s (%d x %d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    ed.reset();
    return 0;
}
