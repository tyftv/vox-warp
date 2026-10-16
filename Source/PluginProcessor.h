#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Engine.h"
#include "Params.h"

struct ModLink { std::atomic<int> slot { -1 }, param { -1 }; std::atomic<float> depth { 0 }; };

struct Preset { juce::String name; std::vector<std::pair<juce::String, float>> values; int scale; std::vector<std::tuple<int, juce::String, float>> links; };

class VoxProcessor : public juce::AudioProcessor
{
public:
    VoxProcessor();
    ~VoxProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Vox Warp"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.6; }

    int getNumPrograms() override { return (int) presets().size(); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram (int i) override { applyPreset (i); }
    const juce::String getProgramName (int i) override { return juce::isPositiveAndBelow (i, (int) presets().size()) ? presets()[(size_t) i].name : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- shared with the editor ----
    juce::AudioProcessorValueTreeState apvts;
    std::array<juce::RangedAudioParameter*, 32> knobParams {};
    juce::AudioParameterChoice *keyParam = nullptr, *scaleParam = nullptr;
    std::array<juce::AudioParameterChoice*, 3> modeParams {};

    std::array<std::array<std::atomic<float>, 64>, 3> shapes;
    std::array<ModLink, 32> links;
    std::array<std::atomic<float>, 32> effNorm;              // modulated knob values (for ghost dots)
    std::array<std::atomic<float>, 3> modVal, modPhase, modEnv;
    std::array<std::atomic<int>, 3> modTrigCount;
    std::atomic<float> teleIn { 0 }, teleOut { 0 }, teleRms { 0 }, outLevel { 0 };
    std::atomic<float> inPeak { 0 };   // loudest input block since the UI last looked (fluid SENS)
    std::atomic<bool> teleVoiced { false }, teleStut { false };
    std::atomic<int> teleGrains { 0 };

    // visualizer tap (mono output), written by the audio thread
    static constexpr int vizSize = 4096;
    std::array<float, vizSize> vizBuf {};
    std::atomic<int> vizWrite { 0 };
    std::atomic<double> sampleRateAtomic { 48000.0 };

    static const std::vector<Preset>& presets();
    void applyPreset (int index);
    std::atomic<int> currentPresetAtomic { 0 };
    int findLink (int slot, int param) const;
    void setLink (int slot, int param, float depth);
    void removeLink (int slot, int param);
    void shapeFill (int slot, const juce::String& kind);

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    VoxEngine engine;
    juce::AudioBuffer<float> mono, wetL, wetR;
    std::vector<float> dryDelay; int dryW = 0, latency = 0;
    double sr = 48000, internalBeats = 0;
    struct ModState { float phase = 0, env = 0, envS = 0; bool armed = true, playing = false; };
    std::array<ModState, 3> ms;
    juce::SmoothedValue<float> outGain, mixS;
    int currentPreset = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxProcessor)
};
