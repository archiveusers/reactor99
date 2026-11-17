#pragma once
#include "PluginProcessor.h"
#include <memory>

class ReactorContent;

class ReactorAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReactorAudioProcessorEditor (ReactorAudioProcessor&);
    ~ReactorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

private:
    ReactorAudioProcessor& proc;
    std::unique_ptr<ReactorContent> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReactorAudioProcessorEditor)
};
