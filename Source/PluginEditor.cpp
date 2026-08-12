#include "PluginEditor.h"

#include <cmath>

namespace
{
struct ControlSpec
{
    const char* id;
    const char* label;
    const char* tip;
};

constexpr ControlSpec controls[] {
    { "clock", "CLOCK", "Clock division and micro-buffer cadence." },
    { "wordSize", "WORD", "Bit word size for integer corruption." },
    { "opMorph", "OP", "Morph between bitwise operations." },
    { "mask", "MASK", "Bit mask pressure for the corruptor." },
    { "jitter", "JITTER", "Clock instability and short timing offsets." },
    { "stutter", "STUTTER", "Micro-repeat probability and length." },
    { "feedback", "FDBK", "Internal feedback into the glitch network." },
    { "density", "DENS", "Density of destructive events." },
    { "outputDb", "OUTPUT", "Final output trim in dB." },
};

static_assert (std::size (controls) == 9);

float normalizedSliderValue (juce::Slider& slider) noexcept
{
    const auto normalized = static_cast<float> (slider.valueToProportionOfLength (slider.getValue()));
    return std::isfinite (normalized) ? juce::jlimit (0.0f, 1.0f, normalized) : 0.0f;
}
} // namespace

GlitchNoiseAudioProcessorEditor::GlitchNoiseAudioProcessorEditor (GlitchNoiseAudioProcessor& p)
: AudioProcessorEditor (&p), audioProcessor (p),
  tooltipText ("GlitchNoiseJUCE: integer and bitwise glitch controls with preset load, randomize locks, limiter A/B, panic, and output trim.")
{
    setLookAndFeel (&lookAndFeel);
    setResizeLimits (minimumWidth, minimumHeight,
                     ehl::juce_design::Metrics::maximumWidth,
                     ehl::juce_design::Metrics::maximumHeight);
    setResizable (true, true);
    setName ("GlitchNoiseJUCE editor");
    setComponentID ("glitchnoisejuce-editor");
    setTitle ("GlitchNoiseJUCE");
    setDescription ("GlitchNoiseJUCE monochrome 8-bit glitch editor");
    setWantsKeyboardFocus (true);

    parameterDisplay.setComponentID ("glitchnoisejuce-parameter-display");
    parameterDisplay.setName ("GlitchNoiseJUCE parameter display");
    addAndMakeVisible (parameterDisplay);

    for (int i = 0; i < static_cast<int> (std::size (controls)); ++i)
        addSlider (i, controls[i].id, controls[i].label, controls[i].tip);

    for (int i = 0; i < static_cast<int> (lockButtons.size()); ++i)
        addLock (i, controls[i].id);

    ehl::juce_design::styleToggle (limiterButton);
    limiterButton.setButtonText ("LIMIT");
    limiterButton.setComponentID ("glitchnoisejuce-limiterOn");
    limiterButton.setName ("Limiter");
    limiterButton.setTooltip ("Limiter A/B; randomize keeps it on by default.");
    limiterButton.setWantsKeyboardFocus (true);
    addAndMakeVisible (limiterButton);
    limiterAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, "limiterOn", limiterButton);

    ehl::juce_design::styleComboBox (presetBox);
    presetBox.setComponentID ("glitchnoisejuce-preset");
    presetBox.setName ("Preset");
    presetBox.setTooltip ("Load a GlitchNoiseJUCE preset.");
    for (int i = 0; i < audioProcessor.getPresetCount(); ++i)
        presetBox.addItem (audioProcessor.getPresetName (i), i + 1);
    presetBox.onChange = [this]
    {
        if (presetBox.getSelectedId() > 0)
            audioProcessor.loadPreset (presetBox.getSelectedId() - 1);
    };
    presetBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (presetBox);

    styleCommandButton (randomizeButton, "RAND", "glitchnoisejuce-randomize");
    randomizeButton.setTooltip ("Randomize unlocked macro parameters.");
    randomizeButton.onClick = [this] { audioProcessor.randomizeParams(); };
    addAndMakeVisible (randomizeButton);

    styleCommandButton (panicButton, "PANIC", "glitchnoisejuce-panic");
    panicButton.setTooltip ("Clear the next block and reset transient glitch state.");
    panicButton.onClick = [this] { audioProcessor.triggerPanic(); };
    addAndMakeVisible (panicButton);

    updateParameterDisplay();
    startTimerHz (30);
    setSize (defaultWidth, defaultHeight);
}

GlitchNoiseAudioProcessorEditor::~GlitchNoiseAudioProcessorEditor()
{
    stopTimer();
    for (auto& slider : sliders)
        slider.setLookAndFeel (nullptr);
    for (auto& label : labels)
        label.setLookAndFeel (nullptr);
    for (auto& lock : lockButtons)
        lock.setLookAndFeel (nullptr);
    limiterButton.setLookAndFeel (nullptr);
    presetBox.setLookAndFeel (nullptr);
    randomizeButton.setLookAndFeel (nullptr);
    panicButton.setLookAndFeel (nullptr);
    tooltipWindow.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void GlitchNoiseAudioProcessorEditor::addSlider (int index, const juce::String& parameterId, const juce::String& labelText, const juce::String& tip)
{
    auto& slider = sliders[static_cast<std::size_t> (index)];
    ehl::juce_design::styleSlider (slider);
    slider.setComponentID ("glitchnoisejuce-control-" + parameterId);
    slider.setName ("GlitchNoiseJUCE " + labelText);
    slider.setTitle (labelText);
    slider.setDescription (tip);
    slider.setTooltip (tip);
    slider.setWantsKeyboardFocus (true);
    addAndMakeVisible (slider);

    auto& label = labels[static_cast<std::size_t> (index)];
    label.setText (labelText, juce::dontSendNotification);
    ehl::juce_design::styleLabel (label);
    label.setComponentID ("glitchnoisejuce-label-" + parameterId);
    label.setName (labelText);
    label.setTooltip (tip);
    addAndMakeVisible (label);

    sliderAttachments[static_cast<std::size_t> (index)] = std::make_unique<SliderAttachment> (audioProcessor.apvts, parameterId, slider);
}

void GlitchNoiseAudioProcessorEditor::addLock (int index, const juce::String& parameterId)
{
    auto& lock = lockButtons[static_cast<std::size_t> (index)];
    ehl::juce_design::styleToggle (lock);
    lock.setButtonText ("L");
    lock.setComponentID ("glitchnoisejuce-lock-" + parameterId);
    lock.setName ("Lock " + parameterId);
    lock.setTooltip ("Lock " + parameterId + " during randomize.");
    lock.setWantsKeyboardFocus (true);
    lock.onClick = [this, parameterId, &lock] { audioProcessor.setLock (parameterId, lock.getToggleState()); };
    addAndMakeVisible (lock);
}

void GlitchNoiseAudioProcessorEditor::styleCommandButton (juce::TextButton& button, const juce::String& text, const juce::String& componentId)
{
    button.setButtonText (text);
    button.setComponentID (componentId);
    button.setName (text);
    button.setWantsKeyboardFocus (true);
    button.setColour (juce::TextButton::buttonColourId, ehl::juce_design::Palette::ink());
    button.setColour (juce::TextButton::buttonOnColourId, ehl::juce_design::Palette::paper());
    button.setColour (juce::TextButton::textColourOffId, ehl::juce_design::Palette::paper());
    button.setColour (juce::TextButton::textColourOnId, ehl::juce_design::Palette::ink());
}

void GlitchNoiseAudioProcessorEditor::paint (juce::Graphics& g)
{
    ehl::juce_design::paintEditorChrome (g, getLocalBounds(), "GlitchNoiseJUCE", "BITWISE GLITCH");
}

void GlitchNoiseAudioProcessorEditor::resized()
{
    parameterDisplay.setBounds (ehl::juce_design::parameterDisplayArea (getLocalBounds()));

    for (int i = 0; i < static_cast<int> (sliders.size()); ++i)
        ehl::juce_design::layoutLabelledControl (labels[static_cast<std::size_t> (i)],
                                                 sliders[static_cast<std::size_t> (i)],
                                                 ehl::juce_design::controlCell (getLocalBounds(), static_cast<std::size_t> (i)));

    for (int i = 0; i < static_cast<int> (lockButtons.size()); ++i)
    {
        auto control = sliders[static_cast<std::size_t> (i)].getBounds();
        lockButtons[static_cast<std::size_t> (i)].setBounds (control.removeFromRight (28).removeFromTop (24));
    }

    auto presetCell = ehl::juce_design::controlCell (getLocalBounds(), 9);
    auto randomizeCell = ehl::juce_design::controlCell (getLocalBounds(), 10);
    auto finalCell = ehl::juce_design::controlCell (getLocalBounds(), 11);
    presetBox.setBounds (presetCell.reduced (0, ehl::juce_design::Metrics::labelHeight));
    randomizeButton.setBounds (randomizeCell.reduced (0, ehl::juce_design::Metrics::labelHeight));

    finalCell.reduce (0, ehl::juce_design::Metrics::labelHeight);
    limiterButton.setBounds (finalCell.removeFromTop (juce::jmin (40, finalCell.getHeight() / 2)).reduced (0, 2));
    panicButton.setBounds (finalCell.reduced (0, 2));
}

void GlitchNoiseAudioProcessorEditor::timerCallback()
{
    updateParameterDisplay();
}

void GlitchNoiseAudioProcessorEditor::updateParameterDisplay()
{
    parameterDisplay.setValues ({ normalizedSliderValue (sliders[0]),
                                  normalizedSliderValue (sliders[1]),
                                  normalizedSliderValue (sliders[2]),
                                  normalizedSliderValue (sliders[3]) });
}
