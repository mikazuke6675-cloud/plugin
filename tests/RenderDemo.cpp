// Offline renderer: plays a short phrase through every factory preset, writes WAVs and
// prints level / sanity stats. Not part of the plugin itself.
#include "../Source/PluginProcessor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

struct Ev { double start, dur; int note; float vel; };

static std::vector<Ev> chords()
{
    const std::vector<std::vector<int>> prog { { 53, 56, 60, 63, 67 }, { 49, 53, 56, 60, 63 }, { 46, 49, 53, 56, 60 }, { 48, 51, 55, 58, 62 } };
    std::vector<Ev> ev;
    for (size_t c = 0; c < prog.size(); ++c)
    {
        ev.push_back ({ c * 2.4, 2.2, prog[c][0] - 12, 0.8f });
        for (auto n : prog[c]) ev.push_back ({ c * 2.4 + 0.01, 2.2, n, 0.7f });
    }
    return ev;
}

static std::vector<Ev> lead()
{
    return { { 0.0, 1.5, 72, 0.8f }, { 1.4, 0.7, 75, 0.7f }, { 2.0, 1.0, 77, 0.8f }, { 3.0, 1.5, 79, 0.9f },
             { 4.4, 0.7, 77, 0.7f }, { 5.0, 1.1, 75, 0.8f }, { 6.0, 2.0, 72, 0.8f } };
}

static std::vector<Ev> pluck()
{
    const int pat[] { 60, 63, 67, 70, 72, 70, 67, 63, 58, 63, 67, 70, 72, 75, 72, 67 };
    std::vector<Ev> ev;
    for (int i = 0; i < 16; ++i) ev.push_back ({ i * 0.25, 0.2, pat[i], i % 4 == 0 ? 0.9f : 0.65f });
    return ev;
}

static std::vector<Ev> bell()
{
    const int pat[] { 72, 75, 79, 84, 82, 79, 77, 75 };
    std::vector<Ev> ev;
    for (int i = 0; i < 8; ++i) ev.push_back ({ i * 0.5, 0.4, pat[i], 0.8f });
    return ev;
}

static std::vector<Ev> bass()
{
    return { { 0.0, 1.6, 29, 0.9f }, { 1.5, 0.6, 32, 0.9f }, { 2.5, 0.9, 36, 0.9f }, { 3.5, 1.4, 27, 0.9f },
             { 5.0, 0.9, 29, 0.9f }, { 5.8, 0.35, 41, 0.9f }, { 6.2, 1.5, 29, 0.9f } };
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const double sr = 44100.0;
    const int block = 512;
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    AfterhoursProcessor proc;
    proc.setPlayConfigDetails (0, 2, sr, block);
    proc.prepareToPlay (sr, block);

    juce::AudioBuffer<float> all (2, 0);
    bool ok = true;

    for (int p = 0; p < proc.getNumPrograms(); ++p)
    {
        proc.setCurrentProgram (p);
        proc.prepareToPlay (sr, block);   // reset state between presets
        const auto name = proc.getProgramName (p);

        std::vector<Ev> ev;
        if (name.containsIgnoreCase ("808") || name.containsIgnoreCase ("Sub")) ev = bass();
        else if (name.containsIgnoreCase ("Lead")) ev = lead();
        else if (name.containsIgnoreCase ("Pluck")) ev = pluck();
        else if (name.containsIgnoreCase ("Bell")) ev = bell();
        else ev = chords();

        double end = 0;
        juce::MidiBuffer seq;
        for (auto& e : ev)
        {
            seq.addEvent (juce::MidiMessage::noteOn (1, e.note, e.vel), (int) (e.start * sr));
            seq.addEvent (juce::MidiMessage::noteOff (1, e.note), (int) ((e.start + e.dur) * sr));
            end = std::max (end, e.start + e.dur);
        }
        const int total = (int) ((end + 3.0) * sr);
        juce::AudioBuffer<float> out (2, total);

        for (int pos = 0; pos < total; pos += block)
        {
            const int n = std::min (block, total - pos);
            juce::AudioBuffer<float> buf (2, n);
            juce::MidiBuffer midi;
            for (const auto meta : seq)
                if (meta.samplePosition >= pos && meta.samplePosition < pos + n)
                    midi.addEvent (meta.getMessage(), meta.samplePosition - pos);
            proc.processBlock (buf, midi);
            for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, buf, c, 0, n);
        }

        float peak = 0; double sumSq = 0; bool bad = false;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < total; ++i)
            {
                const float s = out.getSample (c, i);
                if (! std::isfinite (s)) bad = true;
                peak = std::max (peak, std::abs (s));
                sumSq += (double) s * s;
            }
        const double rms = std::sqrt (sumSq / (2.0 * total));
        std::cout << juce::String (p).paddedLeft (' ', 2) << "  " << name.paddedRight (' ', 20)
                  << " peak " << juce::String (juce::Decibels::gainToDecibels (peak), 1) << " dBFS"
                  << "  rms " << juce::String (juce::Decibels::gainToDecibels ((float) rms), 1) << " dBFS"
                  << (bad ? "  NaN/Inf!" : "") << std::endl;
        if (bad || peak < 0.01f) ok = false;

        // Append to combined demo with a short gap
        const int gap = (int) (0.6 * sr);
        const int startAll = all.getNumSamples();
        all.setSize (2, startAll + total + gap, true, true);
        for (int c = 0; c < 2; ++c) all.copyFrom (c, startAll, out, c, 0, total);

        juce::WavAudioFormat wav;
        auto file = outDir.getChildFile (juce::String (p).paddedLeft ('0', 2) + "_" + name.replaceCharacter (' ', '_') + ".wav");
        file.deleteFile();
        if (auto stream = file.createOutputStream())
            if (std::unique_ptr<juce::AudioFormatWriter> w { wav.createWriterFor (stream.get(), sr, 2, 16, {}, 0) })
            {
                stream.release();
                w->writeFromAudioSampleBuffer (out, 0, total);
            }
    }

    juce::WavAudioFormat wav;
    auto file = outDir.getChildFile ("Afterhours_all_presets.wav");
    file.deleteFile();
    if (auto stream = file.createOutputStream())
        if (std::unique_ptr<juce::AudioFormatWriter> w { wav.createWriterFor (stream.get(), sr, 2, 16, {}, 0) })
        {
            stream.release();
            w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
        }

    std::cout << (ok ? "ALL OK" : "PROBLEMS FOUND") << std::endl;
    return ok ? 0 : 1;
}
