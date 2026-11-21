#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace afterhours;

AfterhoursProcessor::AfterhoursProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    setCurrentProgram (1);   // start on "Underwater Keys"
}

bool AfterhoursProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void AfterhoursProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    fx.prepare (sampleRate, kChunk);
    lfo.sampleRate = sampleRate;
    mod.resize (kChunk);
    synthBuffer.setSize (2, kChunk);

    cutoffSm.reset (sampleRate, 0.03);
    cutoffSm.setCurrentAndTargetValue (get ("cutoff"));
    bendSm.reset (sampleRate, 0.01);
    bendSm.setCurrentAndTargetValue (0.0f);
}

void AfterhoursProcessor::readParameters()
{
    auto& p = voiceParams;
    p.waveA  = (int) get ("oscAWave");
    p.octA   = (int) get ("oscAOct");
    p.semiA  = (int) get ("oscASemi");
    p.unison = (int) get ("oscAUnison");
    p.detune = get ("oscADetune");
    p.levelA = get ("oscALevel");
    p.waveB  = (int) get ("oscBWave");
    p.octB   = (int) get ("oscBOct");
    p.semiB  = (int) get ("oscBSemi");
    p.fineB  = get ("oscBFine");
    p.levelB = get ("oscBLevel");
    p.sub    = get ("subLevel");
    p.noise  = get ("noiseLevel");
    p.spread = get ("spread");

    p.reso     = get ("resonance");
    p.slope24  = get ("filterSlope") > 0.5f;
    p.envAmt   = get ("filterEnvAmt");
    p.keyTrack = get ("keyTrack");
    p.drive    = get ("drive");

    p.filtEnv = { get ("fAttack"), get ("fDecay"), get ("fSustain"), get ("fRelease") };
    p.ampEnv  = { get ("aAttack"), get ("aDecay"), get ("aSustain"), get ("aRelease") };
    p.velSens = get ("velSens");

    p.lfoPitch  = get ("lfoPitch");
    p.lfoFilter = get ("lfoFilter");
    p.lfoAmp    = get ("lfoAmp");
    p.warble    = get ("warble");
    p.glideSec  = get ("glide");

    lfo.rate  = get ("lfoRate");
    lfo.shape = (int) get ("lfoShape");
    bendRange = get ("bendRange");
    cutoffSm.setTargetValue (get ("cutoff"));

    engine.setParams (p, get ("mono") > 0.5f, get ("legato") > 0.5f);

    auto& f = fxParams;
    f.chorusMix     = get ("chorusMix");
    f.chorusRate    = get ("chorusRate");
    f.chorusDepth   = get ("chorusDepth");
    f.delayBeats    = delayDivToBeats ((int) get ("delayDiv"));
    f.delayFeedback = get ("delayFeedback");
    f.delayTone     = get ("delayTone");
    f.delayMix      = get ("delayMix");
    f.revSize       = get ("reverbSize");
    f.revDamp       = get ("reverbDamp");
    f.revWidth      = get ("reverbWidth");
    f.revMix        = get ("reverbMix");
    f.muffle        = get ("muffle");
    f.gainDb        = get ("masterGain");
}

void AfterhoursProcessor::renderSegment (int start, int num)
{
    if (num <= 0)
        return;

    for (int i = start; i < start + num; ++i)
    {
        mod.cutoff[(size_t) i] = cutoffSm.getNextValue();
        mod.bend[(size_t) i]   = bendSm.getNextValue() * bendRange;
        mod.lfo[(size_t) i]    = lfo.next();
    }
    engine.render (synthBuffer.getWritePointer (0), synthBuffer.getWritePointer (1), start, num, mod, modWheel);
}

void AfterhoursProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
        engine.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff())
        engine.noteOff (m.getNoteNumber());
    else if (m.isPitchWheel())
        bendSm.setTargetValue ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isSustainPedalOn())
        engine.setSustain (true);
    else if (m.isSustainPedalOff())
        engine.setSustain (false);
    else if (m.isControllerOfType (1))
        modWheel = (float) m.getControllerValue() / 127.0f;
    else if (m.isAllNotesOff() || m.isAllSoundOff())
        engine.allNotesOff (m.isAllSoundOff());
}

void AfterhoursProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int total = buffer.getNumSamples();
    readParameters();

    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = juce::jlimit (30.0, 300.0, *b);

    auto it = midi.cbegin();
    const auto end = midi.cend();

    for (int offset = 0; offset < total; offset += kChunk)
    {
        const int len = std::min (kChunk, total - offset);
        synthBuffer.clear (0, len);

        int pos = 0;
        while (it != end)
        {
            const auto meta = *it;
            const int evPos = juce::jlimit (pos, len, meta.samplePosition - offset);
            if (meta.samplePosition - offset >= len)
                break;
            renderSegment (pos, evPos - pos);
            handleMidi (meta.getMessage());
            pos = evPos;
            ++it;
        }
        renderSegment (pos, len - pos);

        fx.process (synthBuffer.getWritePointer (0), synthBuffer.getWritePointer (1), len, fxParams, bpm);

        const int outCh = buffer.getNumChannels();
        if (outCh >= 2)
        {
            buffer.copyFrom (0, offset, synthBuffer, 0, 0, len);
            buffer.copyFrom (1, offset, synthBuffer, 1, 0, len);
            for (int c = 2; c < outCh; ++c) buffer.clear (c, offset, len);
        }
        else if (outCh == 1)
        {
            buffer.copyFrom (0, offset, synthBuffer, 0, 0, len);
            buffer.addFrom  (0, offset, synthBuffer, 1, 0, len);
            buffer.applyGain (0, offset, len, 0.5f);
        }
    }
}

//==============================================================================
int AfterhoursProcessor::getNumPrograms() { return (int) getFactoryPresets().size(); }

const juce::String AfterhoursProcessor::getProgramName (int index)
{
    const auto& presets = getFactoryPresets();
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? juce::String (presets[(size_t) index].name) : juce::String();
}

void AfterhoursProcessor::setCurrentProgram (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    currentProgram = index;
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& [id, value] : presets[(size_t) index].values)
        if (auto* rp = apvts.getParameter (id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (value));
}

void AfterhoursProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("program", currentProgram, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void AfterhoursProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            currentProgram = (int) vt.getProperty ("program", 0);
            apvts.replaceState (vt);
        }
}

juce::AudioProcessorEditor* AfterhoursProcessor::createEditor() { return new AfterhoursEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new AfterhoursProcessor(); }
