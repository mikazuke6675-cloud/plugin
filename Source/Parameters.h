#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace afterhours
{

//==============================================================================
// Display helpers
inline juce::String secondsToText (float v, int) { return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s"; }
inline juce::String hzToText (float v, int)      { return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz" : juce::String (v / 1000.0f, 2) + " kHz"; }
inline juce::String pctToText (float v, int)     { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
inline juce::String semiToText (float v, int)    { return (v > 0 ? "+" : "") + juce::String (v, 1) + " st"; }
inline juce::String centsToText (float v, int)   { return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + " ct"; }
inline juce::String dbToText (float v, int)      { return juce::String (v, 1) + " dB"; }
inline juce::String plainToText (float v, int)   { return juce::String (v, 2); }

inline juce::NormalisableRange<float> skewed (float lo, float hi, float centre)
{
    juce::NormalisableRange<float> r (lo, hi);
    r.setSkewForCentre (centre);
    return r;
}

inline const juce::StringArray& waveNames()     { static juce::StringArray s { "Saw", "Square", "Triangle", "Sine" }; return s; }
inline const juce::StringArray& lfoShapeNames() { static juce::StringArray s { "Sine", "Triangle", "Square", "S&H" }; return s; }
inline const juce::StringArray& delayDivNames() { static juce::StringArray s { "1/16", "1/8", "1/8 dot", "1/4", "1/4 dot", "1/2", "1/8 trip", "1/4 trip" }; return s; }
inline float delayDivToBeats (int index)
{
    static const float beats[] { 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 1.0f / 3.0f, 2.0f / 3.0f };
    return beats[juce::jlimit (0, 7, index)];
}

//==============================================================================
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addF = [&] (const char* id, const char* name, NormalisableRange<float> range, float def,
                     std::function<String (float, int)> toText)
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes().withStringFromValueFunction (toText)));
    };
    auto addI = [&] (const char* id, const char* name, int lo, int hi, int def)
    { layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, name, lo, hi, def)); };
    auto addC = [&] (const char* id, const char* name, const StringArray& choices, int def)
    { layout.add (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, choices, def)); };
    auto addB = [&] (const char* id, const char* name, bool def)
    { layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };

    const NormalisableRange<float> unit (0.0f, 1.0f);

    // Oscillator A
    addC ("oscAWave",   "Osc A Wave",    waveNames(), 0);
    addI ("oscAOct",    "Osc A Octave",  -3, 3, 0);
    addI ("oscASemi",   "Osc A Semi",    -12, 12, 0);
    addI ("oscAUnison", "Osc A Unison",  1, 7, 1);
    addF ("oscADetune", "Osc A Detune",  unit, 0.25f, pctToText);
    addF ("oscALevel",  "Osc A Level",   unit, 0.8f, pctToText);

    // Oscillator B / sub / noise
    addC ("oscBWave",   "Osc B Wave",    waveNames(), 0);
    addI ("oscBOct",    "Osc B Octave",  -3, 3, 0);
    addI ("oscBSemi",   "Osc B Semi",    -12, 12, 0);
    addF ("oscBFine",   "Osc B Fine",    NormalisableRange<float> (-50.0f, 50.0f), 0.0f, centsToText);
    addF ("oscBLevel",  "Osc B Level",   unit, 0.0f, pctToText);
    addF ("subLevel",   "Sub Level",     unit, 0.0f, pctToText);
    addF ("noiseLevel", "Noise Level",   unit, 0.0f, pctToText);
    addF ("spread",     "Stereo Width",  unit, 0.6f, pctToText);

    // Filter
    addF ("cutoff",       "Cutoff",          skewed (20.0f, 20000.0f, 1000.0f), 8000.0f, hzToText);
    addF ("resonance",    "Resonance",       unit, 0.1f, pctToText);
    addC ("filterSlope",  "Filter Slope",    StringArray { "12 dB", "24 dB" }, 1);
    addF ("filterEnvAmt", "Filter Env Amt",  NormalisableRange<float> (-48.0f, 48.0f), 0.0f, semiToText);
    addF ("keyTrack",     "Key Track",       unit, 0.3f, pctToText);
    addF ("drive",        "Drive",           unit, 0.0f, pctToText);

    // Filter envelope
    addF ("fAttack",  "Filter Attack",  skewed (0.001f, 5.0f, 0.3f), 0.005f, secondsToText);
    addF ("fDecay",   "Filter Decay",   skewed (0.005f, 8.0f, 0.6f), 0.4f, secondsToText);
    addF ("fSustain", "Filter Sustain", unit, 0.0f, pctToText);
    addF ("fRelease", "Filter Release", skewed (0.005f, 10.0f, 0.8f), 0.3f, secondsToText);

    // Amp envelope
    addF ("aAttack",  "Amp Attack",  skewed (0.001f, 5.0f, 0.3f), 0.005f, secondsToText);
    addF ("aDecay",   "Amp Decay",   skewed (0.005f, 8.0f, 0.6f), 0.5f, secondsToText);
    addF ("aSustain", "Amp Sustain", unit, 0.8f, pctToText);
    addF ("aRelease", "Amp Release", skewed (0.005f, 10.0f, 0.8f), 0.3f, secondsToText);
    addF ("velSens",  "Velocity",    unit, 0.5f, pctToText);

    // LFO / motion
    addF ("lfoRate",   "LFO Rate",    skewed (0.05f, 20.0f, 2.0f), 4.0f, [] (float v, int) { return String (v, 2) + " Hz"; });
    addC ("lfoShape",  "LFO Shape",   lfoShapeNames(), 0);
    addF ("lfoPitch",  "LFO > Pitch", NormalisableRange<float> (0.0f, 100.0f), 0.0f, centsToText);
    addF ("lfoFilter", "LFO > Filter", NormalisableRange<float> (0.0f, 36.0f), 0.0f, semiToText);
    addF ("lfoAmp",    "LFO > Amp",   unit, 0.0f, pctToText);
    addF ("warble",    "Tape Warble", unit, 0.0f, pctToText);

    // Voicing
    addB ("mono",      "Mono",        false);
    addB ("legato",    "Legato",      true);
    addF ("glide",     "Glide",       skewed (0.0f, 2.0f, 0.15f), 0.08f, secondsToText);
    addI ("bendRange", "Bend Range",  1, 24, 2);

    // Effects
    addF ("chorusMix",   "Chorus Mix",   unit, 0.0f, pctToText);
    addF ("chorusRate",  "Chorus Rate",  skewed (0.05f, 5.0f, 0.8f), 0.6f, [] (float v, int) { return String (v, 2) + " Hz"; });
    addF ("chorusDepth", "Chorus Depth", unit, 0.4f, pctToText);

    addC ("delayDiv",      "Delay Time",     delayDivNames(), 2);
    addF ("delayFeedback", "Delay Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.35f, pctToText);
    addF ("delayTone",     "Delay Tone",     unit, 0.5f, pctToText);
    addF ("delayMix",      "Delay Mix",      unit, 0.0f, pctToText);

    addF ("reverbSize",  "Reverb Size",  unit, 0.7f, pctToText);
    addF ("reverbDamp",  "Reverb Damp",  unit, 0.5f, pctToText);
    addF ("reverbWidth", "Reverb Width", unit, 1.0f, pctToText);
    addF ("reverbMix",   "Reverb Mix",   unit, 0.0f, pctToText);

    // Master
    addF ("muffle",     "Muffle",      unit, 0.0f, pctToText);
    addF ("masterGain", "Master Gain", NormalisableRange<float> (-24.0f, 6.0f), -3.0f, dbToText);

    return layout;
}

//==============================================================================
// Factory presets. Values are in real units (Hz, seconds, choice index, 0/1 for switches).
// Anything not listed falls back to the parameter's default.
struct Preset
{
    const char* name;
    std::vector<std::pair<const char*, float>> values;
};

inline const std::vector<Preset>& getFactoryPresets()
{
    static const std::vector<Preset> presets
    {
        { "Init", {} },

        // Muffled, "underwater" electric-keys sound - the filtered OVO late-night vibe
        { "Underwater Keys", {
            { "oscAWave", 2 }, { "oscALevel", 0.8f }, { "oscBWave", 3 }, { "oscBOct", 1 }, { "oscBLevel", 0.25f },
            { "noiseLevel", 0.04f }, { "cutoff", 1400 }, { "resonance", 0.15f }, { "filterEnvAmt", 14 },
            { "fAttack", 0.002f }, { "fDecay", 0.9f }, { "fSustain", 0.2f }, { "fRelease", 0.6f },
            { "aAttack", 0.004f }, { "aDecay", 1.6f }, { "aSustain", 0.35f }, { "aRelease", 0.9f }, { "velSens", 0.6f },
            { "chorusMix", 0.35f }, { "chorusRate", 0.5f }, { "chorusDepth", 0.35f },
            { "reverbMix", 0.35f }, { "reverbSize", 0.8f }, { "muffle", 0.5f }, { "warble", 0.15f }, { "masterGain", -2 } } },

        // Wide, slow-opening detuned pad
        { "Toronto Night Pad", {
            { "oscAWave", 0 }, { "oscAUnison", 5 }, { "oscADetune", 0.35f }, { "oscALevel", 0.7f },
            { "oscBWave", 0 }, { "oscBOct", -1 }, { "oscBFine", 7 }, { "oscBLevel", 0.45f }, { "subLevel", 0.2f }, { "spread", 0.9f },
            { "cutoff", 700 }, { "resonance", 0.2f }, { "filterEnvAmt", 20 },
            { "fAttack", 1.4f }, { "fDecay", 2.5f }, { "fSustain", 0.5f }, { "fRelease", 2.0f },
            { "aAttack", 0.9f }, { "aDecay", 1.5f }, { "aSustain", 0.85f }, { "aRelease", 2.8f },
            { "lfoRate", 0.25f }, { "lfoFilter", 5 }, { "chorusMix", 0.4f },
            { "reverbMix", 0.45f }, { "reverbSize", 0.92f }, { "reverbDamp", 0.6f }, { "muffle", 0.35f }, { "warble", 0.2f }, { "masterGain", -5 } } },

        // Gliding psychedelic lead with vibrato, dotted delay and tape wobble
        { "Cactus Space Lead", {
            { "oscAWave", 0 }, { "oscAUnison", 3 }, { "oscADetune", 0.18f }, { "oscBWave", 1 }, { "oscBSemi", 7 }, { "oscBLevel", 0.3f },
            { "cutoff", 1800 }, { "resonance", 0.35f }, { "filterEnvAmt", 18 }, { "fDecay", 0.5f }, { "fSustain", 0.35f }, { "drive", 0.2f },
            { "aAttack", 0.01f }, { "aSustain", 0.9f }, { "aRelease", 0.45f },
            { "mono", 1 }, { "legato", 1 }, { "glide", 0.09f }, { "lfoRate", 5.2f }, { "lfoPitch", 14 },
            { "delayDiv", 2 }, { "delayFeedback", 0.45f }, { "delayMix", 0.28f }, { "delayTone", 0.4f },
            { "reverbMix", 0.35f }, { "reverbSize", 0.85f }, { "warble", 0.3f }, { "masterGain", -1 } } },

        // Distorted sine 808 with a knocking attack; overlap notes to slide
        { "Glide 808", {
            { "oscAWave", 3 }, { "oscALevel", 1.0f }, { "drive", 0.7f }, { "cutoff", 900 }, { "resonance", 0.05f },
            { "filterEnvAmt", 30 }, { "fAttack", 0.001f }, { "fDecay", 0.18f }, { "fSustain", 0.0f }, { "keyTrack", 1.0f },
            { "aAttack", 0.001f }, { "aDecay", 2.5f }, { "aSustain", 0.55f }, { "aRelease", 0.25f },
            { "mono", 1 }, { "legato", 1 }, { "glide", 0.12f }, { "bendRange", 12 }, { "velSens", 0.25f }, { "masterGain", 6 } } },

        // Short filtered pluck with quarter-note delay
        { "Haze Pluck", {
            { "oscAWave", 0 }, { "oscAUnison", 3 }, { "oscADetune", 0.2f }, { "oscBWave", 1 }, { "oscBOct", 1 }, { "oscBLevel", 0.35f },
            { "cutoff", 350 }, { "resonance", 0.3f }, { "filterEnvAmt", 38 },
            { "fAttack", 0.001f }, { "fDecay", 0.22f }, { "fSustain", 0.0f }, { "fRelease", 0.2f },
            { "aAttack", 0.001f }, { "aDecay", 0.7f }, { "aSustain", 0.0f }, { "aRelease", 0.45f },
            { "delayDiv", 3 }, { "delayFeedback", 0.38f }, { "delayMix", 0.25f },
            { "reverbMix", 0.35f }, { "chorusMix", 0.25f }, { "muffle", 0.2f }, { "masterGain", 2 } } },

        // Detuned bell with heavy tape wobble
        { "Wobble Bell", {
            { "oscAWave", 3 }, { "oscBWave", 3 }, { "oscBOct", 2 }, { "oscBSemi", 7 }, { "oscBLevel", 0.35f },
            { "cutoff", 6000 }, { "aAttack", 0.002f }, { "aDecay", 2.2f }, { "aSustain", 0.0f }, { "aRelease", 1.5f },
            { "warble", 0.65f }, { "chorusMix", 0.4f }, { "reverbMix", 0.5f }, { "reverbSize", 0.9f },
            { "delayMix", 0.15f }, { "delayDiv", 4 }, { "muffle", 0.3f }, { "masterGain", -2 } } },

        // Breathy, slowly moving choir-ish pad
        { "Dark Choir Pad", {
            { "oscAWave", 2 }, { "oscAUnison", 7 }, { "oscADetune", 0.55f }, { "oscBWave", 0 }, { "oscBLevel", 0.15f }, { "oscBOct", 1 },
            { "noiseLevel", 0.06f }, { "spread", 1.0f }, { "cutoff", 1100 }, { "resonance", 0.25f },
            { "lfoRate", 0.15f }, { "lfoFilter", 7 }, { "lfoShape", 1 },
            { "aAttack", 1.3f }, { "aDecay", 2.0f }, { "aSustain", 0.9f }, { "aRelease", 3.5f },
            { "reverbMix", 0.6f }, { "reverbSize", 0.95f }, { "chorusMix", 0.3f }, { "muffle", 0.45f }, { "warble", 0.25f }, { "masterGain", -5 } } },

        // Clean round sub bass
        { "Late Night Sub", {
            { "oscAWave", 2 }, { "oscALevel", 0.5f }, { "subLevel", 0.9f }, { "cutoff", 450 }, { "drive", 0.25f },
            { "aAttack", 0.005f }, { "aDecay", 0.8f }, { "aSustain", 0.8f }, { "aRelease", 0.2f },
            { "mono", 1 }, { "legato", 1 }, { "glide", 0.05f }, { "masterGain", 4 } } },
    };
    return presets;
}

} // namespace afterhours
