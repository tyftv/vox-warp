#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PluginProcessor.h"
#include "Liquid.h"

namespace ui
{
struct Assets
{
    Assets(); ~Assets();
    juce::Typeface::Ptr disp, bold;
    juce::Image logo;
};
juce::Font disp (float h);   // Michroma
juce::Font bold (float h);   // Chakra Petch Bold
juce::Font mono (float h);

struct Y2KLook : juce::LookAndFeel_V4
{
    Y2KLook();
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return bold (11.0f); }
};

class VoxEditorBase;

// Chrome knob bound to one 0..1 parameter. In link mode, clicks connect/disconnect and drags set mod depth.
class Knob : public juce::Component
{
public:
    Knob (VoxProcessor&, int knobIndex, float dial, std::function<int()> linkMode, std::function<int()> modeForFmt = {});
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool hasLinks() const;
    juce::String labelOverride;
    bool dark = false;
private:
    VoxProcessor& proc; int index; float dial;
    std::function<int()> linkMode, modeForFmt;
    juce::RangedAudioParameter* param;
    juce::ParameterAttachment att;
    float startV = 0; int startY = 0; bool moved = false, linking = false;
    juce::String dragText;
};

class ModCanvas : public juce::Component
{
public:
    ModCanvas (VoxProcessor& p, int s) : proc (p), slot (s) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override { last = -1; }
    int lastTrig = 0; float flash = 0;
private:
    void put (juce::Point<float>);
    VoxProcessor& proc; int slot; int last = -1;
};

class ModPanel : public juce::Component
{
public:
    ModPanel (VoxProcessor&, int slot, std::function<int()> linkMode, std::function<void (int)> setLinkMode);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();
    ModCanvas canvas;
    std::unique_ptr<Knob> rate, sens;
private:
    VoxProcessor& proc; int slot;
    std::function<int()> linkMode; std::function<void (int)> setLinkMode;
    juce::TextButton modeBtn, linkBtn;
    juce::OwnedArray<juce::TextButton> shapeBtns;
    juce::String linkText;
};

class Visualizer : public juce::Component
{
public:
    explicit Visualizer (VoxProcessor& p);
    void tick (float dt);
    void paint (juce::Graphics&) override;
private:
    VoxProcessor& proc;
    juce::dsp::FFT fft { 11 };
    std::array<float, 4096> fftBuf {};
    std::array<float, 2048> td {};
    static constexpr int NB = 36;
    std::array<float, NB> bands {};
    juce::Image A, B, tiny, liq;
    Liquid lq;
    float t = 0, rot = 0, pk = 0.05f, env = 0, la = 0, lvl = 0, kickCool = 0, dripCool = 0; bool kickArmed = true, hitArmed = true;
    bool running = false;
    std::mt19937 rng { 77 }; std::uniform_real_distribution<float> uni { 0.0f, 1.0f };
    float E (const char* id) const { return proc.effNorm[(size_t) vw::knobIndex (id)].load(); }
};

class Readout : public juce::Component
{
public:
    explicit Readout (VoxProcessor& p) : proc (p) {}
    void paint (juce::Graphics&) override;
private:
    VoxProcessor& proc;
};
}

class VoxEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int W = 1100, H = 700;
    explicit VoxEditor (VoxProcessor&);
    ~VoxEditor() override;
    void paint (juce::Graphics&) override {}
    void resized() override;
    void tickForTest() { timerCallback(); }
private:
    void timerCallback() override;
    void setLinkMode (int m);
    void refreshPreset();

    juce::SharedResourcePointer<ui::Assets> assets;   // first member: built before, released after everything that draws

    struct Root : juce::Component
    {
        explicit Root (VoxEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        VoxEditor& ed;
    } root { *this };

    VoxProcessor& proc;
    ui::Y2KLook look;
    int linkMode = -1;
    juce::OwnedArray<ui::Knob> knobs;
    std::unique_ptr<ui::ModPanel> mods[3];
    ui::Visualizer viz;
    ui::Readout readout;
    juce::TextButton prevBtn { juce::String::fromUTF8 ("\xe2\x97\x80") }, nextBtn { juce::String::fromUTF8 ("\xe2\x96\xb6") }, presetBtn;
    juce::TextButton keyDn { juce::String::fromUTF8 ("\xe2\x97\x80") }, keyUp { juce::String::fromUTF8 ("\xe2\x96\xb6") }, scaleBtn, doneBtn { "DONE" };
    juce::Label keyLcd;
    int tickCount = 0; double lastTime = 0;
    friend struct Root;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxEditor)
};
