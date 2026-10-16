// Offline harness: runs audio through the real processor and renders the real editor.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <iostream>

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    VoxProcessor p;
    const double sr = 48000; const int bs = 256;
    p.setPlayConfigDetails (2, 2, sr, bs);
    p.prepareToPlay (sr, bs);
    std::cout << "latency " << p.getLatencySamples() << "\n";
    juce::AudioBuffer<float> b (2, bs); juce::MidiBuffer mid;
    double ph = 0; float peak = 0; int nan = 0; float gate = 1.0f;
    auto run = [&] (double hz, double secs)
    {
        for (int k = 0; k < (int) (secs * sr / bs); ++k)
        {
            for (int s = 0; s < bs; ++s) { ph += 2 * juce::MathConstants<double>::pi * hz / sr; const float v = gate * 0.25f * (float) (std::sin (ph) + 0.5 * std::sin (2 * ph) + 0.25 * std::sin (3 * ph)); b.setSample (0, s, v); b.setSample (1, s, v); }
            p.processBlock (b, mid);
            for (int c = 0; c < 2; ++c) for (int s = 0; s < bs; ++s) { const float v = b.getSample (c, s); if (! std::isfinite (v)) ++nan; peak = std::max (peak, std::abs (v)); }
        }
    };
    run (226, 2.0);
    std::cout << "hard tune 226Hz: in " << p.teleIn.load() << " out " << p.teleOut.load() << " (expect 57)  peak " << peak << " nan " << nan << "\n";
    for (int i = 0; i < (int) VoxProcessor::presets().size(); ++i)
    {
        p.applyPreset (i); peak = 0; run (200 + i * 17, 1.0);
        std::cout << "preset " << VoxProcessor::presets()[(size_t) i].name << " peak " << peak << " nan " << nan << "\n";
    }
    // state round trip
    p.applyPreset (9); juce::MemoryBlock mb; p.getStateInformation (mb);
    VoxProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
    int links = 0; for (auto& L : q.links) if (L.slot.load() >= 0) ++links;
    std::cout << "state restore: links " << links << " (expect 3) preset " << q.currentPresetAtomic.load() << "\n";

    // ---- keyframe envelopes ----
    {
        VoxProcessor k;
        k.setPoints (0, { { 0, 0, 0 }, { 1, 1, 0 } });
        std::cout << "linear mid " << k.shapes[0][128].load() << " end " << k.shapes[0][255].load() << " (expect ~0.5, 1)\n";
        k.setPoints (0, { { 0, 0, 0.8f }, { 1, 1, 0 } });
        std::cout << "curved mid " << k.shapes[0][128].load() << " (expect < 0.5)\n";
        k.setPoints (1, { { 0, 0.2f, 0 }, { 0.3f, 0.9f, -0.5f }, { 0.7f, 0.1f, 0.3f }, { 1, 0.6f, 0 } });
        juce::MemoryBlock m; k.getStateInformation (m);
        VoxProcessor k2; k2.setStateInformation (m.getData(), (int) m.getSize());
        auto a = k.getPoints (1), b2 = k2.getPoints (1); bool same = a.size() == b2.size();
        for (size_t i = 0; same && i < a.size(); ++i) same = std::abs (a[i].x - b2[i].x) < 1e-3f && std::abs (a[i].y - b2[i].y) < 1e-3f && std::abs (a[i].curve - b2[i].curve) < 1e-3f;
        std::cout << "points state round trip: " << (same ? "OK" : "MISMATCH") << " (" << b2.size() << " pts)\n";
        // old project format (64 freehand values)
        auto st = k.apvts.copyState(); juce::ValueTree ex ("EXTRA"); juce::StringArray v; for (int i = 0; i < 64; ++i) v.add (juce::String (i / 63.0f, 3));
        ex.setProperty ("shape2", v.joinIntoString (","), nullptr); st.appendChild (ex, nullptr);
        juce::MemoryBlock mo; k.copyXmlToBinary (*st.createXml(), mo);
        VoxProcessor k3; k3.setStateInformation (mo.getData(), (int) mo.getSize());
        std::cout << "old drawing -> " << k3.getPoints (2).size() << " keyframes, mid " << k3.shapes[2][128].load() << "\n";
    }
    // editor render
    p.applyPreset (juce::SystemStats::getEnvironmentVariable ("VWP", "6").getIntValue());
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (VoxEditor::W, VoxEditor::H);
    auto* ve = dynamic_cast<VoxEditor*> (ed.get());
    juce::Thread::sleep (1);
    {   // simulated mouse editing on MOD B's canvas
        std::function<ui::ModCanvas* (juce::Component*)> find = [&] (juce::Component* c) -> ui::ModCanvas* {
            for (auto* ch : c->getChildren()) { if (auto* mc = dynamic_cast<ui::ModCanvas*> (ch)) { if (mc->getParentComponent()->getX() > 300 && mc->getParentComponent()->getX() < 600) return mc; } if (auto* r = find (ch)) return r; }
            return nullptr; };
        auto* mc = find (ed.get());
        auto src = juce::Desktop::getInstance().getMainMouseSource();
        auto ev = [&] (juce::Point<float> pos, juce::Point<float> down, int clicks, juce::ModifierKeys mods = juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier)) {
            return juce::MouseEvent (src, pos, mods, 1.0f, 0, 0, 0, 0, mc, mc, juce::Time::getCurrentTime(), down, juce::Time::getCurrentTime(), clicks, pos != down); };
        p.setPoints (1, { { 0, 0, 0 }, { 1, 0, 0 } });
        const float W = (float) mc->getWidth(), H = (float) mc->getHeight();
        const juce::Point<float> c1 (7 + (W - 14 - 14) * 0.5f, 6 + (H - 12) * 0.2f);   // x=0.5, y=0.8
        mc->mouseDown (ev (c1, c1, 1)); mc->mouseUp (ev (c1, c1, 1));
        auto pt = p.getPoints (1);
        std::cout << "click adds point: " << pt.size() << " pts, new at x=" << pt[1].x << " y=" << pt[1].y << "\n";
        const juce::Point<float> c2 (7 + (W - 28) * 0.3f, 6 + (H - 12) * 0.5f);
        mc->mouseDown (ev (c1, c1, 1)); mc->mouseDrag (ev (c2, c1, 1)); mc->mouseUp (ev (c2, c1, 1));
        pt = p.getPoints (1);
        std::cout << "drag moves it to x=" << pt[1].x << " y=" << pt[1].y << " (expect ~0.3, 0.5)\n";
        // bend: handle of segment 0 sits at its midpoint
        const float hx = 7 + (W - 28) * (pt[0].x + pt[1].x) * 0.5f, hy = 6 + (H - 12) * (1 - (pt[0].y + pt[1].y) * 0.5f);
        const juce::Point<float> h1 (hx, hy), h2 (hx, hy - 30);
        mc->mouseDown (ev (h1, h1, 1)); mc->mouseDrag (ev (h2, h1, 1)); mc->mouseUp (ev (h2, h1, 1));
        pt = p.getPoints (1);
        std::cout << "drag diamond up bends segment: curve=" << pt[0].curve << " (expect < 0 = bows up)\n";
        const juce::Point<float> c3 (7 + (W - 28) * pt[1].x, 6 + (H - 12) * (1 - pt[1].y));
        mc->mouseDoubleClick (ev (c3, c3, 2));
        std::cout << "double-click deletes: " << p.getPoints (1).size() << " pts (expect 2)\n";
        auto snap2 = mc->createComponentSnapshot (mc->getLocalBounds(), true, 2.0f);
        p.setPoints (1, { { 0, 0, -0.5f }, { 0.12f, 1, 0.55f }, { 0.45f, 0.35f, 0 }, { 0.6f, 0.8f, -0.6f }, { 1, 0, 0 } });
    }
    for (int f = 0; f < 540; ++f) { gate = (f % 18) < 11 ? 1.0f : 0.05f; run (180 + 40 * std::sin (f * 0.1), 1.0 / 60); juce::Thread::sleep (16); ve->tickForTest();
        if (f % 30 == 29) { auto im = ed->createComponentSnapshot ({ 394, 46, 312, 312 }, true, 1.0f); juce::File fo ("/tmp/claude-0/fr" + juce::String (f / 30) + ".png"); fo.deleteFile(); juce::FileOutputStream os2 (fo); juce::PNGImageFormat().writeImageToStream (im, os2); } }
    juce::ignoreUnused (ve);
    auto snap = [&] (const char* path) { auto im = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f); juce::File f (path); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (im, os); };
    snap ("/tmp/claude-0/editor.png");
    for (int f = 0; f < 200; ++f) { b.clear(); p.processBlock (b, mid); juce::Thread::sleep (16); ve->tickForTest(); }
    snap ("/tmp/claude-0/editor_idle.png");
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f ("/tmp/claude-0/editor_last.png"); f.deleteFile();
    juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
    std::cout << "snapshot written\n";
    ed.reset();
    return 0;
}
