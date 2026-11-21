#include "PluginEditor.h"

using namespace afterhours;

//==============================================================================
LookAndFeel::LookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::dimText);
    setColour (juce::ComboBox::backgroundColourId, colours::knobBody);
    setColour (juce::ComboBox::outlineColourId, colours::panelEdge);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::arrowColourId, colours::accent);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accent.withAlpha (0.35f));
    setColour (juce::ToggleButton::tickColourId, colours::accent);
    setColour (juce::ToggleButton::tickDisabledColourId, colours::dimText);
    setColour (juce::ToggleButton::textColourId, colours::text);
    setColour (juce::TextButton::buttonColourId, colours::knobBody);
    setColour (juce::TextButton::textColourOffId, colours::text);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                    float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float lineW = 3.5f;
    const float arcR = radius - lineW * 0.5f;
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::panelEdge);
    g.strokePath (track, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setGradientFill (juce::ColourGradient (colours::accent, bounds.getBottomLeft(), colours::accent2, bounds.getTopRight(), false));
    g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = arcR - 6.0f;
    g.setColour (colours::knobBody);
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);
    g.setColour (colours::panelEdge.brighter (0.2f));
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.0f);

    g.setColour (colours::text);
    g.drawLine (juce::Line<float> (centre.getPointOnCircumference (knobR * 0.25f, angle),
                                   centre.getPointOnCircumference (knobR * 0.85f, angle)), 2.0f);
}

//==============================================================================
ParamControl::ParamControl (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& labelText)
{
    auto* param = state.getParameter (paramID);
    jassert (param != nullptr);

    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    addAndMakeVisible (label);

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
    {
        kind = Kind::choice;
        combo.addItemList (choice->choices, 1);
        addAndMakeVisible (combo);
        comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramID, combo);
    }
    else if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
    {
        kind = Kind::toggle;
        toggle.setButtonText ("ON");
        addAndMakeVisible (toggle);
        buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramID, toggle);
    }
    else
    {
        kind = Kind::knob;
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 16);
        slider.setTextBoxIsEditable (false);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxTextColourId, colours::text);
        addAndMakeVisible (slider);
        sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);
    }
}

void ParamControl::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));

    switch (kind)
    {
        case Kind::knob:   slider.setBounds (area); break;
        case Kind::choice: combo.setBounds (area.withSizeKeepingCentre (juce::jmin (area.getWidth() - 6, 96), 24)); break;
        case Kind::toggle: toggle.setBounds (area.withSizeKeepingCentre (juce::jmin (area.getWidth() - 6, 60), 24)); break;
    }
}

//==============================================================================
AfterhoursEditor::AfterhoursEditor (AfterhoursProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    setLookAndFeel (&lnf);

    addSection (0, "OSC A", 3, 2, { { "oscAWave", "WAVE" }, { "oscAOct", "OCTAVE" }, { "oscASemi", "SEMI" },
                                    { "oscAUnison", "UNISON" }, { "oscADetune", "DETUNE" }, { "oscALevel", "LEVEL" } });
    addSection (0, "OSC B / SUB / NOISE", 4, 2, { { "oscBWave", "WAVE" }, { "oscBOct", "OCTAVE" }, { "oscBSemi", "SEMI" }, { "oscBFine", "FINE" },
                                                  { "oscBLevel", "LEVEL" }, { "subLevel", "SUB" }, { "noiseLevel", "NOISE" }, { "spread", "WIDTH" } });
    addSection (0, "FILTER", 3, 2, { { "cutoff", "CUTOFF" }, { "resonance", "RESO" }, { "filterSlope", "SLOPE" },
                                     { "filterEnvAmt", "ENV AMT" }, { "keyTrack", "KEY TRK" }, { "drive", "DRIVE" } });
    addSection (0, "FILTER ENV", 2, 2, { { "fAttack", "ATTACK" }, { "fDecay", "DECAY" }, { "fSustain", "SUSTAIN" }, { "fRelease", "RELEASE" } });

    addSection (1, "AMP ENV", 3, 2, { { "aAttack", "ATTACK" }, { "aDecay", "DECAY" }, { "aSustain", "SUSTAIN" },
                                      { "aRelease", "RELEASE" }, { "velSens", "VELOCITY" } });
    addSection (1, "MOTION", 3, 2, { { "lfoRate", "LFO RATE" }, { "lfoShape", "SHAPE" }, { "lfoPitch", "VIBRATO" },
                                     { "lfoFilter", "LFO>FILT" }, { "lfoAmp", "TREMOLO" }, { "warble", "TAPE WARBLE" } });
    addSection (1, "VOICE", 2, 2, { { "mono", "MONO" }, { "legato", "LEGATO" }, { "glide", "GLIDE" }, { "bendRange", "BEND" } });
    addSection (1, "MASTER", 2, 2, { { "muffle", "MUFFLE" }, { "masterGain", "GAIN" } });

    addSection (2, "CHORUS", 3, 1, { { "chorusMix", "MIX" }, { "chorusRate", "RATE" }, { "chorusDepth", "DEPTH" } });
    addSection (2, "DELAY", 4, 1, { { "delayDiv", "TIME" }, { "delayFeedback", "FEEDBACK" }, { "delayTone", "TONE" }, { "delayMix", "MIX" } });
    addSection (2, "REVERB", 4, 1, { { "reverbSize", "SIZE" }, { "reverbDamp", "DAMP" }, { "reverbWidth", "WIDTH" }, { "reverbMix", "MIX" } });

    for (int i = 0; i < processor.getNumPrograms(); ++i)
        presetBox.addItem (processor.getProgramName (i), i + 1);
    presetBox.setSelectedId (processor.getCurrentProgram() + 1, juce::dontSendNotification);
    presetBox.onChange = [this] { selectPreset (presetBox.getSelectedId() - 1); };
    prevButton.onClick = [this] { selectPreset ((processor.getCurrentProgram() + processor.getNumPrograms() - 1) % processor.getNumPrograms()); };
    nextButton.onClick = [this] { selectPreset ((processor.getCurrentProgram() + 1) % processor.getNumPrograms()); };
    addAndMakeVisible (presetBox);
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    setSize (1080, 632);
}

AfterhoursEditor::~AfterhoursEditor()
{
    setLookAndFeel (nullptr);
}

void AfterhoursEditor::addSection (int row, const juce::String& title, int cols, int rows,
                                   std::initializer_list<std::pair<const char*, const char*>> params)
{
    Section s;
    s.title = title;
    s.cols = cols;
    s.rows = rows;
    for (const auto& [id, text] : params)
    {
        s.controls.push_back (std::make_unique<ParamControl> (processor.apvts, id, text));
        addAndMakeVisible (*s.controls.back());
    }
    sections.push_back (std::move (s));

    if ((int) rowLayout.size() <= row)
        rowLayout.resize ((size_t) row + 1);
    rowLayout[(size_t) row].push_back (sections.size() - 1);
}

void AfterhoursEditor::selectPreset (int index)
{
    if (index < 0 || index == processor.getCurrentProgram())
        return;
    processor.setCurrentProgram (index);
    presetBox.setSelectedId (index + 1, juce::dontSendNotification);
}

void AfterhoursEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (colours::background.brighter (0.05f), 0.0f, 0.0f,
                                             colours::background, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // Header
    g.setFont (juce::Font (juce::FontOptions (30.0f).withStyle ("Bold")));
    g.setGradientFill (juce::ColourGradient (colours::accent, 16.0f, 0.0f, colours::accent2, 260.0f, 0.0f, false));
    g.drawText ("AFTERHOURS", 18, 12, 300, 36, juce::Justification::centredLeft);
    g.setColour (colours::dimText);
    g.setFont (juce::Font (juce::FontOptions (12.0f)));
    g.drawText ("late-night hip-hop synth", 222, 22, 220, 20, juce::Justification::centredLeft);

    for (const auto& s : sections)
    {
        const auto b = s.bounds.toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (colours::panelEdge);
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

        g.setColour (colours::accent);
        g.setFont (juce::Font (juce::FontOptions (12.0f).withStyle ("Bold")));
        g.drawText (s.title, s.bounds.getX() + 12, s.bounds.getY() + 6, s.bounds.getWidth() - 24, 16, juce::Justification::centredLeft);
    }
}

void AfterhoursEditor::resized()
{
    constexpr int gap = 10, titleH = 26, cellH = 86;
    auto area = getLocalBounds().reduced (gap);

    auto header = area.removeFromTop (46);
    auto presetArea = header.removeFromRight (380).withSizeKeepingCentre (380, 30);
    prevButton.setBounds (presetArea.removeFromLeft (30));
    nextButton.setBounds (presetArea.removeFromRight (30));
    presetBox.setBounds (presetArea.reduced (6, 0));
    area.removeFromTop (6);

    for (const auto& row : rowLayout)
    {
        int maxRows = 1, totalCols = 0;
        for (auto idx : row) { maxRows = juce::jmax (maxRows, sections[idx].rows); totalCols += sections[idx].cols; }

        const int h = titleH + maxRows * cellH + 6;
        auto rowArea = area.removeFromTop (h);
        area.removeFromTop (gap);

        const int available = rowArea.getWidth() - gap * ((int) row.size() - 1);
        int x = rowArea.getX();
        for (size_t n = 0; n < row.size(); ++n)
        {
            auto& s = sections[row[n]];
            const int w = (n + 1 == row.size()) ? rowArea.getRight() - x : available * s.cols / totalCols;
            s.bounds = { x, rowArea.getY(), w, h };
            x += w + gap;

            auto inner = s.bounds.reduced (4).withTrimmedTop (titleH - 4);
            const int cellW = inner.getWidth() / s.cols;
            for (size_t c = 0; c < s.controls.size(); ++c)
            {
                const int col = (int) c % s.cols, r = (int) c / s.cols;
                s.controls[c]->setBounds (inner.getX() + col * cellW, inner.getY() + r * cellH, cellW, cellH);
            }
        }
    }
}
