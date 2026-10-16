#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace vw;

juce::AudioProcessorValueTreeState::ParameterLayout VoxProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (const auto& d : knobDefs())
    {
        const juce::String id (d.id);
        auto attrs = juce::AudioParameterFloatAttributes().withStringFromValueFunction ([id] (float v, int) { return formatKnob (id, v, id.startsWith ("rate") ? 0 : 0); });
        juce::String name (d.name);
        if (id.startsWith ("rate") || id.startsWith ("sens")) name = "Mod " + juce::String::charToString ((juce::juce_wchar) ('A' + id.getTrailingIntValue())) + " " + (id.startsWith ("rate") ? "Rate" : "Sens");
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 1.0f), d.def, attrs));
    }
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "key", 1 }, "Key", keyNames(), 0));
    l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "scale", 1 }, "Scale", scaleNames(), 1));
    for (int i = 0; i < 3; ++i)
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { "mode" + juce::String (i), 1 }, "Mod " + juce::String::charToString ((juce::juce_wchar) ('A' + i)) + " Mode", modeNames(), 0));
    return l;
}

VoxProcessor::VoxProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VOXWARP", createLayout())
{
    for (int i = 0; i < numKnobs(); ++i) knobParams[(size_t) i] = apvts.getParameter (knobDefs()[(size_t) i].id);
    keyParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("key"));
    scaleParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("scale"));
    for (int i = 0; i < 3; ++i) modeParams[(size_t) i] = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("mode" + juce::String (i)));
    for (auto& e : effNorm) e = 0;
    for (int i = 0; i < 3; ++i) { modVal[(size_t) i] = 0; modPhase[(size_t) i] = 0; modEnv[(size_t) i] = 0; modTrigCount[(size_t) i] = 0; }
    shapeFill (0, "SIN"); shapeFill (1, "SAW"); shapeFill (2, "RND");
}

bool VoxProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void VoxProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; sampleRateAtomic = sampleRate;
    engine.prepare (sampleRate);
    mono.setSize (1, samplesPerBlock); wetL.setSize (1, samplesPerBlock); wetR.setSize (1, samplesPerBlock);
    latency = juce::roundToInt (512.0 * sampleRate / 48000.0);
    setLatencySamples (latency);
    dryDelay.assign ((size_t) (latency + 1) * 2, 0.0f); dryW = 0;
    outGain.reset (sampleRate, 0.03); mixS.reset (sampleRate, 0.03);
    outGain.setCurrentAndTargetValue (knobParams[(size_t) knobIndex ("out")]->getValue() * 2.0f);
    mixS.setCurrentAndTargetValue (knobParams[(size_t) knobIndex ("mix")]->getValue());
}

void VoxProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels(), nOut = getTotalNumOutputChannels();
    if (mono.getNumSamples() < n) { mono.setSize (1, n, false, false, true); wetL.setSize (1, n, false, false, true); wetR.setSize (1, n, false, false, true); }

    // mono input + level for the mods
    float* m = mono.getWritePointer (0);
    float e = 0;
    for (int s = 0; s < n; ++s)
    {
        float v = 0; for (int c = 0; c < nIn; ++c) v += buffer.getReadPointer (c)[s];
        v = nIn > 0 ? v / (float) nIn : 0; m[s] = v; e += v * v;
    }
    const float inLvl = n > 0 ? std::sqrt (e / (float) n) : 0.0f;
    if (inLvl > inPeak.load()) inPeak = inLvl;
    const float dt = (float) n / (float) sr;

    // host tempo for SYNC mods
    double bpm = 120, ppq = 0; bool hasPpq = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) { ppq = *q; hasPpq = true; }
        }
    internalBeats += dt * bpm / 60.0;
    const double beats = hasPpq ? ppq : internalBeats;

    // ---- mods ----
    float vals[3];
    for (int i = 0; i < 3; ++i)
    {
        auto& S = ms[(size_t) i];
        const int mode = modeParams[(size_t) i]->getIndex();
        const float rv = knobParams[(size_t) knobIndex ("rate" + juce::String (i))]->getValue();
        const float sv = knobParams[(size_t) knobIndex ("sens" + juce::String (i))]->getValue();
        S.env = inLvl * sensGain (sv);
        const float up = 1 - std::pow (0.5f, dt * 60), dn = 1 - std::pow (0.92f, dt * 60);
        S.envS += (S.env - S.envS) * (S.env > S.envS ? up : dn);
        if (mode == 2) S.phase = std::fmod (S.phase + dt * freeRate (rv), 1.0f);
        else if (mode == 1) S.phase = (float) std::fmod (beats / divBeats (juce::roundToInt (rv * 9)), 1.0);
        else
        {
            if (S.armed && S.env > 0.25f) { S.armed = false; S.playing = true; S.phase = 0; ++modTrigCount[(size_t) i]; }
            else if (! S.armed && S.env < 0.12f) S.armed = true;
            if (S.playing) { S.phase += dt / trigLen (rv); if (S.phase >= 1) { S.phase = 0.9999f; S.playing = false; } }
        }
        if (S.phase < 0) S.phase += 1;
        const float x = S.phase * 64; const int i0 = juce::jmin (63, (int) x);
        const int i1 = mode == 0 ? juce::jmin (63, i0 + 1) : (i0 + 1) % 64;
        const float f = x - std::floor (x);
        float v = shapes[(size_t) i][(size_t) i0] + (shapes[(size_t) i][(size_t) i1] - shapes[(size_t) i][(size_t) i0]) * f;
        if (mode != 0 && sv >= 0.01f) v *= juce::jmin (1.0f, S.envS * 2);
        vals[i] = v;
        modVal[(size_t) i] = v; modPhase[(size_t) i] = S.phase; modEnv[(size_t) i] = S.env;
    }

    // ---- effective (modulated) values ----
    float E[32];
    for (int k = 0; k < numKnobs(); ++k)
    {
        float v = knobParams[(size_t) k]->getValue();
        if (knobDefs()[(size_t) k].linkable)
            for (auto& L : links) { const int sl = L.slot.load(); if (sl >= 0 && L.param.load() == k) v += L.depth.load() * vals[sl]; }
        E[k] = juce::jlimit (0.0f, 1.0f, v);
        effNorm[(size_t) k] = E[k];
    }
    auto ev = [&] (const char* id) { return E[knobIndex (id)]; };

    auto& p = engine.p;
    p.snap = ev ("snap"); p.shift = (float) juce::roundToInt (ev ("shift") * 48 - 24); p.formant = ev ("formant") * 24 - 12;
    p.throat = 0.5f * std::pow (8.0f, ev ("throat")); p.width = ev ("width");
    p.h1 = juce::roundToInt (ev ("h1") * 10); p.h2 = juce::roundToInt (ev ("h2") * 10);
    p.hblend = ev ("hblend"); p.hspread = ev ("hspread"); p.hhuman = ev ("hhuman");
    p.spray = ev ("spray"); p.scatter = ev ("scatter"); p.stutter = ev ("stutter"); p.reverse = ev ("reverse");
    p.crush = ev ("crush") < 0.01f ? 16.0f : 16.0f - ev ("crush") * 14.0f;
    p.decim = 1.0f + ev ("decim") * ev ("decim") * 40.0f; p.fold = ev ("fold");
    p.key = keyParam->getIndex(); p.scale = scaleParam->getIndex();
    mixS.setTargetValue (ev ("mix")); outGain.setTargetValue (ev ("out") * 2.0f);

    engine.process (m, wetL.getWritePointer (0), wetR.getWritePointer (0), n);
    teleIn = engine.inMidi; teleOut = engine.outMidi; teleRms = engine.rms; teleVoiced = engine.voiced; teleStut = engine.stut; teleGrains = engine.grains;

    // ---- latency-matched dry + mix + output ----
    const float* wl = wetL.getReadPointer (0); const float* wr = wetR.getReadPointer (0);
    const int D = latency + 1;
    float* o0 = nOut > 0 ? buffer.getWritePointer (0) : nullptr;
    float* o1 = nOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* i0p = nIn > 0 ? buffer.getReadPointer (0) : nullptr;
    const float* i1p = nIn > 1 ? buffer.getReadPointer (1) : i0p;
    int vw = vizWrite.load(); float oe = 0;
    for (int s = 0; s < n; ++s)
    {
        const float dl0 = i0p ? i0p[s] : 0.0f, dl1 = i1p ? i1p[s] : 0.0f;
        dryDelay[(size_t) dryW * 2] = dl0; dryDelay[(size_t) dryW * 2 + 1] = dl1;
        const int rdI = (dryW + 1) % D;
        const float d0 = dryDelay[(size_t) rdI * 2], d1 = dryDelay[(size_t) rdI * 2 + 1];
        dryW = rdI;
        const float mx = mixS.getNextValue(), g = outGain.getNextValue();
        const float L = std::tanh ((d0 * (1 - mx) + wl[s] * mx) * g);
        const float R = std::tanh ((d1 * (1 - mx) + wr[s] * mx) * g);
        if (o1) { o0[s] = L; o1[s] = R; }
        else if (o0) o0[s] = 0.5f * (L + R);
        const float mo = 0.5f * (L + R); oe += mo * mo;
        vizBuf[(size_t) vw] = mo; vw = (vw + 1) & (vizSize - 1);
    }
    vizWrite = vw;
    outLevel = n > 0 ? std::sqrt (oe / (float) n) : 0.0f;
    for (int c = 2; c < nOut; ++c) buffer.clear (c, 0, n);
}

// ---------------- mod links / shapes ----------------
int VoxProcessor::findLink (int slot, int param) const
{
    for (int i = 0; i < (int) links.size(); ++i) if (links[(size_t) i].slot.load() == slot && links[(size_t) i].param.load() == param) return i;
    return -1;
}
void VoxProcessor::setLink (int slot, int param, float depth)
{
    int i = findLink (slot, param);
    if (i < 0) for (int k = 0; k < (int) links.size(); ++k) if (links[(size_t) k].slot.load() < 0) { i = k; break; }
    if (i < 0) return;
    links[(size_t) i].depth = juce::jlimit (-1.0f, 1.0f, depth);
    links[(size_t) i].param = param; links[(size_t) i].slot = slot;
}
void VoxProcessor::removeLink (int slot, int param)
{
    const int i = findLink (slot, param);
    if (i >= 0) { links[(size_t) i].slot = -1; links[(size_t) i].param = -1; links[(size_t) i].depth = 0; }
}
void VoxProcessor::shapeFill (int slot, const juce::String& kind)
{
    uint32_t s = (uint32_t) (7 + slot * 13);
    for (int k = 0; k < 64; ++k)
    {
        const float x = k / 64.0f; float v = 0;
        if (kind == "SIN") v = 0.5f - 0.5f * std::cos (x * juce::MathConstants<float>::twoPi);
        else if (kind == "SAW") v = 1 - x;
        else if (kind == "SQR") v = x < 0.5f ? 1.0f : 0.0f;
        else if (kind == "RND") { if (k % 8 == 0) s = (uint32_t) (((uint64_t) s * 16807u) % 2147483647u); v = (float) (s % 1000) / 1000.0f; }
        shapes[(size_t) slot][(size_t) k] = v;
    }
}

// ---------------- presets ----------------
const std::vector<Preset>& VoxProcessor::presets()
{
    static const std::vector<Preset> P {
        { "Hard Tune",    {}, 1, {} },
        { "Stack",        { {"hblend",.75f},{"h1",.6f},{"h2",.8f},{"hspread",.7f},{"width",.3f} }, 2, {} },
        { "Robo Pop",     { {"formant",.58f},{"width",.5f},{"crush",.12f} }, 3, { {0,"width",.35f} } },
        { "Chipmunk",     { {"shift",.75f},{"formant",.75f},{"width",.25f} }, 1, {} },
        { "Demon",        { {"shift",.25f},{"formant",.2f},{"hblend",.5f},{"h1",.1f},{"h2",0.0f},{"crush",.1f} }, 1, {} },
        { "Octave Choir", { {"hblend",.8f},{"h1",1.0f},{"h2",.1f},{"hspread",.9f},{"hhuman",.5f},{"spray",.2f} }, 1, {} },
        { "Glass Choir",  { {"formant",.6f},{"width",.9f},{"spray",.25f},{"scatter",.45f} }, 4, { {0,"formant",.18f} } },
        { "Stutter Pop",  { {"stutter",.55f},{"width",.4f} }, 3, { {0,"stutter",.4f} } },
        { "Shattered",    { {"width",.8f},{"spray",.5f},{"scatter",.3f},{"stutter",.6f},{"reverse",.5f},{"decim",.12f},{"fold",.2f} }, 3, { {2,"stutter",.3f} } },
        { "Gone",         { {"formant",.2f},{"throat",.9f},{"width",1.0f},{"spray",.85f},{"scatter",.85f},{"stutter",.8f},{"reverse",.6f},{"crush",.55f},{"decim",.5f},{"fold",.7f} }, 5,
                          { {1,"formant",.55f},{2,"throat",-.45f},{0,"spray",.15f} } },
    };
    return P;
}

void VoxProcessor::applyPreset (int index)
{
    const auto& P = presets();
    if (! juce::isPositiveAndBelow (index, (int) P.size())) return;
    const auto& pr = P[(size_t) index];
    for (int k = 0; k < numKnobs(); ++k)
    {
        const auto& d = knobDefs()[(size_t) k];
        const juce::String id (d.id);
        if (d.grp == gMod || id == "react" || id == "mix" || id == "out") continue;
        knobParams[(size_t) k]->setValueNotifyingHost (d.def);
    }
    for (auto& [id, v] : pr.values) if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (v);
    scaleParam->setValueNotifyingHost (scaleParam->convertTo0to1 ((float) pr.scale));
    for (auto& L : links) { L.slot = -1; L.param = -1; L.depth = 0; }
    for (auto& [slot, id, depth] : pr.links) setLink (slot, knobIndex (id), depth);
    currentPreset = index; currentPresetAtomic = index;
}

// ---------------- state ----------------
void VoxProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::ValueTree extra ("EXTRA");
    for (int i = 0; i < 3; ++i)
    {
        juce::StringArray sa; for (auto& v : shapes[(size_t) i]) sa.add (juce::String (v.load(), 4));
        extra.setProperty ("shape" + juce::String (i), sa.joinIntoString (","), nullptr);
    }
    juce::StringArray ls;
    for (auto& L : links) if (L.slot.load() >= 0) ls.add (juce::String (L.slot.load()) + ":" + knobDefs()[(size_t) L.param.load()].id + ":" + juce::String (L.depth.load(), 4));
    extra.setProperty ("links", ls.joinIntoString (";"), nullptr);
    extra.setProperty ("preset", currentPreset, nullptr);
    state.removeChild (state.getChildWithName ("EXTRA"), nullptr);
    state.appendChild (extra, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void VoxProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;
    auto extra = state.getChildWithName ("EXTRA");
    apvts.replaceState (state);
    if (extra.isValid())
    {
        for (int i = 0; i < 3; ++i)
        {
            juce::StringArray sa; sa.addTokens (extra.getProperty ("shape" + juce::String (i)).toString(), ",", "");
            if (sa.size() == 64) for (int k = 0; k < 64; ++k) shapes[(size_t) i][(size_t) k] = sa[k].getFloatValue();
        }
        for (auto& L : links) { L.slot = -1; L.param = -1; L.depth = 0; }
        juce::StringArray ls; ls.addTokens (extra.getProperty ("links").toString(), ";", "");
        for (auto& s : ls)
        {
            juce::StringArray f; f.addTokens (s, ":", "");
            if (f.size() == 3) { const int pi = knobIndex (f[1]); if (pi >= 0) setLink (f[0].getIntValue(), pi, f[2].getFloatValue()); }
        }
        currentPreset = (int) extra.getProperty ("preset", 0); currentPresetAtomic = currentPreset;
    }
}

juce::AudioProcessorEditor* VoxProcessor::createEditor() { return new VoxEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VoxProcessor(); }
