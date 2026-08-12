#pragma once
#include "PluginProcessor.h"

#include <ehl/juce_design/EhlDesign.h>
#include <JuceHeader.h>

#include <array>
#include <memory>

class GlitchNoiseAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit GlitchNoiseAudioProcessorEditor (GlitchNoiseAudioProcessor&);
    ~GlitchNoiseAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    juce::String getTooltip() { return tooltipText; }

    static constexpr int defaultWidth = ehl::juce_design::Metrics::defaultWidth;
    static constexpr int defaultHeight = ehl::juce_design::Metrics::defaultHeight;
    static constexpr int minimumWidth = ehl::juce_design::Metrics::minimumWidth;
    static constexpr int minimumHeight = ehl::juce_design::Metrics::minimumHeight;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void addSlider (int index, const juce::String& parameterId, const juce::String& labelText, const juce::String& tip);
    void addLock (int index, const juce::String& parameterId);
    void styleCommandButton (juce::TextButton& button, const juce::String& text, const juce::String& componentId);
    void timerCallback() override;
    void updateParameterDisplay();

    GlitchNoiseAudioProcessor& audioProcessor;
    ehl::juce_design::LookAndFeel lookAndFeel;
    ehl::juce_design::ParameterDisplay parameterDisplay { ehl::juce_design::DisplayKind::bitcrusher };
    juce::TooltipWindow tooltipWindow { this, 700 };
    juce::String tooltipText;

    std::array<juce::Slider, 9> sliders;
    std::array<juce::Label, 9> labels;
    std::array<juce::ToggleButton, 8> lockButtons;
    std::array<std::unique_ptr<SliderAttachment>, 9> sliderAttachments;
    std::unique_ptr<ButtonAttachment> limiterAttachment;

    juce::ToggleButton limiterButton;
    juce::ComboBox presetBox;
    juce::TextButton randomizeButton;
    juce::TextButton panicButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlitchNoiseAudioProcessorEditor)
};
