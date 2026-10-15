#pragma once
#include <juce_core/juce_core.h>
#include <cmath>

// Every knob is a 0..1 parameter; mapping to real units happens here,
// the same way the browser prototype did it.
namespace vw
{
enum Group { gTune, gHarm, gShat, gRot, gMaster, gMod };

struct PDef { const char* id; const char* name; float def; Group grp; bool linkable; };

inline const std::vector<PDef>& knobDefs()
{
    static const std::vector<PDef> d {
        { "snap",    "SNAP",     1.0f,        gTune,   true },
        { "shift",   "SHIFT",    0.5f,        gTune,   true },
        { "formant", "FORMANT",  0.5f,        gTune,   true },
        { "throat",  "THROAT",   1.0f / 3.0f, gTune,   true },
        { "width",   "WIDTH",    0.0f,        gTune,   true },
        { "h1",      "VOICE 1",  0.6f,        gHarm,   true },
        { "h2",      "VOICE 2",  0.8f,        gHarm,   true },
        { "hblend",  "BLEND",    0.0f,        gHarm,   true },
        { "hspread", "SPREAD",   0.6f,        gHarm,   true },
        { "hhuman",  "HUMAN",    0.3f,        gHarm,   true },
        { "spray",   "SPRAY",    0.0f,        gShat,   true },
        { "scatter", "SCATTER",  0.0f,        gShat,   true },
        { "stutter", "STUTTER",  0.0f,        gShat,   true },
        { "reverse", "REVERSE",  0.0f,        gShat,   true },
        { "crush",   "CRUSH",    0.0f,        gRot,    true },
        { "decim",   "DECIMATE", 0.0f,        gRot,    true },
        { "fold",    "FOLD",     0.0f,        gRot,    true },
        { "react",   "REACT",    0.6f,        gMaster, false },
        { "mix",     "MIX",      1.0f,        gMaster, true },
        { "out",     "OUTPUT",   0.5f,        gMaster, true },
        { "rate0",   "LENGTH",   0.53f,       gMod,    false },
        { "sens0",   "SENS",     0.5f,        gMod,    false },
        { "rate1",   "LENGTH",   0.62f,       gMod,    false },
        { "sens1",   "SENS",     0.5f,        gMod,    false },
        { "rate2",   "LENGTH",   0.60f,       gMod,    false },
        { "sens2",   "SENS",     0.5f,        gMod,    false },
    };
    return d;
}
inline int knobIndex (const juce::String& id)
{
    const auto& d = knobDefs();
    for (int i = 0; i < (int) d.size(); ++i) if (id == d[(size_t) i].id) return i;
    return -1;
}
inline int numKnobs() { return (int) knobDefs().size(); }

inline const juce::StringArray& keyNames()   { static juce::StringArray k { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" }; return k; }
inline const juce::StringArray& scaleNames() { static juce::StringArray s { "OFF","CHROM","MAJOR","MINOR","PENTA","HIJAZ" }; return s; }
inline const juce::StringArray& modeNames()  { static juce::StringArray m { "TRIG","SYNC","FREE" }; return m; }
inline const juce::StringArray& harmNames()  { static juce::StringArray h { "OFF","-OCT","-6TH","-5TH","-4TH","-3RD","+3RD","+4TH","+5TH","+6TH","+OCT" }; return h; }
inline const juce::StringArray& divNames()   { static juce::StringArray d { "1/32","1/16","1/8T","1/8","1/4T","1/4","1/2","1 BAR","2 BAR","4 BAR" }; return d; }
inline double divBeats (int i) { static const double b[] { 0.125, 0.25, 1.0/3.0, 0.5, 2.0/3.0, 1, 2, 4, 8, 16 }; return b[juce::jlimit (0, 9, i)]; }

inline float trigLen (float v)  { return 0.05f * std::pow (80.0f, v); }
inline float sensGain (float v) { return 0.5f * std::pow (60.0f, v); }
inline float freeRate (float v) { return 0.05f * std::pow (200.0f, v); }
inline float reactGain (float v){ return 0.5f * std::pow (8.0f, v); }

inline juce::String pct (float v) { return juce::String (juce::roundToInt (v * 100)) + "%"; }

// Text for a knob value (mode is only used by the mod rate knobs).
inline juce::String formatKnob (const juce::String& id, float v, int mode = 0)
{
    if (id == "snap")    return v > 0.985f ? "ROBOT" : juce::String (juce::roundToInt (400 * std::pow (1 - v, 2.0f))) + " ms";
    if (id == "shift")   { const int s = juce::roundToInt (v * 48 - 24); return (s > 0 ? "+" : "") + juce::String (s) + " st"; }
    if (id == "formant") { const float f = v * 24 - 12; return (f > 0 ? "+" : "") + juce::String (f, 1) + " st"; }
    if (id == "throat")  return juce::String::fromUTF8 ("\xc3\x97") + juce::String (0.5f * std::pow (8.0f, v), 2);
    if (id == "h1" || id == "h2") return harmNames()[juce::roundToInt (v * 10)];
    if (id == "hblend" || id == "stutter" || id == "reverse") return v < 0.01f ? "OFF" : pct (v);
    if (id == "spray")   return v < 0.01f ? "OFF" : juce::String (juce::roundToInt (v * 500)) + " ms";
    if (id == "scatter") { static const char* n[] { "+UNISON","+OCT UP","+OCT DN","+5TH","+12TH","+4TH DN","+2 OCT","+4TH" }; return v < 0.01f ? "OFF" : n[juce::jlimit (0, 7, (int) (v * 7.99f))]; }
    if (id == "crush")   return v < 0.01f ? "OFF" : juce::String (16 - v * 14, 1) + " bit";
    if (id == "decim")   return v < 0.01f ? "OFF" : juce::String::fromUTF8 ("\xc3\xb7") + juce::String (1 + v * v * 40, 1);
    if (id == "react")   return juce::String::fromUTF8 ("\xc3\x97") + juce::String (reactGain (v), 1);
    if (id == "out")     { const float g = v * 2; return g < 0.001f ? "-inf dB" : juce::String (20 * std::log10 (g), 1) + " dB"; }
    if (id.startsWith ("rate"))
    {
        if (mode == 1) return divNames()[juce::roundToInt (v * 9)];
        if (mode == 0) { const float l = trigLen (v); return l < 1 ? juce::String (juce::roundToInt (l * 1000)) + " ms" : juce::String (l, 2) + " s"; }
        const float r = freeRate (v); return juce::String (r, r < 1 ? 2 : 1) + " Hz";
    }
    if (id.startsWith ("sens")) return (mode != 0 && v < 0.01f) ? juce::String ("OFF") : juce::String::fromUTF8 ("\xc3\x97") + juce::String (sensGain (v), 1);
    return pct (v);
}
}
