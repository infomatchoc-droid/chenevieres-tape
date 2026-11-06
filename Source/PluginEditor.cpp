#include "PluginEditor.h"
#include "Look.h"

using namespace tape;

namespace
{
juce::String u8 (const char* t) { return juce::String::fromUTF8 (t); }
constexpr float pi = juce::MathConstants<float>::pi;
constexpr float startA = -0.75f * pi, endA = 0.75f * pi; // course des boutons (270°)

juce::String machineCaption (int m)
{
    static const char* t[3] = { "PRÉCISION SUISSE", "PUNCH AMÉRICAIN", "CHALEUR VINTAGE" };
    return u8 (t[juce::jlimit (0, 2, m)]);
}

void screw (juce::Graphics& g, float x, float y, float r)
{
    juce::ColourGradient gr (juce::Colour (0xff9a948c), x - r, y - r, juce::Colour (0xff2c2925), x + r, y + r, false);
    g.setGradientFill (gr);
    g.fillEllipse (x - r, y - r, 2 * r, 2 * r);
    g.setColour (juce::Colour (0xff141210));
    g.drawEllipse (x - r, y - r, 2 * r, 2 * r, 1.0f);
    juce::Path p;
    p.addRectangle (-r * 0.75f, -r * 0.13f, r * 1.5f, r * 0.26f);
    p.addRectangle (-r * 0.13f, -r * 0.75f, r * 0.26f, r * 1.5f);
    g.setColour (juce::Colour (0xff1b1916));
    g.fillPath (p, juce::AffineTransform::rotation (0.5f).translated (x, y));
}
}

// ======================================================================= Knob
Knob::Knob (TapeProcessor& p, int prm, juce::String t, float r)
    : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox),
      title (std::move (t)), radius (r), param (prm), proc (p),
      attachment (*p.param (prm), *this, nullptr)
{
    setRotaryParameters (startA + 2 * pi, endA + 2 * pi, true);
    setMouseDragSensitivity (220);
    setVelocityBasedMode (false);
    setDoubleClickReturnValue (true, spec (prm).def);
    setPopupDisplayEnabled (false, false, nullptr);
}

juce::String Knob::valueText() const
{
    const double v = getValue();
    switch (param)
    {
        case pInput: case pOutput: return (v > 0.05 ? "+" : "") + juce::String (v, 1) + " dB";
        case pBias:
            if (std::abs (v) < 1.0) return "Nominal";
            return (v < 0 ? "Sous " : "Sur ") + juce::String ((int) std::lround (std::abs (v))) + " %";
        default: return juce::String ((int) std::lround (v)) + " %";
    }
}

void Knob::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX(), cy = 20.0f + radius + 6.0f, r = radius;
    const bool showVal = hover || isMouseButtonDown();

    // titre
    g.setColour (look::text);
    g.setFont (look::font (radius > 34 ? 17.0f : 15.0f, 700));
    g.drawText (title, juce::Rectangle<float> (0, 0, b.getWidth(), 18), juce::Justification::centred);

    const double norm = valueToProportionOfLength (getValue());
    const float ang = startA + (float) norm * (endA - startA);

    // graduations
    for (int i = 0; i <= 10; ++i)
    {
        const float a = startA + (float) i / 10.0f * (endA - startA);
        const float r1 = r + 5.0f, r2 = r + (i % 5 == 0 ? 11.0f : 8.0f);
        g.setColour (i % 5 == 0 ? look::textDim : look::label.withAlpha (0.7f));
        g.drawLine (cx + r1 * std::sin (a), cy - r1 * std::cos (a), cx + r2 * std::sin (a), cy - r2 * std::cos (a), i % 5 == 0 ? 1.6f : 1.0f);
    }

    // arc de valeur (depuis le centre pour les réglages bipolaires)
    const bool bipolar = spec (param).min < 0;
    const float from = bipolar ? 0.0f : startA;
    juce::Path track, arc;
    track.addCentredArc (cx, cy, r + 1.5f, r + 1.5f, 0, startA, endA, true);
    g.setColour (juce::Colour (0xff2a2622));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    arc.addCentredArc (cx, cy, r + 1.5f, r + 1.5f, 0, std::min (from, ang), std::max (from, ang), true);
    g.setColour (look::gold);
    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // jupe sombre puis capuchon métal brossé
    g.setColour (juce::Colour (0xff0c0b0a));
    g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);
    const float rc = r * 0.84f;
    juce::ColourGradient cap (juce::Colour (0xffe9e6e1), cx - rc * 0.6f, cy - rc * 0.8f, juce::Colour (0xff5d5953), cx + rc * 0.5f, cy + rc, false);
    cap.addColour (0.5, juce::Colour (0xffa9a49d));
    g.setGradientFill (cap);
    g.fillEllipse (cx - rc, cy - rc, 2 * rc, 2 * rc);
    // reflets concentriques
    for (int i = 1; i < 6; ++i)
    {
        const float rr = rc * (float) i / 6.0f;
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawEllipse (cx - rr, cy - rr, 2 * rr, 2 * rr, 0.8f);
    }
    g.setColour (juce::Colour (0xff1a1816));
    g.drawEllipse (cx - rc, cy - rc, 2 * rc, 2 * rc, 1.2f);
    // repère
    g.setColour (juce::Colour (0xff1c1a17));
    const float p1 = rc * 0.25f, p2 = rc * 0.92f;
    g.drawLine (cx + p1 * std::sin (ang), cy - p1 * std::cos (ang), cx + p2 * std::sin (ang), cy - p2 * std::cos (ang), 3.0f);

    // valeur
    const juce::Rectangle<float> vr (0, cy + r + 10.0f, b.getWidth(), 18);
    if (showVal)
    {
        auto pill = vr.withSizeKeepingCentre (std::min (b.getWidth() - 4, 92.0f), 20);
        g.setColour (look::boxFill);
        g.fillRoundedRectangle (pill, 5);
        g.setColour (look::gold);
        g.drawRoundedRectangle (pill, 5, 1.0f);
        g.setColour (look::text);
    }
    else
        g.setColour (look::textDim);
    g.setFont (look::font (14.0f, 600));
    g.drawText (valueText(), vr, juce::Justification::centred);
}

// ======================================================================= Selector
Selector::Selector (TapeProcessor& p, int prm, juce::StringArray l, juce::StringArray c)
    : proc (p), param (prm), labels (std::move (l)), captions (std::move (c))
{
    setRepaintsOnMouseActivity (true);
}

void Selector::paint (juce::Graphics& g)
{
    const int n = labels.size();
    const float gap = 6.0f;
    const float w = ((float) getWidth() - gap * (float) (n - 1)) / (float) n, h = (float) getHeight();
    const int cur = toggle ? (proc.value (param) >= 0.5f ? 0 : -1) : proc.choice (param);
    for (int i = 0; i < n; ++i)
    {
        const juce::Rectangle<float> r ((w + gap) * (float) i, 0, w, h);
        const bool on = i == cur;
        if (on)
        {
            g.setColour (look::gold.withAlpha (0.25f));
            g.fillRoundedRectangle (r.expanded (2.5f), 7);
            juce::ColourGradient gr (juce::Colour (0xffffc35a), r.getX(), r.getY(), juce::Colour (0xffe28f1d), r.getX(), r.getBottom(), false);
            g.setGradientFill (gr);
        }
        else
        {
            juce::ColourGradient gr (juce::Colour (0xff2e2a26), r.getX(), r.getY(), juce::Colour (0xff1a1816), r.getX(), r.getBottom(), false);
            g.setGradientFill (gr);
        }
        g.fillRoundedRectangle (r, 5);
        g.setColour (on ? juce::Colour (0xff8a5a12) : juce::Colour (0xff050404));
        g.drawRoundedRectangle (r.reduced (0.5f), 5, 1.0f);
        const auto txt = on ? juce::Colour (0xff1a1206) : (isMouseOver() ? look::text : look::textDim);
        g.setColour (txt);
        if (captions.size() > i && captions[i].isNotEmpty())
        {
            g.setFont (look::font (h > 50 ? 21.0f : 18.0f, 700));
            g.drawText (labels[i], r.withTrimmedBottom (h * 0.36f).withTrimmedTop (2), juce::Justification::centredBottom);
            g.setColour (txt.withAlpha (0.8f));
            g.setFont (look::font (11.5f, 600));
            g.drawText (captions[i], r.withTrimmedTop (h * 0.62f), juce::Justification::centredTop);
        }
        else
        {
            g.setFont (look::font (17.0f, 700));
            g.drawText (labels[i], r, juce::Justification::centred);
        }
    }
}

void Selector::mouseDown (const juce::MouseEvent& e)
{
    const int n = labels.size();
    const int i = juce::jlimit (0, n - 1, (int) (e.position.x / ((float) getWidth() / (float) n)));
    if (toggle) proc.setValue (param, proc.value (param) >= 0.5f ? 0.0f : 1.0f);
    else proc.setValue (param, (float) i);
    repaint();
}

// ======================================================================= Contenu
EditorContent::EditorContent (TapeProcessor& p, std::function<void (float)> s)
    : proc (p), setScale (std::move (s)),
      input (p, pInput, "INPUT", 40), bias (p, pBias, "BIAS", 40), output (p, pOutput, "OUTPUT", 40),
      wow (p, pWow, "WOW", 28), flutter (p, pFlutter, "FLUTTER", 28), hiss (p, pHiss, "HISS", 28), mix (p, pMix, "MIX", 28),
      machine (p, pMachine, { "CH-80", "CH-16", "CH-440" }, { u8 ("PRÉCISION"), "PUNCH", "CHALEUR" }),
      speed (p, pSpeed, { "7.5", "15", "30" }, { "IPS", "IPS", "IPS" }),
      formula (p, pFormula, { "STANDARD", "+6 dB", "+9 dB" }),
      eq (p, pEq, { "NAB", "CCIR" }),
      autoGain (p, pAutoGain, { "AUTO GAIN" }),
      bypass (p, pBypass, { "BYPASS" })
{
    wordmark = look::image (BinaryData::wordmark_tape_png, BinaryData::wordmark_tape_pngSize);
    beagle = look::image (BinaryData::logo_beagle_png, BinaryData::logo_beagle_pngSize);
    autoGain.toggle = bypass.toggle = true;
    for (juce::Component* c : std::initializer_list<juce::Component*> { &input, &bias, &output, &wow, &flutter, &hiss, &mix,
                                                                         &machine, &speed, &formula, &eq, &autoGain, &bypass })
        addAndMakeVisible (c);
    setSize (W, H);
    lastTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
}

void EditorContent::resized()
{
    const int x0 = 556;
    machine.setBounds (x0, 112, 330, 58);
    speed.setBounds (x0 + 352, 112, 186, 58);
    formula.setBounds (x0, 206, 258, 40);
    eq.setBounds (x0 + 274, 206, 126, 40);
    autoGain.setBounds (x0 + 420, 206, 118, 40);

    const int kw = 130, ky = 266;
    input.setBounds (x0 + 6, ky, kw, 130);
    bias.setBounds (x0 + 6 + 204, ky, kw, 130);
    output.setBounds (x0 + 6 + 408, ky, kw, 130);

    const int sw = 112, sy = 418;
    for (int i = 0; i < 4; ++i)
    {
        Knob* k = i == 0 ? &wow : i == 1 ? &flutter : i == 2 ? &hiss : &mix;
        k->setBounds (x0 + i * (sw + 30), sy, sw, 118);
    }

    bypass.setBounds (W - 133 - 14 - 112 - 52, 15, 112, 36);
    sizeButton = { W - 133 - 14 - 44, 15, 40, 36 };
}

void EditorContent::mouseDown (const juce::MouseEvent& e)
{
    if (sizeButton.contains (e.getPosition()))
    {
        juce::PopupMenu m;
        for (int pct : { 75, 90, 100, 125, 150, 175, 200 })
            m.addItem (pct, juce::String (pct) + " %", true, std::abs (proc.uiScale * 100.0f - (float) pct) < 1.0f);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (sizeButton)),
                         [this] (int r) { if (r > 0 && setScale) setScale ((float) r / 100.0f); });
    }
}

void EditorContent::settle (float reelTurn)
{
    for (int c = 0; c < 2; ++c)
    {
        const float rms = proc.tapeRms[c].load();
        vu[c] = rms > 1e-6f ? 20.0f * std::log10 (rms / 0.0891f) : -30.0f;
    }
    reelAngle = reelTurn;
    counter = 1234.0f;
}

void EditorContent::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) juce::jlimit (0.0, 0.2, (now - lastTime) / 1000.0);
    lastTime = now;
    // vu-mètres : temps d'intégration ≈ 300 ms
    const bool alive = juce::Time::getMillisecondCounter() - proc.lastBlockMs.load() < 400;
    const float a = 1.0f - std::exp (-dt / 0.065f);
    for (int c = 0; c < 2; ++c)
    {
        const float rms = alive ? proc.tapeRms[c].load() : 0.0f;
        const float target = rms > 1e-6f ? 20.0f * std::log10 (rms / 0.0891f) : -30.0f; // 0 VU = sinus à -18 dBFS
        vu[c] += (juce::jlimit (-30.0f, 6.0f, target) - vu[c]) * a;
    }
    // bobines : tournent quand le DAW lit
    if (alive && proc.rolling.load())
    {
        const float ips = proc.choice (pSpeed) == 0 ? 7.5f : proc.choice (pSpeed) == 1 ? 15.0f : 30.0f;
        reelAngle += dt * ips * 0.32f;
        counter += dt * ips * 0.4f;
        if (counter >= 10000.0f) counter -= 10000.0f;
    }
    repaint();
}

void EditorContent::paintSection (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t)
{
    g.setFont (look::font (13.0f, 700));
    const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t);
    g.setColour (look::label);
    g.drawText (t, r.withWidth (tw + 2), juce::Justification::centredLeft);
    g.setColour (look::boxLine);
    g.fillRect (r.getX() + tw + 10, r.getCentreY(), r.getWidth() - tw - 10, 1.0f);
}

void EditorContent::paintBar (juce::Graphics& g)
{
    juce::ColourGradient gr (juce::Colour (0xff1e1b18), 0, 0, juce::Colour (0xff0f0e0c), 0, (float) barH, false);
    g.setGradientFill (gr);
    g.fillRect (0, 0, W, barH);
    g.setColour (look::goldDim.withAlpha (0.5f));
    g.fillRect (0, barH - 1, W, 1);

    if (wordmark.isValid())
    {
        const float wh = 40.0f, ww = wh * (float) wordmark.getWidth() / (float) wordmark.getHeight();
        g.drawImage (wordmark, { 16.0f, 33.0f - wh / 2 + 1, ww, wh });
    }
    g.setColour (look::label);
    g.setFont (look::font (13.0f, 600));
    g.drawText (u8 ("ÉMULATION DE MAGNÉTOPHONE À BANDES"), juce::Rectangle<float> (300, 0, 400, (float) barH), juce::Justification::centredLeft);

    // taille de l'interface
    auto sb = sizeButton.toFloat();
    g.setColour (look::boxFill);
    g.fillRoundedRectangle (sb, 5);
    g.setColour (look::boxLine);
    g.drawRoundedRectangle (sb.reduced (0.5f), 5, 1);
    g.setColour (look::textDim);
    juce::Path p;
    const float cx = sb.getCentreX(), cy = sb.getCentreY();
    p.startNewSubPath (cx - 8, cy + 2); p.lineTo (cx - 8, cy + 8); p.lineTo (cx - 2, cy + 8);
    p.startNewSubPath (cx + 2, cy - 8); p.lineTo (cx + 8, cy - 8); p.lineTo (cx + 8, cy - 2);
    p.startNewSubPath (cx - 7, cy + 7); p.lineTo (cx + 7, cy - 7);
    g.strokePath (p, juce::PathStrokeType (1.8f));

    // Pistache, à droite, fondu vers la gauche
    if (beagle.isValid())
    {
        const float bh = (float) barH - 1, bw = bh * (float) beagle.getWidth() / (float) beagle.getHeight();
        const juce::Rectangle<float> br ((float) W - bw, 0, bw, bh);
        g.drawImage (beagle, br);
        juce::ColourGradient fade (juce::Colour (0xff181614), br.getX(), 0, juce::Colour (0x00181614), br.getX() + 36, 0, false);
        g.setGradientFill (fade);
        g.fillRect (br.withWidth (36));
    }
}

void EditorContent::paintReel (juce::Graphics& g, juce::Point<float> c, float r, float packR, float angle)
{
    // bande enroulée
    g.setColour (juce::Colour (0xff2b1e15));
    g.fillEllipse (c.x - packR, c.y - packR, 2 * packR, 2 * packR);
    for (float rr = r * 0.3f; rr < packR; rr += 2.2f)
    {
        g.setColour (juce::Colour (0xff4a3424).withAlpha (0.25f));
        g.drawEllipse (c.x - rr, c.y - rr, 2 * rr, 2 * rr, 0.6f);
    }
    g.setColour (juce::Colour (0xff6a4c34).withAlpha (0.6f));
    g.drawEllipse (c.x - packR, c.y - packR, 2 * packR, 2 * packR, 1.0f);

    // flasque aluminium ajourée (3 fenêtres), en rotation
    juce::Path flange;
    flange.addEllipse (-r, -r, 2 * r, 2 * r);
    juce::Path windows;
    for (int i = 0; i < 3; ++i)
    {
        juce::Path w;
        const float a0 = (float) i * 2 * pi / 3 + 0.32f, a1 = a0 + 2 * pi / 3 - 0.64f;
        w.addCentredArc (0, 0, r * 0.86f, r * 0.86f, 0, a0, a1, true);
        w.addCentredArc (0, 0, r * 0.36f, r * 0.36f, 0, a1, a0, false);
        w.closeSubPath();
        windows.addPath (w);
    }
    flange.addPath (windows);
    flange.setUsingNonZeroWinding (false);
    const auto tr = juce::AffineTransform::rotation (angle).translated (c.x, c.y);
    juce::ColourGradient al (juce::Colour (0xffd9d6d0), c.x - r, c.y - r, juce::Colour (0xff6b6760), c.x + r, c.y + r, false);
    al.addColour (0.45, juce::Colour (0xffb5b0a8));
    al.addColour (0.55, juce::Colour (0xfff0ede8));
    g.setGradientFill (al);
    g.fillPath (flange, tr);
    g.setColour (juce::Colour (0xff2a2724));
    g.strokePath (flange, juce::PathStrokeType (1.2f), tr);
    // lignes brossées concentriques
    for (float rr = r * 0.9f; rr > r * 0.38f; rr -= 6.0f)
    {
        juce::Path ring;
        ring.addEllipse (-rr, -rr, 2 * rr, 2 * rr);
        g.saveState();
        g.reduceClipRegion (flange, tr);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.strokePath (ring, juce::PathStrokeType (0.7f), tr);
        g.restoreState();
    }
    // moyeu NAB
    const float hr = r * 0.27f;
    juce::ColourGradient hub (juce::Colour (0xff8d8a85), c.x - hr, c.y - hr, juce::Colour (0xff36332f), c.x + hr, c.y + hr, false);
    g.setGradientFill (hub);
    g.fillEllipse (c.x - hr, c.y - hr, 2 * hr, 2 * hr);
    g.setColour (juce::Colour (0xff141210));
    g.fillEllipse (c.x - hr * 0.42f, c.y - hr * 0.42f, hr * 0.84f, hr * 0.84f);
    juce::Path teeth;
    for (int i = 0; i < 3; ++i)
    {
        juce::Path t;
        t.addRoundedRectangle (-hr * 0.12f, -hr * 0.8f, hr * 0.24f, hr * 0.42f, 1.5f);
        teeth.addPath (t, juce::AffineTransform::rotation ((float) i * 2 * pi / 3));
    }
    g.setColour (juce::Colour (0xffc9c5bf));
    g.fillPath (teeth, tr);
    g.setColour (juce::Colour (0xff1a1816));
    g.drawEllipse (c.x - hr, c.y - hr, 2 * hr, 2 * hr, 1.0f);
}

void EditorContent::paintTransport (juce::Graphics& g, juce::Rectangle<float> area)
{
    // fenêtre sombre
    g.setColour (juce::Colour (0xff0b0a09));
    g.fillRoundedRectangle (area, 10);
    g.setColour (juce::Colour (0xff3a342d));
    g.drawRoundedRectangle (area.reduced (0.5f), 10, 1.2f);

    const float R = 96.0f;
    const juce::Point<float> cl (area.getX() + 128, area.getY() + 112), cr (area.getRight() - 128, area.getY() + 112);
    const float packL = R * 0.88f, packR = R * 0.55f;
    const float headY = area.getBottom() - 34;

    // chemin de bande : bobine gauche → galet → têtes → galet → bobine droite
    const juce::Point<float> gl (area.getCentreX() - 118, headY + 2), gr (area.getCentreX() + 118, headY + 2);
    juce::Path tapePath;
    tapePath.startNewSubPath (cl.x - packL, cl.y);
    tapePath.lineTo (gl.x - 9, gl.y);
    tapePath.lineTo (gr.x + 9, gr.y);
    tapePath.lineTo (cr.x + packR, cr.y);
    g.setColour (juce::Colour (0xff4a3322));
    g.strokePath (tapePath, juce::PathStrokeType (3.0f));

    paintReel (g, cl, R, packL, reelAngle);
    paintReel (g, cr, R, packR, reelAngle * packL / packR);

    // galets de guidage
    for (auto p : { gl, gr })
    {
        g.setColour (juce::Colour (0xffbdb8b0));
        g.fillEllipse (p.x - 9, p.y - 9, 18, 18);
        g.setColour (juce::Colour (0xff22201d));
        g.fillEllipse (p.x - 3, p.y - 3, 6, 6);
    }

    // bloc de têtes avec plaque du modèle
    const juce::Rectangle<float> block (area.getCentreX() - 92, headY - 16, 184, 40);
    juce::ColourGradient bg (juce::Colour (0xff3a3631), block.getX(), block.getY(), juce::Colour (0xff1b1917), block.getX(), block.getBottom(), false);
    g.setGradientFill (bg);
    g.fillRoundedRectangle (block, 5);
    g.setColour (juce::Colour (0xff0a0908));
    g.drawRoundedRectangle (block, 5, 1);
    const int m = proc.choice (pMachine);
    g.setColour (look::gold);
    g.setFont (look::font (19.0f, 700));
    g.drawText (tape::machine (m).name, block.withTrimmedBottom (15), juce::Justification::centred);
    g.setColour (look::textDim);
    g.setFont (look::font (10.5f, 600));
    g.drawText (machineCaption (m), block.withTrimmedTop (22), juce::Justification::centred);

    // compteur (en bas à gauche) et réglages de la bande (en bas à droite)
    const juce::Rectangle<float> cnt (area.getX() + 16, area.getBottom() - 46, 84, 30);
    g.setColour (juce::Colour (0xff191714));
    g.fillRoundedRectangle (cnt, 4);
    g.setColour (juce::Colour (0xff47413a));
    g.drawRoundedRectangle (cnt, 4, 1);
    g.setColour (look::text);
    g.setFont (look::font (21.0f, 700));
    g.drawText (juce::String ((int) counter).paddedLeft ('0', 4), cnt, juce::Justification::centred);
    const int sp = proc.choice (pSpeed);
    const juce::Rectangle<float> info (area.getRight() - 112, area.getBottom() - 50, 100, 38);
    g.setColour (look::gold);
    g.setFont (look::font (16.0f, 700));
    g.drawText (juce::String (sp == 0 ? "7.5" : sp == 1 ? "15" : "30") + " IPS", info.withHeight (18), juce::Justification::centredRight);
    g.setColour (look::label);
    g.setFont (look::font (11.5f, 600));
    const juce::String tapeName = proc.choice (pFormula) == 0 ? "STANDARD" : proc.choice (pFormula) == 1 ? "+6 dB" : "+9 dB";
    g.drawText (tapeName + u8 ("  ·  ") + (proc.choice (pEq) == 0 ? "NAB" : "CCIR"), info.withTrimmedTop (20), juce::Justification::centredRight);
}

void EditorContent::paintVu (juce::Graphics& g, juce::Rectangle<float> r, float vuDb, const juce::String& ch)
{
    // boîtier
    g.setColour (juce::Colour (0xff0b0a09));
    g.fillRoundedRectangle (r, 8);
    const auto face = r.reduced (9).withTrimmedBottom (18);
    juce::ColourGradient amber (juce::Colour (0xffffe3a0), face.getCentreX(), face.getY(), juce::Colour (0xffe8a64a), face.getCentreX(), face.getBottom() + 30, false);
    g.setGradientFill (amber);
    g.fillRoundedRectangle (face, 5);
    for (auto p : { r.getTopLeft().translated (7, 7), r.getTopRight().translated (-7, 7), r.getBottomLeft().translated (7, -7), r.getBottomRight().translated (-7, -7) })
        screw (g, p.x, p.y, 4.0f);

    // échelle VU : position proportionnelle à la tension (comme les vrais)
    const auto pos = [] (float db) {
        const float v = std::pow (10.0f, db / 20.0f), v0 = std::pow (10.0f, -20.0f / 20.0f), v1 = std::pow (10.0f, 3.0f / 20.0f);
        return (v - v0) / (v1 - v0);
    };
    const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + 40);
    const float arcR = face.getHeight() + 38, a0 = -0.74f, a1 = 0.74f;
    const auto angOf = [&] (float db) { return a0 + juce::jlimit (-0.04f, 1.04f, pos (db)) * (a1 - a0); };
    const auto pt = [&] (float a, float rad) { return juce::Point<float> (pivot.x + rad * std::sin (a), pivot.y - rad * std::cos (a)); };

    g.saveState();
    g.reduceClipRegion (face.toNearestInt());
    juce::Path scale, red;
    scale.addCentredArc (pivot.x, pivot.y, arcR - 20, arcR - 20, 0, a0, angOf (0), true);
    red.addCentredArc (pivot.x, pivot.y, arcR - 20, arcR - 20, 0, angOf (0), a1, true);
    g.setColour (juce::Colour (0xff2a1a08));
    g.strokePath (scale, juce::PathStrokeType (1.6f));
    g.setColour (juce::Colour (0xffc4281c));
    g.strokePath (red, juce::PathStrokeType (5.0f));
    g.setFont (look::font (13.0f, 700));
    for (float db : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
    {
        const float a = angOf (db);
        const bool major = db == -20 || db == -10 || db == -7 || db == -5 || db == -3 || db == 0 || db == 3;
        g.setColour (db > 0 ? juce::Colour (0xffb0201a) : juce::Colour (0xff2a1a08));
        g.drawLine ({ pt (a, arcR - 20), pt (a, arcR - (major ? 30.0f : 26.0f)) }, 1.5f);
        if (major || db == -1)
        {
            const auto tp = pt (a, arcR - 9);
            const juce::String s = db > 0 ? "+" + juce::String ((int) db) : juce::String ((int) db);
            g.drawText (db == 0 ? "0" : s, juce::Rectangle<float> (tp.x - 16, tp.y - 8, 32, 16), juce::Justification::centred);
        }
    }
    g.setColour (juce::Colour (0xff2a1a08));
    g.setFont (look::font (22.0f, 700));
    g.drawText ("VU", juce::Rectangle<float> (face.getCentreX() - 30, face.getBottom() - 34, 60, 24), juce::Justification::centred);
    // aiguille
    const float na = angOf (juce::jlimit (-22.0f, 4.0f, vuDb));
    g.setColour (juce::Colour (0xff1a1006));
    g.drawLine ({ pt (na, 40), pt (na, arcR - 16) }, 1.8f);
    g.restoreState();
    juce::ColourGradient glass (juce::Colours::white.withAlpha (0.18f), face.getX(), face.getY(), juce::Colours::transparentWhite, face.getX(), face.getCentreY(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (face, 5);

    g.setColour (look::textDim);
    g.setFont (look::font (14.0f, 700));
    g.drawText (ch, juce::Rectangle<float> (r.getX(), r.getBottom() - 21, r.getWidth(), 18), juce::Justification::centred);
}

void EditorContent::paint (juce::Graphics& g)
{
    // fond : métal sombre brossé
    g.fillAll (juce::Colour (0xff151311));
    const juce::Rectangle<float> panel (6, (float) barH + 6, (float) W - 12, (float) H - barH - 12);
    juce::ColourGradient pg (juce::Colour (0xff24211d), 0, panel.getY(), juce::Colour (0xff181614), 0, panel.getBottom(), false);
    g.setGradientFill (pg);
    g.fillRoundedRectangle (panel, 10);
    juce::Random rnd (7);
    for (float y = panel.getY() + 2; y < panel.getBottom() - 2; y += 2.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.008f + 0.012f * rnd.nextFloat()));
        g.fillRect (panel.getX() + 4, y, panel.getWidth() - 8, 1.0f);
    }
    g.setColour (juce::Colour (0xff3b352e));
    g.drawRoundedRectangle (panel.reduced (0.5f), 10, 1.2f);
    for (auto p : { panel.getTopLeft().translated (14, 14), panel.getTopRight().translated (-14, 14),
                    panel.getBottomLeft().translated (14, -14), panel.getBottomRight().translated (-14, -14) })
        screw (g, p.x, p.y, 6.0f);

    paintBar (g);

    paintTransport (g, { 32, (float) barH + 22, 488, 282 });
    paintVu (g, { 32, (float) barH + 318, 238, 154 }, vu[0], "L");
    paintVu (g, { 282, (float) barH + 318, 238, 154 }, vu[1], "R");

    // titres des sections
    const float x0 = 556;
    paintSection (g, { x0, 88, 330, 16 }, "MACHINE");
    paintSection (g, { x0 + 352, 88, 186, 16 }, "VITESSE");
    paintSection (g, { x0, 182, 258, 16 }, "BANDE");
    paintSection (g, { x0 + 274, 182, 126, 16 }, u8 ("ÉGALISATION"));
    paintSection (g, { x0 + 420, 182, 118, 16 }, "NIVEAU");
    g.setColour (look::boxLine);
    g.fillRect (x0, 406.0f, 538.0f, 1.0f);

    // repères du BIAS
    g.setColour (look::label);
    g.setFont (look::font (11.5f, 600));
    const auto bb = bias.getBounds().toFloat();
    g.drawText ("SOUS", juce::Rectangle<float> (bb.getX() - 22, bb.getY() + 98, 40, 14), juce::Justification::centred);
    g.drawText ("SUR", juce::Rectangle<float> (bb.getRight() - 18, bb.getY() + 98, 40, 14), juce::Justification::centred);
}

// ======================================================================= Éditeur
TapeEditor::TapeEditor (TapeProcessor& p)
    : AudioProcessorEditor (p), proc (p), content (p, [this] (float s) { applyScale (s); })
{
    addAndMakeVisible (content);
    applyScale (proc.uiScale);
}

void TapeEditor::applyScale (float s)
{
    proc.uiScale = juce::jlimit (0.6f, 2.0f, s);
    content.setTransform (juce::AffineTransform::scale (proc.uiScale));
    setSize (juce::roundToInt (EditorContent::W * proc.uiScale), juce::roundToInt (EditorContent::H * proc.uiScale));
}

void TapeEditor::resized()
{
    content.setBounds (0, 0, EditorContent::W, EditorContent::H);
}
