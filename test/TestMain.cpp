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
    double ph = 0; float peak = 0; int nan = 0;
    auto run = [&] (double hz, double secs)
    {
        for (int k = 0; k < (int) (secs * sr / bs); ++k)
        {
            for (int s = 0; s < bs; ++s) { ph += 2 * juce::MathConstants<double>::pi * hz / sr; const float v = 0.25f * (float) (std::sin (ph) + 0.5 * std::sin (2 * ph) + 0.25 * std::sin (3 * ph)); b.setSample (0, s, v); b.setSample (1, s, v); }
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

    // editor render
    p.applyPreset (juce::SystemStats::getEnvironmentVariable ("VWP", "6").getIntValue());
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (VoxEditor::W, VoxEditor::H);
    auto* ve = dynamic_cast<VoxEditor*> (ed.get());
    juce::Thread::sleep (1);
    for (int f = 0; f < 150; ++f) { run (180 + 40 * std::sin (f * 0.1), 1.0 / 60); for (int c = 0; c < 2; ++c) b.applyGain (c, 0, bs, (f % 20) < 10 ? 1.0f : 0.0f); juce::Thread::sleep (16); ve->tickForTest();
        if (f % 12 == 11) { auto im = ed->createComponentSnapshot ({ 394, 46, 312, 312 }, true, 1.0f); juce::File fo ("/tmp/claude-0/fr" + juce::String (f / 12) + ".png"); fo.deleteFile(); juce::FileOutputStream os2 (fo); juce::PNGImageFormat().writeImageToStream (im, os2); } }
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
