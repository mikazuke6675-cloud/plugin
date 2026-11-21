#pragma once
#include "PluginProcessor.h"

namespace afterhours
{
namespace colours
{
    const juce::Colour background { 0xff0c0a13 };
    const juce::Colour panel      { 0xff16131f };
    const juce::Colour panelEdge  { 0xff2a2440 };
    const juce::Colour accent     { 0xffb57cff };
    const juce::Colour accent2    { 0xffff6ec7 };
    const juce::Colour text       { 0xffe8e2f5 };
    const juce::Colour dimText    { 0xff8a83a3 };
    const juce::Colour knobBody   { 0xff221d33 };
}

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

// One labelled control (knob, dropdown or switch) bound to a parameter.
class ParamControl : public juce::Component
{
public:
    ParamControl (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& labelText);
    void resized() override;

private:
    juce::Label label;
    juce::Slider slider;
    juce::ComboBox combo;
    juce::ToggleButton toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   sliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   buttonAttachment;
    enum class Kind { knob, choice, toggle } kind = Kind::knob;
};
} // namespace afterhours

class AfterhoursEditor : public juce::AudioProcessorEditor
{
public:
    explicit AfterhoursEditor (AfterhoursProcessor&);
    ~AfterhoursEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Section
    {
        juce::String title;
        int cols = 1, rows = 1;
        std::vector<std::unique_ptr<afterhours::ParamControl>> controls;
        juce::Rectangle<int> bounds;
    };

    void addSection (int row, const juce::String& title, int cols, int rows,
                     std::initializer_list<std::pair<const char*, const char*>> params);
    void selectPreset (int index);

    AfterhoursProcessor& processor;
    afterhours::LookAndFeel lnf;
    std::vector<Section> sections;
    std::vector<std::vector<size_t>> rowLayout;

    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AfterhoursEditor)
};
