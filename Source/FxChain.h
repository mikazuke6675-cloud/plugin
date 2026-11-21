#pragma once
#include <juce_dsp/juce_dsp.h>
#include "SynthEngine.h"

namespace afterhours
{

struct FxParams
{
    float chorusMix = 0, chorusRate = 0.6f, chorusDepth = 0.4f;
    float delayBeats = 0.75f, delayFeedback = 0.35f, delayTone = 0.5f, delayMix = 0;
    float revSize = 0.7f, revDamp = 0.5f, revWidth = 1.0f, revMix = 0;
    float muffle = 0, gainDb = -3;
};

// Chorus -> ping-pong delay -> reverb -> "muffle" lowpass -> master gain + soft clip
class FxChain
{
public:
    void prepare (double sr, int maxBlock)
    {
        sampleRate = sr;
        juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
        chorus.prepare (spec);
        chorus.setCentreDelay (8.0f);
        chorus.setFeedback (0.1f);
        chorus.reset();

        reverb.setSampleRate (sr);
        reverb.reset();

        delaySize = (int) (sr * 4.0) + 4;
        delayL.assign ((size_t) delaySize, 0.0f);
        delayR.assign ((size_t) delaySize, 0.0f);
        writePos = 0;
        delayTime = sr * 0.375;
        toneL = toneR = 0.0f;

        muffleFc.reset (sr, 0.05);
        muffleFc.setCurrentAndTargetValue (20000.0f);
        gain.reset (sr, 0.05);
        gain.setCurrentAndTargetValue (1.0f);
        for (auto& ch : muffleFilters) for (auto& f : ch) f.reset();
    }

    void process (float* L, float* R, int n, const FxParams& p, double bpm)
    {
        // Chorus
        chorus.setRate (p.chorusRate);
        chorus.setDepth (p.chorusDepth);
        chorus.setMix (p.chorusMix);
        float* chans[] { L, R };
        juce::dsp::AudioBlock<float> block (chans, 2, (size_t) n);
        chorus.process (juce::dsp::ProcessContextReplacing<float> (block));

        // Tempo-synced ping-pong delay with a darkening filter in the feedback path
        const double target   = juce::jlimit (1.0, (double) delaySize - 4.0, 60.0 / bpm * p.delayBeats * sampleRate);
        const float toneHz    = 800.0f * std::pow (16.0f, p.delayTone);
        const float toneCoef  = 1.0f - std::exp (-kTwoPi * toneHz / (float) sampleRate);
        for (int i = 0; i < n; ++i)
        {
            delayTime += (target - delayTime) * 0.0002;
            double rp = (double) writePos - delayTime;
            if (rp < 0) rp += delaySize;
            const int i0 = (int) rp;
            const int i1 = (i0 + 1) % delaySize;
            const float frac = (float) (rp - i0);
            const float dl = delayL[(size_t) i0] + frac * (delayL[(size_t) i1] - delayL[(size_t) i0]);
            const float dr = delayR[(size_t) i0] + frac * (delayR[(size_t) i1] - delayR[(size_t) i0]);
            toneL += (dl - toneL) * toneCoef;
            toneR += (dr - toneR) * toneCoef;

            const float in = 0.5f * (L[i] + R[i]);
            delayL[(size_t) writePos] = std::tanh (in + p.delayFeedback * toneR);
            delayR[(size_t) writePos] = p.delayFeedback * toneL;
            if (++writePos >= delaySize) writePos = 0;

            L[i] += toneL * p.delayMix;
            R[i] += toneR * p.delayMix;
        }

        // Reverb
        juce::Reverb::Parameters rp;
        rp.roomSize   = p.revSize;
        rp.damping    = p.revDamp;
        rp.width      = p.revWidth;
        rp.wetLevel   = p.revMix * 0.5f;
        rp.dryLevel   = 0.5f * (1.0f - 0.4f * p.revMix);
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        reverb.processStereo (L, R, n);

        // Muffle: the "playing through the wall" lowpass, with a slight resonant bump
        muffleFc.setTargetValue (std::min (20000.0f * std::pow (350.0f / 20000.0f, p.muffle), 0.45f * (float) sampleRate));
        const float k    = 1.41421356f - 0.6f * p.muffle;
        const float comp = 1.0f + 0.4f * p.muffle;
        gain.setTargetValue (juce::Decibels::decibelsToGain (p.gainDb));
        for (int i = 0; i < n; ++i)
        {
            const float g = std::tan (kPi * muffleFc.getNextValue() / (float) sampleRate);
            const auto c = SvfCoeffs::make (g, k);
            float l = muffleFilters[0][1].process (muffleFilters[0][0].process (L[i], c), c);
            float r = muffleFilters[1][1].process (muffleFilters[1][0].process (R[i], c), c);
            const float gg = gain.getNextValue() * comp;
            L[i] = std::tanh (l * gg);
            R[i] = std::tanh (r * gg);
        }
    }

private:
    double sampleRate = 44100.0;
    juce::dsp::Chorus<float> chorus;
    juce::Reverb reverb;

    std::vector<float> delayL, delayR;
    int delaySize = 1, writePos = 0;
    double delayTime = 1.0;
    float toneL = 0, toneR = 0;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> muffleFc;
    juce::SmoothedValue<float> gain;
    Svf muffleFilters[2][2];
};

} // namespace afterhours
