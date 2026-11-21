#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <vector>
#include <algorithm>

namespace afterhours
{

constexpr float kPi    = juce::MathConstants<float>::pi;
constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

//==============================================================================
// Snapshot of all per-voice settings, taken once per block.
struct VoiceParams
{
    int   waveA = 0, octA = 0, semiA = 0, unison = 1;
    float detune = 0.25f, levelA = 0.8f;
    int   waveB = 0, octB = 0, semiB = 0;
    float fineB = 0.0f, levelB = 0.0f, sub = 0.0f, noise = 0.0f, spread = 0.6f;

    float reso = 0.1f, envAmt = 0.0f, keyTrack = 0.3f, drive = 0.0f;
    bool  slope24 = true;

    juce::ADSR::Parameters ampEnv  { 0.005f, 0.5f, 0.8f, 0.3f };
    juce::ADSR::Parameters filtEnv { 0.005f, 0.4f, 0.0f, 0.3f };
    float velSens = 0.5f;

    float lfoPitch = 0.0f, lfoFilter = 0.0f, lfoAmp = 0.0f, warble = 0.0f;
    float glideSec = 0.0f;
};

// Per-sample modulation shared by all voices.
struct ModBuffers
{
    std::vector<float> lfo, cutoff, bend;
    void resize (int n) { lfo.assign ((size_t) n, 0.0f); cutoff.assign ((size_t) n, 1000.0f); bend.assign ((size_t) n, 0.0f); }
};

//==============================================================================
inline float polyBlep (float t, float dt)
{
    if (t < dt)        { t /= dt;               return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt;   return t * t + t + t + 1.0f; }
    return 0.0f;
}

inline float oscillator (int wave, float phase, float dt)
{
    switch (wave)
    {
        case 0:  return 2.0f * phase - 1.0f - polyBlep (phase, dt);                // saw
        case 1:                                                                     // square
        {
            float s = phase < 0.5f ? 1.0f : -1.0f;
            float t2 = phase + 0.5f;
            if (t2 >= 1.0f) t2 -= 1.0f;
            return s + polyBlep (phase, dt) - polyBlep (t2, dt);
        }
        case 2:  return 4.0f * std::abs (phase - 0.5f) - 1.0f;                     // triangle
        default: return std::sin (kTwoPi * phase);                                  // sine
    }
}

//==============================================================================
// Zero-delay-feedback state variable filter (lowpass output).
struct SvfCoeffs
{
    float a1, a2, a3;
    static SvfCoeffs make (float g, float k)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        return { a1, g * a1, g * g * a1 };
    }
};

struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;
    void reset() { ic1 = ic2 = 0.0f; }
    inline float process (float v0, const SvfCoeffs& c)
    {
        const float v3 = v0 - ic2;
        const float v1 = c.a1 * ic1 + c.a2 * v3;
        const float v2 = ic2 + c.a2 * ic1 + c.a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }
};

//==============================================================================
class Voice
{
public:
    static constexpr int maxUnison = 7;

    void prepare (double sr)
    {
        sampleRate = sr;
        ampEnv.setSampleRate (sr);
        filtEnv.setSampleRate (sr);
        rng.setSeedRandomly();
        wRate = 0.45f + rng.nextFloat() * 0.5f;
        kill();
    }

    bool  isActive() const       { return active; }
    bool  isKeyHeld() const      { return keyDown || sustained; }
    bool  isSustained() const    { return sustained; }
    bool  isKeyDown() const      { return keyDown; }
    int   getNote() const        { return note; }
    float getCurrentPitch() const { return currentNote; }
    float getVelocity() const    { return velocity; }
    juce::uint64 getAge() const  { return age; }

    void start (int noteNumber, float vel, float glideFrom, const VoiceParams& p, juce::uint64 order)
    {
        const bool wasActive = active;
        note        = noteNumber;
        targetNote  = (float) noteNumber;
        currentNote = glideFrom >= 0.0f ? glideFrom : targetNote;
        velocity    = vel;
        keyDown     = true;
        sustained   = false;
        age         = order;

        if (! wasActive)
        {
            const bool freeRun = p.unison > 1 && p.waveA != 3;
            for (auto& ph : phaseA) ph = freeRun ? rng.nextFloat() : 0.0f;
            phaseB = phaseSub = 0.0f;
            for (auto& ch : filters) for (auto& f : ch) f.reset();
            ampEnv.reset();
            filtEnv.reset();
            wPhase = rng.nextFloat();
        }

        ampEnv.setParameters (p.ampEnv);
        filtEnv.setParameters (p.filtEnv);
        ampEnv.noteOn();
        filtEnv.noteOn();
        active = true;
    }

    void glideTo (int noteNumber)
    {
        note = noteNumber;
        targetNote = (float) noteNumber;
        keyDown = true;
        sustained = false;
    }

    void markSustained() { keyDown = false; sustained = true; }
    void release()       { keyDown = false; sustained = false; ampEnv.noteOff(); filtEnv.noteOff(); }
    void kill()          { active = false; keyDown = false; sustained = false; ampEnv.reset(); filtEnv.reset(); }

    void render (float* outL, float* outR, int start, int num, const VoiceParams& p, const ModBuffers& mod, float modWheel)
    {
        if (! active)
            return;

        ampEnv.setParameters (p.ampEnv);
        filtEnv.setParameters (p.filtEnv);

        const float sr    = (float) sampleRate;
        const float invSr = 1.0f / sr;

        // Unison layout
        const int nUni = juce::jlimit (1, maxUnison, p.unison);
        float ratio[maxUnison], gL[maxUnison], gR[maxUnison];
        const float detuneCents = p.detune * 50.0f;
        for (int u = 0; u < nUni; ++u)
        {
            const float pos = nUni == 1 ? 0.0f : (float) u / (float) (nUni - 1) * 2.0f - 1.0f;
            ratio[u] = std::exp2 (pos * detuneCents / 1200.0f);
            const float ang = (pos * p.spread + 1.0f) * kPi * 0.25f;
            gL[u] = std::cos (ang) * 1.41421356f;
            gR[u] = std::sin (ang) * 1.41421356f;
        }
        const float uniNorm = 1.0f / std::sqrt ((float) nUni);

        const float ratioA    = std::exp2 ((float) (p.octA * 12 + p.semiA) / 12.0f);
        const float ratioB    = std::exp2 (((float) (p.octB * 12 + p.semiB) + p.fineB * 0.01f) / 12.0f);
        const float glideCoef = p.glideSec <= 0.0005f ? 1.0f : 1.0f - std::exp (-1.0f / (p.glideSec * 0.25f * sr));
        const float velGain   = 1.0f - p.velSens + p.velSens * velocity;
        const float driveGain = 1.0f + p.drive * 8.0f;
        const float driveComp = 1.0f / (1.0f + p.drive * 1.5f);
        const float kRes      = 2.0f - 1.94f * p.reso;
        const float nyq       = 0.45f * sr;

        for (int i = start; i < start + num; ++i)
        {
            currentNote += (targetNote - currentNote) * glideCoef;

            // Tape warble: slow sine + smoothed random pitch drift (cents)
            float drift = 0.0f;
            if (p.warble > 0.0f)
            {
                wPhase += wRate * invSr;
                if (wPhase >= 1.0f) wPhase -= 1.0f;
                if (--rndCounter <= 0)
                {
                    rndCounter = (int) (sampleRate / 6.0);
                    rndTarget = rng.nextFloat() * 2.0f - 1.0f;
                }
                rndCur += (rndTarget - rndCur) * (8.0f * invSr);
                drift = p.warble * (std::sin (kTwoPi * wPhase) * 16.0f + rndCur * 12.0f);
            }

            const float lfo    = mod.lfo[(size_t) i];
            const float cents  = lfo * (p.lfoPitch + modWheel * 40.0f) + drift;
            const float pitch  = currentNote + mod.bend[(size_t) i] + cents * 0.01f;
            const float baseHz = 440.0f * std::exp2 ((pitch - 69.0f) / 12.0f);

            float l = 0.0f, r = 0.0f;

            if (p.levelA > 0.0f)
            {
                const float fA = baseHz * ratioA;
                float aL = 0.0f, aR = 0.0f;
                for (int u = 0; u < nUni; ++u)
                {
                    const float inc = std::min (fA * ratio[u] * invSr, 0.49f);
                    const float s = oscillator (p.waveA, phaseA[u], inc);
                    phaseA[u] += inc;
                    if (phaseA[u] >= 1.0f) phaseA[u] -= 1.0f;
                    aL += s * gL[u];
                    aR += s * gR[u];
                }
                l += aL * uniNorm * p.levelA;
                r += aR * uniNorm * p.levelA;
            }

            if (p.levelB > 0.0f)
            {
                const float inc = std::min (baseHz * ratioB * invSr, 0.49f);
                const float s = oscillator (p.waveB, phaseB, inc) * p.levelB;
                phaseB += inc;
                if (phaseB >= 1.0f) phaseB -= 1.0f;
                l += s;
                r += s;
            }

            if (p.sub > 0.0f)
            {
                const float inc = std::min (baseHz * ratioA * 0.5f * invSr, 0.49f);
                const float s = std::sin (kTwoPi * phaseSub) * p.sub;
                phaseSub += inc;
                if (phaseSub >= 1.0f) phaseSub -= 1.0f;
                l += s;
                r += s;
            }

            if (p.noise > 0.0f)
            {
                const float n = (rng.nextFloat() * 2.0f - 1.0f) * p.noise * 0.5f;
                l += n;
                r += n;
            }

            if (p.drive > 0.001f)
            {
                l = std::tanh (l * driveGain) * driveComp;
                r = std::tanh (r * driveGain) * driveComp;
            }

            // Filter
            const float fe    = filtEnv.getNextSample();
            const float semis = p.envAmt * fe + p.lfoFilter * lfo + p.keyTrack * (currentNote - 60.0f);
            const float fc    = juce::jlimit (20.0f, nyq, mod.cutoff[(size_t) i] * std::exp2 (semis / 12.0f));
            const float g     = std::tan (kPi * fc * invSr);

            if (p.slope24)
            {
                const auto c1 = SvfCoeffs::make (g, 1.41421356f);
                const auto c2 = SvfCoeffs::make (g, kRes);
                l = filters[0][1].process (filters[0][0].process (l, c1), c2);
                r = filters[1][1].process (filters[1][0].process (r, c1), c2);
            }
            else
            {
                const auto c = SvfCoeffs::make (g, kRes);
                l = filters[0][0].process (l, c);
                r = filters[1][0].process (r, c);
            }

            const float a    = ampEnv.getNextSample();
            const float trem = 1.0f - p.lfoAmp * 0.5f * (lfo + 1.0f);
            const float gain = a * velGain * trem * 0.35f;

            outL[i] += l * gain;
            outR[i] += r * gain;

            if (! ampEnv.isActive())
            {
                kill();
                break;
            }
        }
    }

private:
    double sampleRate = 44100.0;
    bool active = false, keyDown = false, sustained = false;
    int note = 60;
    float targetNote = 60.0f, currentNote = 60.0f, velocity = 1.0f;
    juce::uint64 age = 0;

    std::array<float, maxUnison> phaseA {};
    float phaseB = 0.0f, phaseSub = 0.0f;
    Svf filters[2][2];
    juce::ADSR ampEnv, filtEnv;
    juce::Random rng;

    float wPhase = 0.0f, wRate = 0.6f, rndCur = 0.0f, rndTarget = 0.0f;
    int rndCounter = 0;
};

//==============================================================================
class SynthEngine
{
public:
    static constexpr int numVoices = 16;

    SynthEngine() { noteStack.reserve (128); }

    void prepare (double sr)
    {
        for (auto& v : voices) v.prepare (sr);
        noteStack.clear();
    }

    void setParams (const VoiceParams& p, bool monoMode, bool legatoMode)
    {
        if (monoMode != mono)
        {
            allNotesOff (true);
            mono = monoMode;
        }
        params = p;
        legato = legatoMode;
    }

    void noteOn (int note, float vel)  { if (mono) monoNoteOn (note, vel); else polyNoteOn (note, vel); }
    void noteOff (int note)            { if (mono) monoNoteOff (note);     else polyNoteOff (note); }

    void setSustain (bool on)
    {
        sustainOn = on;
        if (! on)
            for (auto& v : voices)
                if (v.isActive() && v.isSustained())
                    v.release();
    }

    void allNotesOff (bool hard)
    {
        for (auto& v : voices) { if (hard) v.kill(); else if (v.isActive()) v.release(); }
        noteStack.clear();
    }

    void render (float* L, float* R, int start, int num, const ModBuffers& mod, float modWheel)
    {
        if (mono)
            voices[0].render (L, R, start, num, params, mod, modWheel);
        else
            for (auto& v : voices)
                v.render (L, R, start, num, params, mod, modWheel);
    }

private:
    void polyNoteOn (int note, float vel)
    {
        Voice* target = nullptr;
        for (auto& v : voices) if (v.isActive() && v.getNote() == note) { target = &v; break; }
        if (target == nullptr) for (auto& v : voices) if (! v.isActive()) { target = &v; break; }
        if (target == nullptr)
        {
            for (auto& v : voices)   // steal the oldest released voice first
                if (! v.isKeyHeld() && (target == nullptr || v.getAge() < target->getAge()))
                    target = &v;
            if (target == nullptr)
                for (auto& v : voices)
                    if (target == nullptr || v.getAge() < target->getAge())
                        target = &v;
        }
        target->start (note, vel, -1.0f, params, ++counter);
    }

    void polyNoteOff (int note)
    {
        for (auto& v : voices)
            if (v.isActive() && v.getNote() == note && v.isKeyDown())
            {
                if (sustainOn) v.markSustained();
                else           v.release();
            }
    }

    void removeFromStack (int note) { noteStack.erase (std::remove (noteStack.begin(), noteStack.end(), note), noteStack.end()); }

    float glideSource (const Voice& v) const { return (v.isActive() && params.glideSec > 0.0005f) ? v.getCurrentPitch() : -1.0f; }

    void monoNoteOn (int note, float vel)
    {
        removeFromStack (note);
        if (noteStack.size() < noteStack.capacity()) noteStack.push_back (note);

        auto& v = voices[0];
        if (v.isActive() && legato && noteStack.size() > 1)
        {
            v.glideTo (note);   // slide without re-triggering - the 808 glide
            return;
        }
        v.start (note, vel, glideSource (v), params, ++counter);
    }

    void monoNoteOff (int note)
    {
        removeFromStack (note);
        auto& v = voices[0];
        if (! v.isActive())
            return;

        if (noteStack.empty())
        {
            if (v.getNote() == note && v.isKeyDown())
            {
                if (sustainOn) v.markSustained();
                else           v.release();
            }
            return;
        }

        if (v.getNote() == note)
        {
            const int next = noteStack.back();
            if (legato) v.glideTo (next);
            else        v.start (next, v.getVelocity(), glideSource (v), params, ++counter);
        }
    }

    std::array<Voice, numVoices> voices;
    VoiceParams params;
    bool mono = false, legato = true, sustainOn = false;
    juce::uint64 counter = 0;
    std::vector<int> noteStack;
};

//==============================================================================
struct Lfo
{
    double sampleRate = 44100.0;
    float phase = 0.0f, rate = 1.0f, smoothed = 0.0f, sh = 0.0f;
    int shape = 0;
    juce::Random rng;

    float next()
    {
        float out;
        switch (shape)
        {
            case 0:  out = std::sin (kTwoPi * phase); break;
            case 1:  out = 1.0f - 4.0f * std::abs (phase - 0.5f); break;
            case 2:  out = phase < 0.5f ? 1.0f : -1.0f; break;
            default: out = sh; break;
        }
        phase += rate / (float) sampleRate;
        if (phase >= 1.0f) { phase -= 1.0f; sh = rng.nextFloat() * 2.0f - 1.0f; }
        smoothed += (out - smoothed) * 0.01f;   // de-click square / S&H
        return smoothed;
    }
};

} // namespace afterhours
