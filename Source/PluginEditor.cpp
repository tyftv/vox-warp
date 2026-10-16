#include "PluginEditor.h"
#include "BinaryData.h"

using namespace vw;
static juce::String U8 (const char* t) { return juce::String (juce::CharPointer_UTF8 (t)); }

namespace ui
{
// ---------------- palette ----------------
static const juce::Colour navy (0xff16233a), ice (0xff7fe8ff), lcdBg (0xff05192a), steelLine (0xff2a3a54);
static const juce::Colour cTune (0xff2f7dff), cBody (0xff00b8cc), cShat (0xffb636ff), cRot (0xffff7a1a);
static const juce::Colour modCol[3] { juce::Colour (0xff00a8ff), juce::Colour (0xffe030ff), juce::Colour (0xffff8a00) };

// Fonts and the logo live in a shared object owned by open editors (never in function statics),
// so they're released while the host's graphics system is still alive.
static Assets* gAssets = nullptr;
Assets::Assets()
{
    disp = juce::Typeface::createSystemTypefaceFor (BinaryData::Michroma_ttf, BinaryData::Michroma_ttfSize);
    bold = juce::Typeface::createSystemTypefaceFor (BinaryData::ChakraPetchBold_ttf, BinaryData::ChakraPetchBold_ttfSize);
    logo = juce::PNGImageFormat::loadFrom (BinaryData::Logo_png, BinaryData::Logo_pngSize);
    gAssets = this;
}
Assets::~Assets() { if (gAssets == this) gAssets = nullptr; }
static juce::Typeface::Ptr tfDisp() { return gAssets != nullptr ? gAssets->disp : juce::Typeface::Ptr(); }
static juce::Typeface::Ptr tfBold() { return gAssets != nullptr ? gAssets->bold : juce::Typeface::Ptr(); }
juce::Font disp (float h) { auto t = tfDisp(); return t != nullptr ? juce::Font (juce::FontOptions (t).withHeight (h)) : juce::Font (juce::FontOptions (h)); }
juce::Font bold (float h) { auto t = tfBold(); return t != nullptr ? juce::Font (juce::FontOptions (t).withHeight (h)) : juce::Font (juce::FontOptions (h, juce::Font::bold)); }
juce::Font mono (float h) { return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h, juce::Font::bold)); }

static void lcd (juce::Graphics& g, juce::Rectangle<float> r, float rad = 8)
{
    g.setColour (lcdBg); g.fillRoundedRectangle (r, rad);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.55f), r.getX(), r.getY(), juce::Colours::transparentBlack, r.getX(), r.getY() + 10, false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (juce::Colour (0xff34465f)); g.drawRoundedRectangle (r.reduced (0.5f), rad, 1.3f);
}

static void chromeFill (juce::Graphics& g, juce::Rectangle<float> r, float rad, float a = 1.0f)
{
    juce::ColourGradient cg (juce::Colours::white.withAlpha (a), 0, r.getY(), juce::Colour (0xffeef4fa).withAlpha (a), 0, r.getBottom(), false);
    cg.addColour (0.47, juce::Colour (0xffe2eaf3).withAlpha (a)); cg.addColour (0.53, juce::Colour (0xffb9c8d8).withAlpha (a));
    g.setGradientFill (cg); g.fillRoundedRectangle (r, rad);
}

static void gel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float rad)
{
    juce::ColourGradient cg (c.brighter (0.9f), 0, r.getY(), c, 0, r.getBottom(), false);
    cg.addColour (0.48, c); cg.addColour (0.52, c.darker (0.45f));
    g.setGradientFill (cg); g.fillRoundedRectangle (r, rad);
    auto hl = r.reduced (r.getHeight() * 0.25f, 2).withHeight (r.getHeight() * 0.42f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.75f), 0, hl.getY(), juce::Colours::white.withAlpha (0.06f), 0, hl.getBottom(), false));
    g.fillRoundedRectangle (hl, hl.getHeight() * 0.5f);
}

// ---------------- look and feel ----------------
Y2KLook::Y2KLook()
{
    if (auto t = tfBold()) setDefaultSansSerifTypeface (t);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xffeef3f9));
    setColour (juce::PopupMenu::textColourId, navy);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, ui::cTune);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void Y2KLook::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const float rad = r.getHeight() * 0.5f;
    const bool on = b.getToggleState();
    const auto accent = b.getProperties().contains ("accent") ? juce::Colour ((juce::uint32) (juce::int64) b.getProperties()["accent"]) : ui::cTune;
    g.setColour (juce::Colours::black.withAlpha (0.25f)); g.fillRoundedRectangle (r.translated (0, 1.2f), rad);
    if (on) gel (g, r, accent, rad);
    else
    {
        juce::ColourGradient cg (juce::Colours::white, 0, r.getY(), juce::Colour (0xffeef4fa), 0, r.getBottom(), false);
        cg.addColour (0.48, juce::Colour (0xffe3ebf3)); cg.addColour (0.52, juce::Colour (0xffc2cfdd));
        g.setGradientFill (cg); g.fillRoundedRectangle (r, rad);
    }
    if (over) { g.setColour (juce::Colours::white.withAlpha (0.18f)); g.fillRoundedRectangle (r, rad); }
    if (down) { g.setColour (juce::Colours::black.withAlpha (0.1f)); g.fillRoundedRectangle (r, rad); }
    g.setColour (on ? accent.darker (0.6f) : juce::Colour (0xff34465f)); g.drawRoundedRectangle (r, rad, 1.2f);
}

void Y2KLook::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    g.setFont (bold (b.getProperties().contains ("small") ? 9.5f : 11.0f));
    g.setColour (b.getToggleState() ? juce::Colours::white : navy);
    g.drawText (b.getButtonText().toUpperCase(), b.getLocalBounds(), juce::Justification::centred, false);
}

// ---------------- knob ----------------
Knob::Knob (VoxProcessor& p, int idx, float d, std::function<int()> lm, std::function<int()> mf)
    : proc (p), index (idx), dial (d), linkMode (std::move (lm)), modeForFmt (std::move (mf)),
      param (p.knobParams[(size_t) idx]), att (*param, [this] (float) { repaint(); })
{
    att.sendInitialUpdate();
}

bool Knob::hasLinks() const
{
    for (auto& L : proc.links) if (L.slot.load() >= 0 && L.param.load() == index) return true;
    return false;
}

void Knob::paint (juce::Graphics& g)
{
    const auto& def = knobDefs()[(size_t) index];
    const float v = param->getValue();
    const float cx = getWidth() * 0.5f, cy = dial * 0.5f + 1;
    const float a0 = juce::degreesToRadians (-135.0f), span = juce::degreesToRadians (270.0f);
    const float rT = dial * 0.4f;
    auto arc = [&] (float r, float from, float to, juce::Colour c, float w)
    {
        juce::Path p; p.addCentredArc (cx, cy, r, r, 0, a0 + span * from, a0 + span * to, true);
        g.setColour (c); g.strokePath (p, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };
    arc (rT, 0, 1, (dark ? juce::Colours::white : navy).withAlpha (0.18f), dial * 0.06f);
    arc (rT, 0, juce::jmax (0.004f, v), juce::Colour (0xff1f66f0), dial * 0.06f);
    for (auto& L : proc.links)
    {
        const int sl = L.slot.load();
        if (sl < 0 || L.param.load() != index) continue;
        const float end = juce::jlimit (0.0f, 1.0f, v + L.depth.load());
        const float r = dial * (0.47f + sl * 0.035f);
        arc (r, juce::jmin (v, end), juce::jmax (juce::jmax (v, end), juce::jmin (v, end) + 0.01f), modCol[sl], juce::jmax (1.6f, dial * 0.032f));
    }
    // chrome body
    const float br = dial * 0.3f;
    auto body = juce::Rectangle<float> (cx - br, cy - br, br * 2, br * 2);
    g.setColour (juce::Colours::black.withAlpha (0.3f)); g.fillEllipse (body.translated (0, 2.5f));
    {
        juce::ColourGradient cg (juce::Colour (0xfff7faff), body.getX(), body.getY(), juce::Colour (0xff6f7c8c), body.getRight(), body.getBottom(), false);
        cg.addColour (0.3, juce::Colour (0xff9aa6b5)); cg.addColour (0.5, juce::Colour (0xffeef3f9)); cg.addColour (0.72, juce::Colour (0xff7a8796)); cg.addColour (0.88, juce::Colour (0xffdfe6ee));
        g.setGradientFill (cg); g.fillEllipse (body);
        auto cap = body.reduced (br * 0.42f);
        juce::ColourGradient cc (juce::Colours::white, cap.getX() + cap.getWidth() * 0.35f, cap.getY() + cap.getHeight() * 0.3f, juce::Colour (0xff8996a6), cap.getRight(), cap.getBottom(), true);
        cc.addColour (0.55, juce::Colour (0xffd5dee8));
        g.setGradientFill (cc); g.fillEllipse (cap);
        g.setColour (juce::Colour (0xff1e2c44)); g.drawEllipse (body, 1.3f);
    }
    const float ang = a0 + span * v;
    {
        juce::Line<float> ln (cx + std::sin (ang) * br * 0.45f, cy - std::cos (ang) * br * 0.45f, cx + std::sin (ang) * br * 0.9f, cy - std::cos (ang) * br * 0.9f);
        g.setColour (juce::Colour (0xff5fb4ff).withAlpha (0.5f)); g.drawLine (ln, 5.0f);
        g.setColour (juce::Colour (0xff1f66f0)); g.drawLine (ln, 2.6f);
    }
    if (hasLinks())
    {
        const float ev = proc.effNorm[(size_t) index].load(), ga = a0 + span * ev, gr = dial * 0.4f;
        auto dot = juce::Rectangle<float> (7, 7).withCentre ({ cx + std::sin (ga) * gr, cy - std::cos (ga) * gr });
        g.setColour (ice.withAlpha (0.5f)); g.fillEllipse (dot.expanded (2));
        g.setColour (juce::Colours::white); g.fillEllipse (dot);
        g.setColour (navy); g.drawEllipse (dot, 1.3f);
    }
    if (linkMode && linkMode() >= 0 && def.linkable)
    {
        g.setColour (modCol[linkMode()].withAlpha (0.35f + 0.25f * (float) std::sin (juce::Time::getMillisecondCounterHiRes() * 0.006)));
        g.drawEllipse (juce::Rectangle<float> (dial, dial).withCentre ({ cx, cy }).reduced (1), 2.5f);
    }
    // labels
    g.setColour (dark ? juce::Colour (0xffdce6f2) : navy);
    g.setFont (bold (10.0f));
    g.drawText (labelOverride.isNotEmpty() ? labelOverride : juce::String (def.name), 0, (int) dial + 2, getWidth(), 12, juce::Justification::centred, false);
    g.setColour (dark ? juce::Colour (0xffaebdd0) : juce::Colour (0xff2c3e5c));
    g.setFont (mono (9.0f));
    const juce::String txt = dragText.isNotEmpty() ? dragText : formatKnob (def.id, v, modeForFmt ? modeForFmt() : 0);
    g.drawText (txt, 0, (int) dial + 14, getWidth(), 11, juce::Justification::centred, false);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    startY = e.getScreenY(); moved = false;
    linking = linkMode && linkMode() >= 0 && knobDefs()[(size_t) index].linkable;
    if (linking) { const int li = proc.findLink (linkMode(), index); startV = li >= 0 ? proc.links[(size_t) li].depth.load() : 0.0f; }
    else { startV = param->getValue(); att.beginGesture(); }
}
void Knob::mouseDrag (const juce::MouseEvent& e)
{
    const int dy = startY - e.getScreenY();
    if (std::abs (dy) > 3) moved = true;
    if (! moved) return;
    const float dv = dy / (e.mods.isShiftDown() ? 700.0f : 200.0f);
    if (linking)
    {
        const float depth = juce::jlimit (-1.0f, 1.0f, startV + dv);
        proc.setLink (linkMode(), index, depth);
        dragText = juce::String::charToString ((juce::juce_wchar) ('A' + linkMode())) + " " + (depth > 0 ? "+" : "") + juce::String (juce::roundToInt (depth * 100)) + "%";
    }
    else att.setValueAsPartOfGesture (juce::jlimit (0.0f, 1.0f, startV + dv));
    repaint();
}
void Knob::mouseUp (const juce::MouseEvent&)
{
    if (linking)
    {
        if (! moved) { const int lm = linkMode(); if (proc.findLink (lm, index) >= 0) proc.removeLink (lm, index); else proc.setLink (lm, index, 0.5f); }
    }
    else att.endGesture();
    dragText.clear(); repaint();
}
void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (linking) return;
    att.setValueAsCompleteGesture (knobDefs()[(size_t) index].def);
}
void Knob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
{
    att.setValueAsCompleteGesture (juce::jlimit (0.0f, 1.0f, param->getValue() + w.deltaY * 0.05f));
}

// ---------------- mod canvas ----------------
void ModCanvas::put (juce::Point<float> p)
{
    const int xi = juce::jlimit (0, 63, (int) (p.x / (float) getWidth() * 64));
    const float yv = juce::jlimit (0.0f, 1.0f, 1.0f - (p.y - 4) / (float) (getHeight() - 8));
    auto& a = proc.shapes[(size_t) slot];
    if (last < 0) a[(size_t) xi] = yv;
    else
    {
        const float y0 = a[(size_t) last].load();
        const int s = juce::jmin (last, xi), t = juce::jmax (last, xi);
        for (int k = s; k <= t; ++k) a[(size_t) k] = (t == s) ? yv : y0 + (yv - y0) * (float) std::abs (k - last) / (float) std::abs (xi - last);
    }
    last = xi; repaint();
}
void ModCanvas::mouseDown (const juce::MouseEvent& e) { last = -1; put (e.position); }
void ModCanvas::mouseDrag (const juce::MouseEvent& e) { put (e.position); }

void ModCanvas::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    lcd (g, r, 8);
    const float w = r.getWidth(), h = r.getHeight();
    g.setColour (ice.withAlpha (0.09f));
    for (int i = 1; i < 8; ++i) g.drawVerticalLine (juce::roundToInt (i * w / 8), 2, h - 2);
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine (juce::roundToInt (i * h / 4), 2, w - 2);
    const auto col = modCol[slot];
    auto& a = proc.shapes[(size_t) slot];
    auto yOf = [&] (float v) { return (1 - v) * (h - 10) + 5; };
    juce::Path line, fill;
    for (int k = 0; k < 64; ++k) { const float px = (k + 0.5f) / 64 * w, py = yOf (a[(size_t) k].load()); if (k == 0) { line.startNewSubPath (px, py); fill.startNewSubPath (0, h); fill.lineTo (px, py); } else { line.lineTo (px, py); fill.lineTo (px, py); } }
    fill.lineTo (w, h); fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (col.withAlpha (0.55f), 0, 0, col.withAlpha (0.02f), 0, h, false)); g.fillPath (fill);
    g.setColour (col); g.strokePath (line, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const float ph = proc.modPhase[(size_t) slot].load(), px = ph * w, py = yOf (proc.modVal[(size_t) slot].load());
    g.setColour (ice); g.drawLine (px, 2, px, h - 2, 1.4f);
    g.setColour (juce::Colours::white); g.fillEllipse (px - 4.5f, py - 4.5f, 9, 9);
    g.setColour (lcdBg); g.drawEllipse (px - 4.5f, py - 4.5f, 9, 9, 1.8f);
    const int mode = proc.modeParams[(size_t) slot]->getIndex();
    const float sv = proc.knobParams[(size_t) knobIndex ("sens" + juce::String (slot))]->getValue();
    if (mode == 0 || sv >= 0.01f)
    {
        const float env = proc.modEnv[(size_t) slot].load(), mh = juce::jmin (1.0f, env / 0.5f) * (h - 8), thr = h - 4 - (h - 8) * 0.5f;
        g.setColour (lcdBg.withAlpha (0.9f)); g.fillRect (w - 10, 3.0f, 7.0f, h - 6);
        g.setColour (env > 0.25f ? juce::Colours::white : col); g.fillRect (w - 9, h - 4 - mh, 5.0f, mh);
        g.setColour (juce::Colours::white); g.drawHorizontalLine ((int) thr, w - 13, w - 1);
    }
    if (flash > 0.01f) { g.setColour (juce::Colours::white.withAlpha (flash)); g.drawRoundedRectangle (r.reduced (1.5f), 8, 3); }
}

// ---------------- mod panel ----------------
ModPanel::ModPanel (VoxProcessor& p, int s, std::function<int()> lm, std::function<void (int)> setLm)
    : canvas (p, s), proc (p), slot (s), linkMode (std::move (lm)), setLinkMode (std::move (setLm))
{
    addAndMakeVisible (canvas);
    auto modeOf = [this] { return proc.modeParams[(size_t) slot]->getIndex(); };
    rate = std::make_unique<Knob> (p, knobIndex ("rate" + juce::String (s)), 40.0f, linkMode, modeOf);
    sens = std::make_unique<Knob> (p, knobIndex ("sens" + juce::String (s)), 40.0f, linkMode, modeOf);
    rate->dark = sens->dark = true;
    addAndMakeVisible (*rate); addAndMakeVisible (*sens);
    for (auto* b : { &modeBtn, &linkBtn }) { addAndMakeVisible (*b); b->getProperties().set ("small", true); b->getProperties().set ("accent", (juce::int64) modCol[s].getARGB()); }
    linkBtn.setButtonText ("LINK");
    modeBtn.onClick = [this]
    {
        auto* mp = proc.modeParams[(size_t) slot]; const int nx = (mp->getIndex() + 1) % 3;
        mp->beginChangeGesture(); mp->setValueNotifyingHost (mp->convertTo0to1 ((float) nx)); mp->endChangeGesture(); refresh();
    };
    linkBtn.onClick = [this] { setLinkMode (linkMode() == slot ? -1 : slot); };
    for (auto* nm : { "SIN", "SAW", "SQR", "RND", "CLR" })
    {
        auto* b = shapeBtns.add (new juce::TextButton (nm));
        b->getProperties().set ("small", true);
        const juce::String kind (nm);
        b->onClick = [this, kind] { proc.shapeFill (slot, kind); canvas.repaint(); };
        addAndMakeVisible (b);
    }
    refresh();
}

void ModPanel::refresh()
{
    const int mode = proc.modeParams[(size_t) slot]->getIndex();
    modeBtn.setButtonText (modeNames()[mode]);
    modeBtn.setToggleState (mode == 0, juce::dontSendNotification);
    linkBtn.setToggleState (linkMode() == slot, juce::dontSendNotification);
    rate->labelOverride = mode == 1 ? "DIV" : mode == 0 ? "LENGTH" : "RATE";
    juce::StringArray parts;
    for (auto& L : proc.links)
        if (L.slot.load() == slot) { const float d = L.depth.load(); parts.add (juce::String (knobDefs()[(size_t) L.param.load()].name) + " " + (d > 0 ? "+" : "") + juce::String (juce::roundToInt (d * 100)) + "%"); }
    const juce::String lt = parts.isEmpty() ? juce::String::fromUTF8 ("\xe2\x86\x92 not linked") : juce::String::fromUTF8 ("\xe2\x86\x92 ") + parts.joinIntoString (U8 (" \xc2\xb7 "));
    if (lt != linkText) { linkText = lt; repaint(); }
    const int tc = proc.modTrigCount[(size_t) slot].load();
    if (tc != canvas.lastTrig) { canvas.lastTrig = tc; canvas.flash = 1; }
    canvas.flash = juce::jmax (0.0f, canvas.flash - 0.07f);
    canvas.repaint(); rate->repaint(); sens->repaint();
}

void ModPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    juce::ColourGradient cg (juce::Colour (0xff222b3a), 0, 0, juce::Colour (0xff161d29), 0, r.getHeight(), false);
    g.setGradientFill (cg); g.fillRoundedRectangle (r, 12);
    if (linkMode() == slot) { g.setColour (modCol[slot]); g.drawRoundedRectangle (r.reduced (1), 12, 2.5f); }
    else { g.setColour (juce::Colour (0xff0f1828)); g.drawRoundedRectangle (r.reduced (0.5f), 12, 1.2f); }
    g.setColour (modCol[slot]); g.setFont (disp (11.0f));
    g.drawText ("MOD " + juce::String::charToString ((juce::juce_wchar) ('A' + slot)), 12, 8, 120, 18, juce::Justification::centredLeft);
    g.setColour (ice.withAlpha (0.85f)); g.setFont (mono (8.5f));
    g.drawText (linkText, getWidth() - 160, getHeight() - 28, 150, 20, juce::Justification::centredRight, true);
}

void ModPanel::resized()
{
    const int w = getWidth(), h = getHeight();
    linkBtn.setBounds (w - 62, 6, 52, 22);
    modeBtn.setBounds (w - 122, 6, 56, 22);
    canvas.setBounds (10, 34, w - 10 - 10 - 82, h - 34 - 36);
    rate->setBounds (w - 86, 34, 80, 68);
    sens->setBounds (w - 86, 34 + 74, 80, 68);
    int x = 10;
    for (auto* b : shapeBtns) { b->setBounds (x, h - 28, 38, 20); x += 41; }
}

// ---------------- visualizer ----------------
Visualizer::Visualizer (VoxProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }

void Visualizer::tick (float dt)
{
    dt = juce::jlimit (0.001f, 0.05f, dt);
    t += dt;
    // pull the latest 2048 output samples
    const int wpos = proc.vizWrite.load();
    for (int i = 0; i < 2048; ++i) td[(size_t) i] = proc.vizBuf[(size_t) ((wpos - 2048 + i) & (VoxProcessor::vizSize - 1))];
    float e = 0; for (auto v : td) e += v * v;
    lvl = std::sqrt (e / 2048.0f);
    running = lvl > 0.0005f || proc.teleRms.load() > 0.002f;
    std::fill (fftBuf.begin(), fftBuf.end(), 0.0f);
    for (int i = 0; i < 2048; ++i) fftBuf[(size_t) i] = td[(size_t) i] * (0.5f - 0.5f * std::cos (6.2831853f * i / 2047.0f));
    fft.performFrequencyOnlyForwardTransform (fftBuf.data());
    const double sr = proc.sampleRateAtomic.load(); const double ny = sr / 2;
    for (int b = 0; b < NB; ++b)
    {
        const int b0 = juce::jlimit (1, 1023, (int) std::round (70 * std::pow (160.0, b / (double) NB) / ny * 1024));
        const int b1 = juce::jlimit (b0, 1023, (int) std::round (70 * std::pow (160.0, (b + 1) / (double) NB) / ny * 1024) - 1);
        float m = 0; for (int i = b0; i <= b1; ++i) m += fftBuf[(size_t) i];
        m /= (float) (b1 - b0 + 1);
        const float db = 20 * std::log10 (m * 2 / 2048.0f + 1e-9f);
        const float nv = running ? juce::jlimit (0.0f, 1.0f, (db + 100) / 70) : 0.16f + 0.1f * std::sin (t * 1.1f + b * 0.7f) + 0.06f * std::sin (t * 2.3f + b * 1.9f);
        bands[(size_t) b] = bands[(size_t) b] * 0.7f + nv * 0.3f;
    }
    if (! running) for (int i = 0; i < 2048; ++i) td[(size_t) i] = 0.04f * std::sin (i * 0.02f + t * 3) * std::sin (t * 0.7f);

    // liquid physics, driven by an auto-gained level
    if (! lq.ok) lq.init();
    pk = juce::jmax (lvl, pk * 0.997f, 0.03f);
    const float rg = reactGain (E ("react")), ln = running ? juce::jmin (2.5f, lvl / pk * rg) : 0.0f;
    env += (ln - env) * (ln > env ? 0.45f : 0.08f);
    float onset = juce::jmax (0.0f, ln - la * 1.25f); la += (ln - la) * 0.08f;
    const bool freeze = running && proc.teleStut.load();
    const float dirG = E ("reverse") > 0.5f ? -1.0f : 1.0f;
    Liquid::Forces F;
    F.gx = std::sin (t * 1.6f) * E ("width") * 2.2f; F.gy = std::cos (t * 1.1f) * E ("width") * 1.2f;
    F.mag = 0.9f;   // gentle hold toward the middle so wax doesn't stick to the glass
    if (dirG < 0) F.buoy = -F.buoy;   // REVERSE: upside-down lava lamp
    F.spike = (0.4f + E ("formant") * 0.6f) * (running ? 0.3f + juce::jmin (env, 1.5f) * 0.4f : 0.3f);
    F.swirl = E ("fold") > 0.01f ? std::sin (t * 0.9f) * E ("fold") * 5 : 0;
    F.noise = E ("scatter") * 10; F.damp = 0.992f - E ("spray") * 0.003f;
    F.flow = 0.08f; F.t = t;
    F.buoy = 5.0f + (running ? juce::jmin (env, 1.6f) * 4.0f : 0.0f);      // louder = hotter = faster lava
    F.heat = 0.5f + (running ? juce::jmin (env, 1.6f) * 1.2f : 0.0f);
    if (! running && uni (rng) < 0.006f) onset = 0.06f;
#if VW_DEBUG
    { static const juce::String off = juce::SystemStats::getEnvironmentVariable ("VWOFF", "");
      if (off.contains ("flow")) F.flow = 0; if (off.contains ("spike")) F.spike = 0; if (off.contains ("kick")) { onset = 0; dripCool = 99; } if (off.contains ("grav")) F.mag = 4.0f; }
#endif
#if VW_DEBUG
    fprintf (stderr, "F mag=%.2f spike=%.2f flow=%.2f env=%.2f onset=%.2f lvl=%.3f dt=%.3f\n", F.mag, F.spike, F.flow, env, onset, lvl, dt);
#endif
    if (! freeze)
    {
        kickCool -= dt; dripCool -= dt;
        if (onset < 0.08f) kickArmed = true;
        // while there's sound, keep shedding drips: more often and harder as it gets louder
        if (running && env > 0.35f && dripCool <= 0)
        {
            lq.kick ((0.08f + 0.1f * juce::jmin (env, 1.5f) / 1.5f) * (1 + E ("spray") * 2.0f));
            dripCool = (0.4f + uni (rng) * 0.6f) / juce::jmax (0.6f, juce::jmin (env, 1.6f));
        }
        if (kickArmed && kickCool <= 0 && onset > 0.25f)
        {
            lq.kick (juce::jmin (0.35f, 0.12f + onset * 0.25f) * (1 + E ("spray") * 2.0f)); dripCool = juce::jmax (dripCool, 0.15f);
#if VW_DEBUG
            fprintf (stderr, "KICK onset=%.2f\n", onset);
#endif
            kickArmed = false; kickCool = 0.12f;
        }
        const int sub = juce::jmax (2, (int) std::ceil (dt / (1.0f / 120.0f)));
        for (int k = 0; k < sub; ++k) lq.step (dt / sub, F, bands.data(), NB);
    }
    rot += dt * (0.12f + E ("spray") * 0.5f) * dirG;

    // spectrograph tunnel (ping-pong feedback)
    const float scale = (float) juce::Component::getApproximateScaleFactorForComponent (this);
    const int full = juce::jlimit (128, 900, (int) (getWidth() * scale));
    const int fs = juce::jmax (64, full / 2);
    if (! A.isValid() || A.getWidth() != fs) { A = juce::Image (juce::Image::ARGB, fs, fs, true, juce::SoftwareImageType()); B = juce::Image (juce::Image::ARGB, fs, fs, true, juce::SoftwareImageType()); }
    {
        juce::Graphics g (B);
        g.fillAll (juce::Colour (0xff050a14));
        const float zoom = freeze ? 1.0f : 1.012f + lvl * 0.4f + E ("spray") * 0.012f;
        const float spin = freeze ? 0.0f : dirG * (0.002f + E ("scatter") * 0.012f);
        g.setOpacity (freeze ? 0.985f : 0.925f);
        g.drawImageTransformed (A, juce::AffineTransform::translation (-fs * 0.5f, -fs * 0.5f).scaled (zoom * (1 + E ("width") * 0.012f), zoom).rotated (spin).translated (fs * 0.5f, fs * 0.5f));
        g.setOpacity (1.0f);
        const float hue = 195 + E ("formant") * 110, r0 = fs * 0.17f;
        for (int k = 0; k < 72; ++k)
        {
            const int bi = k < 36 ? k : 71 - k; const float m = bands[(size_t) juce::jmin (NB - 1, (int) (bi / 36.0f * NB))];
            const float a0 = k / 72.0f * juce::MathConstants<float>::twoPi + rot;
            juce::Path p; p.addCentredArc (fs * 0.5f, fs * 0.5f, r0, r0, 0, a0, a0 + juce::MathConstants<float>::twoPi / 72 + 0.01f, true);
            g.setColour (juce::Colour::fromHSL (std::fmod (hue + m * 70 + (k % 2) * 10, 360.0f) / 360.0f, 1.0f, 0.36f + m * 0.42f, 1.0f));
            g.strokePath (p, juce::PathStrokeType (1 + m * fs * 0.022f));
        }
    }
    std::swap (A, B);

    // liquid image
    const int G = E ("decim") > 0.05f ? juce::jmax (36, (int) std::round (132 * (1 - E ("decim") * 0.75f))) : juce::jlimit (160, 240, (int) (full * 0.75f));
    if (! liq.isValid() || liq.getWidth() != G) liq = juce::Image (juce::Image::ARGB, G, G, true, juce::SoftwareImageType());
#if VW_DEBUG
    const double t0 = juce::Time::getMillisecondCounterHiRes();
#endif
    lq.render (liq, 195 + E ("formant") * 110, E ("throat"), E ("crush"), t, env);
#if VW_DEBUG
    fprintf (stderr, "rendertime G=%d %.2f ms\n", G, juce::Time::getMillisecondCounterHiRes() - t0);
#endif
}

void Visualizer::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    // chrome bezel
    {
        juce::ColourGradient cg (juce::Colours::white, b.getX(), b.getY(), juce::Colour (0xff5d6b80), b.getRight(), b.getBottom(), false);
        cg.addColour (0.25, juce::Colour (0xff8796aa)); cg.addColour (0.45, juce::Colour (0xffeef4fb)); cg.addColour (0.7, juce::Colour (0xff93a2b6)); cg.addColour (0.88, juce::Colours::white);
        g.setColour (juce::Colours::black.withAlpha (0.3f)); g.fillEllipse (b.reduced (2).translated (0, 4));
        g.setGradientFill (cg); g.fillEllipse (b.reduced (1));
        g.setColour (juce::Colour (0xff1e2c44)); g.drawEllipse (b.reduced (1), 1.4f);
        for (auto [fx, fy] : { std::pair<float, float> { 0.15f, 0.15f }, { 0.85f, 0.15f }, { 0.15f, 0.85f }, { 0.85f, 0.85f } })
        {
            auto sc = juce::Rectangle<float> (9, 9).withCentre ({ b.getX() + b.getWidth() * fx, b.getY() + b.getHeight() * fy });
            g.setGradientFill (juce::ColourGradient (juce::Colours::white, sc.getX(), sc.getY(), juce::Colour (0xff4d5a6c), sc.getRight(), sc.getBottom(), false));
            g.fillEllipse (sc); g.setColour (juce::Colour (0xff1e2c44)); g.drawEllipse (sc, 1.0f);
        }
    }
    auto gl = b.reduced (b.getWidth() * 0.04f);
    juce::Path clip; clip.addEllipse (gl);
    g.saveState();
    g.reduceClipRegion (clip);
    g.setColour (juce::Colour (0xff050a14)); g.fillEllipse (gl);
    const float decim = E ("decim"), crush = E ("crush");
    const int pix = 1 + (int) (decim * 9 + crush * 5);
    if (A.isValid())
    {
        g.setOpacity (1.0f);
        if (pix > 1)
        {
            const int tw = juce::jmax (8, A.getWidth() / pix);
            if (! tiny.isValid() || tiny.getWidth() != tw) tiny = juce::Image (juce::Image::ARGB, tw, tw, true, juce::SoftwareImageType());
            { juce::Graphics tg (tiny); tg.drawImage (A, 0, 0, tw, tw, 0, 0, A.getWidth(), A.getHeight()); }
            g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
            g.drawImage (tiny, gl);
        }
        else { g.setImageResamplingQuality (juce::Graphics::highResamplingQuality); g.drawImage (A, gl); }
    }
    const float cx = gl.getCentreX(), cy = gl.getCentreY(), s = gl.getWidth();
    // oscilloscope ring
    {
        juce::Path p;
        for (int i = 0; i <= 160; ++i)
        {
            const float a = i / 160.0f * juce::MathConstants<float>::twoPi, v = td[(size_t) ((i % 160) * 2048 / 160)], r = s * 0.3f + v * s * 0.12f;
            const float x = cx + std::sin (a) * r, y = cy - std::cos (a) * r;
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (ice.withAlpha (0.35f)); g.strokePath (p, juce::PathStrokeType (1.3f));
    }
    if (liq.isValid())
    {
        g.setImageResamplingQuality ((crush > 0.04f || decim > 0.05f) ? juce::Graphics::lowResamplingQuality : juce::Graphics::highResamplingQuality);
        g.setOpacity (1.0f);
        g.drawImage (liq, gl);
    }
    // pitch orbit: scale notes, detected note, tuned note
    const float orb = s * 0.43f;
    g.setColour (ice.withAlpha (0.22f)); g.drawEllipse (juce::Rectangle<float> (orb * 2, orb * 2).withCentre ({ cx, cy }), 1.0f);
    static const std::vector<int> SS[6] { {}, { 0,1,2,3,4,5,6,7,8,9,10,11 }, { 0,2,4,5,7,9,11 }, { 0,2,3,5,7,8,10 }, { 0,3,5,7,10 }, { 0,1,4,5,7,8,10 } };
    const int key = proc.keyParam->getIndex(), scl = proc.scaleParam->getIndex();
    for (int pc = 0; pc < 12; ++pc)
    {
        const float a = pc / 12.0f * juce::MathConstants<float>::twoPi;
        const bool inS = scl == 0 || std::find (SS[scl].begin(), SS[scl].end(), ((pc - key) % 12 + 12) % 12) != SS[scl].end();
        const float rr = inS ? s * 0.008f : s * 0.004f;
        const juce::Point<float> pt (cx + std::sin (a) * orb, cy - std::cos (a) * orb);
        g.setColour (inS ? ice.withAlpha (0.95f) : juce::Colour (0x33c8dcf0)); g.fillEllipse (juce::Rectangle<float> (rr * 2, rr * 2).withCentre (pt));
        if (pc == key) { g.setColour (juce::Colours::white); g.drawEllipse (juce::Rectangle<float> (s * 0.032f, s * 0.032f).withCentre (pt), 1.4f); }
    }
    if (running && proc.teleRms.load() > 0.004f)
    {
        auto pa = [] (float m) { return std::fmod (std::fmod (m, 12.0f) + 12.0f, 12.0f) / 12.0f * juce::MathConstants<float>::twoPi; };
        const float ai = pa (proc.teleIn.load()), ao = pa (proc.teleOut.load());
        if (proc.teleVoiced.load()) { g.setColour (juce::Colours::white.withAlpha (0.55f)); g.fillEllipse (juce::Rectangle<float> (s * 0.018f, s * 0.018f).withCentre ({ cx + std::sin (ai) * orb, cy - std::cos (ai) * orb })); }
        g.setColour (ice.withAlpha (0.4f)); g.fillEllipse (juce::Rectangle<float> (s * 0.05f, s * 0.05f).withCentre ({ cx + std::sin (ao) * orb, cy - std::cos (ao) * orb }));
        g.setColour (ice); g.fillEllipse (juce::Rectangle<float> (s * 0.032f, s * 0.032f).withCentre ({ cx + std::sin (ao) * orb, cy - std::cos (ao) * orb }));
    }
    if (running && proc.teleStut.load())
    {
        juce::Path ring; ring.addEllipse (juce::Rectangle<float> (orb * 1.72f, orb * 1.72f).withCentre ({ cx, cy }));
        juce::Path dashed; const float dl[] { s * 0.02f, s * 0.015f };
        juce::PathStrokeType (s * 0.006f).createDashedStroke (dashed, ring, dl, 2);
        g.setColour (juce::Colour (0xffe030ff)); g.fillPath (dashed);
    }
    // glass highlight + vignette
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.28f), cx - s * 0.12f, gl.getY() + s * 0.1f, juce::Colours::transparentWhite, cx - s * 0.12f, gl.getY() + s * 0.32f, true));
    g.fillEllipse (juce::Rectangle<float> (s * 0.6f, s * 0.28f).withCentre ({ cx - s * 0.12f, gl.getY() + s * 0.14f }));
    g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, cx, cy, juce::Colours::black.withAlpha (0.55f), cx + s * 0.5f, cy, true));
    g.fillEllipse (gl);
    g.restoreState();
    g.setColour (juce::Colour (0xff0d1626)); g.drawEllipse (gl, 2.0f);
}

// ---------------- readout ----------------
void Readout::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    lcd (g, r, 10);
    auto noteName = [] (float m) { const int n = juce::roundToInt (m); return keyNames()[((n % 12) + 12) % 12] + juce::String (n / 12 - 1); };
    const bool live = proc.teleRms.load() > 0.004f;
    juce::StringArray rows { "IN", "OUT", "GRAINS" };
    juce::StringArray vals { live && proc.teleVoiced.load() ? noteName (proc.teleIn.load()) : "--", live ? noteName (proc.teleOut.load()) : "--", juce::String (proc.teleGrains.load()) };
    g.setFont (mono (10.0f));
    for (int i = 0; i < 3; ++i)
    {
        const int y = 7 + i * 14;
        g.setColour (juce::Colour (0xff5d8aa8)); g.drawText (rows[i], 10, y, 60, 13, juce::Justification::centredLeft);
        g.setColour (ice); g.drawText (vals[i], getWidth() - 70, y, 60, 13, juce::Justification::centredRight);
    }
    if (proc.teleStut.load() && live) { g.setColour (juce::Colour (0xffe030ff)); g.drawText ("STUTTER", 10, 7 + 3 * 14, getWidth() - 20, 13, juce::Justification::centred); }
}
} // namespace ui

// ======================= editor =======================
namespace
{
const juce::Rectangle<int> podTune { 8, 46, 340, 210 }, podShat { 8, 262, 340, 174 }, podHarm { 752, 46, 340, 210 }, podRot { 752, 262, 340, 174 };
}

VoxEditor::VoxEditor (VoxProcessor& p) : AudioProcessorEditor (p), proc (p), viz (p), readout (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (root);
    auto lm = [this] { return linkMode; };

    auto place = [&] (const char* id, int x, int y, float dial = 56.0f)
    {
        auto* k = knobs.add (new ui::Knob (proc, knobIndex (id), dial, lm));
        root.addAndMakeVisible (k);
        k->setBounds (x, y, 80, (int) dial + 28);
    };
    // TUNE
    for (int i = 0; i < 4; ++i) place (std::array<const char*, 4> { "snap", "shift", "formant", "throat" }[(size_t) i], podTune.getX() + 10 + i * 80, podTune.getY() + 34);
    place ("width", podTune.getX() + 10, podTune.getY() + 34 + 86);
    // HARMONY
    for (int i = 0; i < 4; ++i) place (std::array<const char*, 4> { "h1", "h2", "hblend", "hspread" }[(size_t) i], podHarm.getX() + 10 + i * 80, podHarm.getY() + 34);
    place ("hhuman", podHarm.getX() + 130, podHarm.getY() + 34 + 86);
    // SHATTER / ROT
    for (int i = 0; i < 4; ++i) place (std::array<const char*, 4> { "spray", "scatter", "stutter", "reverse" }[(size_t) i], podShat.getX() + 10 + i * 80, podShat.getY() + 26 + (podShat.getHeight() - 26 - 84) / 2);
    for (int i = 0; i < 3; ++i) place (std::array<const char*, 3> { "crush", "decim", "fold" }[(size_t) i], podRot.getX() + 50 + i * 80, podRot.getY() + 26 + (podRot.getHeight() - 26 - 84) / 2);
    // master
    for (int i = 0; i < 3; ++i) place (std::array<const char*, 3> { "react", "mix", "out" }[(size_t) i], 494 + i * 82, 366, 46.0f);

    root.addAndMakeVisible (viz); viz.setBounds (394, 46, 312, 312);
    root.addAndMakeVisible (readout); readout.setBounds (360, 368, 126, 64);

    for (int i = 0; i < 3; ++i)
    {
        mods[i] = std::make_unique<ui::ModPanel> (proc, i, lm, [this] (int m) { setLinkMode (m); });
        root.addAndMakeVisible (*mods[i]);
        mods[i]->setBounds (8 + i * 364, 442, 356, 250);
    }

    // key / scale
    const int ky = podTune.getY() + 34 + 86 + 28;
    for (auto* b : { &keyDn, &keyUp, &scaleBtn }) { root.addAndMakeVisible (*b); b->getProperties().set ("small", true); }
    keyDn.setBounds (podTune.getX() + 130, ky, 26, 22);
    root.addAndMakeVisible (keyLcd);
    keyLcd.setBounds (podTune.getX() + 158, ky, 40, 22);
    keyLcd.setJustificationType (juce::Justification::centred);
    keyLcd.setFont (ui::mono (12.0f));
    keyLcd.setColour (juce::Label::backgroundColourId, ui::lcdBg);
    keyLcd.setColour (juce::Label::textColourId, ui::ice);
    keyLcd.setColour (juce::Label::outlineColourId, juce::Colour (0xff34465f));
    keyUp.setBounds (podTune.getX() + 200, ky, 26, 22);
    scaleBtn.setBounds (podTune.getX() + 254, ky, 72, 22);
    auto stepKey = [this] (int d)
    {
        auto* kp = proc.keyParam; const int nx = (kp->getIndex() + d + 12) % 12;
        kp->beginChangeGesture(); kp->setValueNotifyingHost (kp->convertTo0to1 ((float) nx)); kp->endChangeGesture();
    };
    keyDn.onClick = [stepKey] { stepKey (-1); };
    keyUp.onClick = [stepKey] { stepKey (1); };
    scaleBtn.onClick = [this]
    {
        auto* sp = proc.scaleParam; const int nx = (sp->getIndex() + 1) % 6;
        sp->beginChangeGesture(); sp->setValueNotifyingHost (sp->convertTo0to1 ((float) nx)); sp->endChangeGesture();
    };

    // presets
    for (auto* b : { &prevBtn, &nextBtn, &presetBtn }) root.addAndMakeVisible (*b);
    prevBtn.setBounds (380, 9, 28, 24); presetBtn.setBounds (412, 9, 200, 24); nextBtn.setBounds (616, 9, 28, 24);
    auto go = [this] (int d) { const int n = (int) VoxProcessor::presets().size(); proc.applyPreset ((proc.currentPresetAtomic.load() + d + n) % n); refreshPreset(); };
    prevBtn.onClick = [go] { go (-1); };
    nextBtn.onClick = [go] { go (1); };
    presetBtn.onClick = [this]
    {
        juce::PopupMenu m;
        const auto& P = VoxProcessor::presets();
        for (int i = 0; i < (int) P.size(); ++i) m.addItem (i + 1, P[(size_t) i].name, true, i == proc.currentPresetAtomic.load());
        juce::Component::SafePointer<VoxEditor> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [safe] (int r) { if (safe != nullptr && r > 0) { safe->proc.applyPreset (r - 1); safe->refreshPreset(); } });
    };
    root.addChildComponent (doneBtn);
    doneBtn.setToggleState (true, juce::dontSendNotification);
    doneBtn.setBounds (1010, 9, 80, 24);
    doneBtn.onClick = [this] { setLinkMode (-1); };
    refreshPreset();

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio ((double) W / H);
    setResizeLimits (W * 6 / 10, H * 6 / 10, W * 2, H * 2);
    setSize (W, H);
    lastTime = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (60);
}

VoxEditor::~VoxEditor() { stopTimer(); setLookAndFeel (nullptr); }

void VoxEditor::resized()
{
    root.setBounds (0, 0, W, H);
    root.setTransform (juce::AffineTransform::scale ((float) getWidth() / W, (float) getHeight() / H));
}

void VoxEditor::refreshPreset()
{
    const auto& P = VoxProcessor::presets();
    presetBtn.setButtonText (P[(size_t) juce::jlimit (0, (int) P.size() - 1, proc.currentPresetAtomic.load())].name);
}

void VoxEditor::setLinkMode (int m)
{
    linkMode = m;
    doneBtn.setVisible (m >= 0);
    for (auto& mp : mods) mp->refresh();
    root.repaint();
}

void VoxEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dt = (float) ((now - lastTime) / 1000.0); lastTime = now;
    viz.tick (dt); viz.repaint();
    if (++tickCount % 2 == 0)
    {
        for (auto& mp : mods) mp->refresh();
        for (auto* k : knobs) if (k->hasLinks() || linkMode >= 0) k->repaint();
        readout.repaint();
        keyLcd.setText (keyNames()[proc.keyParam->getIndex()], juce::dontSendNotification);
        scaleBtn.setButtonText (scaleNames()[proc.scaleParam->getIndex()]);
        refreshPreset();
    }
}

void VoxEditor::Root::paint (juce::Graphics& g)
{
    using namespace ui;
    const auto b = getLocalBounds().toFloat();
    // silver iridescent ground
    {
        juce::ColourGradient cg (juce::Colour (0xffeef3f9), 0, 0, juce::Colour (0xffa9bbcf), b.getRight(), b.getBottom(), false);
        cg.addColour (0.5, juce::Colour (0xffc8d5e3));
        g.setGradientFill (cg); g.fillAll();
        auto orb = [&] (float x, float y, float r, juce::Colour c) { g.setGradientFill (juce::ColourGradient (c, x, y, c.withAlpha (0.0f), x + r, y, true)); g.fillEllipse (x - r, y - r, r * 2, r * 2); };
        orb (120, 120, 380, juce::Colour (0x557fe8ff)); orb (1000, 200, 360, juce::Colour (0x40e030ff)); orb (720, 690, 420, juce::Colour (0x402f7dff));
        g.setColour (navy.withAlpha (0.05f));
        for (int x = 0; x < W; x += 24) g.drawVerticalLine (x, 0, (float) H);
        for (int y = 0; y < H; y += 24) g.drawHorizontalLine (y, 0, (float) W);
    }
    // top bar
    {
        auto bar = juce::Rectangle<float> (0, 0, (float) W, 40);
        chromeFill (g, bar, 0, 0.75f);
        g.setColour (steelLine); g.drawHorizontalLine (40, 0, (float) W);
        g.setFont (disp (24.0f));
        juce::ColourGradient lg (juce::Colours::white, 0, 6, juce::Colour (0xffd9e6f4), 0, 34, false);
        lg.addColour (0.48, juce::Colour (0xff7e8da2)); lg.addColour (0.52, juce::Colour (0xff18243a)); lg.addColour (0.7, juce::Colour (0xff5f7594));
        {
            const juce::Image logo = gAssets != nullptr ? gAssets->logo : juce::Image();
            if (logo.isValid())
            {
                const juce::Rectangle<float> lr (9, 4, 32, 32);
                g.setColour (navy.withAlpha (0.85f));
                g.drawImage (logo, lr.translated (0, 1.4f), juce::RectanglePlacement::centred, true);
                g.setGradientFill (lg);
                g.drawImage (logo, lr, juce::RectanglePlacement::centred, true);
            }
        }
        juce::GlyphArrangement ga; ga.addLineOfText (disp (24.0f), "VOX", 48, 30);
        juce::GlyphArrangement gb; gb.addLineOfText (disp (24.0f), "WARP", 48 + ga.getBoundingBox (0, -1, true).getWidth() + 2, 30);
        juce::Path pa, pb; ga.createPath (pa); gb.createPath (pb);
        g.setColour (navy); g.strokePath (pa, juce::PathStrokeType (1.6f)); g.strokePath (pb, juce::PathStrokeType (1.6f));
        g.setGradientFill (lg); g.fillPath (pa);
        juce::ColourGradient wg (juce::Colour (0xffe8fbff), 0, 6, juce::Colour (0xff9fe6ff), 0, 34, false);
        wg.addColour (0.48, juce::Colour (0xff8fe9ff)); wg.addColour (0.52, juce::Colour (0xff1f7dff)); wg.addColour (0.75, juce::Colour (0xff1658d6));
        g.setGradientFill (wg); g.fillPath (pb);
        g.setColour (juce::Colour (0xff2c3e5c)); g.setFont (mono (8.5f));
        g.drawText ("VOCAL MIXER FX", 232, 13, 96, 16, juce::Justification::centredLeft);
        g.setFont (bold (9.5f)); g.drawText ("PRESET", 330, 13, 46, 16, juce::Justification::centredRight);
        if (ed.linkMode >= 0)
        {
            g.setColour (modCol[ed.linkMode]); g.setFont (bold (11.0f));
            g.drawText ("LINKING MOD " + juce::String::charToString ((juce::juce_wchar) ('A' + ed.linkMode)) + ": TAP A KNOB TO CONNECT, DRAG TO SET DEPTH", 652, 12, 350, 18, juce::Justification::centredRight);
        }
        else { g.setColour (juce::Colour (0xff2c3e5c)); g.setFont (mono (8.5f)); g.drawText (U8 ("TELL YOUR FRIENDS \xc2\xb7 v1.0 build ") + juce::String (VOXWARP_BUILD), 860, 13, 230, 16, juce::Justification::centredRight); }
    }
    // pods
    auto pod = [&] (juce::Rectangle<int> ri, const juce::String& title, const juce::String& sub, juce::Colour c)
    {
        auto r = ri.toFloat();
        g.setColour (juce::Colours::black.withAlpha (0.18f)); g.fillRoundedRectangle (r.translated (0, 3), 14);
        chromeFill (g, r, 14, 0.94f);
        juce::Path hdr; hdr.addRoundedRectangle (r.getX(), r.getY(), r.getWidth(), 26, 14, 14, true, true, false, false);
        g.saveState(); g.reduceClipRegion (hdr); gel (g, r.withHeight (26), c, 0); g.restoreState();
        g.setColour (steelLine); g.drawHorizontalLine ((int) r.getY() + 26, r.getX(), r.getRight());
        g.drawRoundedRectangle (r.reduced (0.5f), 14, 1.3f);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.setFont (disp (12.0f));
        g.drawText (title, (int) r.getX() + 13, (int) r.getY() + 5, 160, 18, juce::Justification::centredLeft);
        g.setColour (juce::Colours::white); g.drawText (title, (int) r.getX() + 12, (int) r.getY() + 4, 160, 18, juce::Justification::centredLeft);
        g.setFont (mono (8.0f)); g.drawText (sub.toUpperCase(), (int) r.getRight() - 212, (int) r.getY() + 4, 200, 18, juce::Justification::centredRight);
    };
    pod (podTune, "TUNE", U8 ("snap \xc2\xb7 formant \xc2\xb7 key"), ui::cTune);
    pod (podHarm, "HARMONY", U8 ("2 voices \xc2\xb7 in key"), ui::cBody);
    pod (podShat, "SHATTER", U8 ("harmony cloud \xc2\xb7 stutter"), ui::cShat);
    pod (podRot, "ROT", U8 ("crush \xc2\xb7 decimate \xc2\xb7 fold"), ui::cRot);
    // key row labels
    g.setColour (navy); g.setFont (bold (10.0f));
    const int ky = podTune.getY() + 34 + 86 + 28;
    g.drawText ("KEY", podTune.getX() + 96, ky, 30, 22, juce::Justification::centredRight);
    g.drawText ("SCALE", podTune.getX() + 212, ky, 40, 22, juce::Justification::centredRight);
    // master pill
    {
        auto r = juce::Rectangle<float> (490, 362, 252, 78);
        g.setColour (juce::Colours::black.withAlpha (0.15f)); g.fillRoundedRectangle (r.translated (0, 3), 20);
        chromeFill (g, r, 20); g.setColour (steelLine); g.drawRoundedRectangle (r.reduced (0.5f), 20, 1.3f);
    }
    // mod strip backing
    {
        auto r = juce::Rectangle<float> (4, 438, (float) W - 8, 258);
        juce::ColourGradient cg (juce::Colour (0xff4a5669), 0, r.getY(), juce::Colour (0xff2f3a4d), 0, r.getBottom(), false);
        cg.addColour (0.48, juce::Colour (0xff2b3546)); cg.addColour (0.52, juce::Colour (0xff1d2533));
        g.setGradientFill (cg); g.fillRoundedRectangle (r, 14);
        g.setColour (juce::Colour (0xff0f1828)); g.drawRoundedRectangle (r.reduced (0.5f), 14, 1.2f);
    }
}
