// Chènevières Tape — interface
#pragma once
#include "PluginProcessor.h"

// Bouton rotatif relié à un paramètre (glisser vertical, double-clic = valeur d'usine)
class Knob : public juce::Slider
{
public:
    Knob (TapeProcessor& p, int param, juce::String title, float radius);
    juce::String valueText() const;
    void paint (juce::Graphics&) override;
    void mouseEnter (const juce::MouseEvent& e) override { hover = true; juce::Slider::mouseEnter (e); repaint(); }
    void mouseExit (const juce::MouseEvent& e) override { hover = false; juce::Slider::mouseExit (e); repaint(); }
    const juce::String title;
    const float radius;
    const int param;
private:
    TapeProcessor& proc;
    juce::SliderParameterAttachment attachment;
    bool hover = false;
};

// Rangée de boutons lumineux pour un paramètre à choix (un seul allumé) ou un interrupteur
class Selector : public juce::Component
{
public:
    Selector (TapeProcessor& p, int param, juce::StringArray labels, juce::StringArray captions = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    bool toggle = false; // un seul bouton : interrupteur marche/arrêt
private:
    TapeProcessor& proc;
    int param;
    juce::StringArray labels, captions;
};

class EditorContent : public juce::Component, private juce::Timer
{
public:
    static constexpr int W = 1120, H = 560, barH = 66;
    EditorContent (TapeProcessor&, std::function<void (float)> setScale);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    // pour la capture d'écran : aiguilles posées sur la mesure, bobines tournées
    void settle (float reelTurn);

private:
    void timerCallback() override;
    void paintBar (juce::Graphics&);
    void paintTransport (juce::Graphics&, juce::Rectangle<float>);
    void paintReel (juce::Graphics&, juce::Point<float> c, float r, float packR, float angle);
    void paintVu (juce::Graphics&, juce::Rectangle<float>, float vuDb, const juce::String& ch);
    void paintSection (juce::Graphics&, juce::Rectangle<float>, const juce::String&);

    TapeProcessor& proc;
    std::function<void (float)> setScale;
    juce::Image wordmark, beagle;
    Knob input, bias, output, wow, flutter, hiss, mix;
    Selector machine, speed, formula, eq, autoGain, bypass;
    juce::Rectangle<int> sizeButton;
    float vu[2] { -30.0f, -30.0f };
    float reelAngle = 0, counter = 0;
    double lastTime = 0;
};

class TapeEditor : public juce::AudioProcessorEditor
{
public:
    explicit TapeEditor (TapeProcessor&);
    void paint (juce::Graphics&) override {}
    void resized() override;
    void applyScale (float s);
    EditorContent& getContent() { return content; }

private:
    TapeProcessor& proc;
    EditorContent content;
};
