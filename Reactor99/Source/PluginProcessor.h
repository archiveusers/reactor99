#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <memory>
#include "DSP.h"

enum class ReactorState : int { Nominal = 0, Warning, Meltdown, Scram, Cooldown };

class ReactorAudioProcessor : public juce::AudioProcessor
{
public:
    ReactorAudioProcessor();
    ~ReactorAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "REACTOR 99"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::AudioProcessorValueTreeState apvts;

    // ---- telemetry for the GUI (written by audio thread, read by UI) ----
    std::atomic<float>    heat { 0.0f }, peakIn { 0.0f }, peakOut { 0.0f };
    std::atomic<float>    driveEffUI { 0.0f }, leakEffUI { 0.0f };
    std::atomic<int>      state { 0 };
    std::atomic<uint32_t> clickCount { 0 };
    std::atomic<bool>     scramRequest { false };

    static constexpr int scopeSize = 4096;
    std::array<std::atomic<float>, scopeSize> scope;
    std::atomic<int> scopeWrite { 0 };

    void triggerScram() { scramRequest.store (true); }

private:
    void processChunk (juce::AudioBuffer<float>& buf);
    void updateState (float dt, bool meltEnabled, float thr);

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 4096 };
    juce::AudioBuffer<float> dryBuf;

    r99::Tube tube;
    std::array<r99::Rod, 4> rods;
    r99::Leak leak;

    juce::SmoothedValue<float> inGainSm, mixSm, outSm;

    double sr = 44100.0;
    int maxBlock = 512;

    ReactorState st = ReactorState::Nominal;
    float stTimer = 0.0f, holdTimer = 0.0f, scramMulV = 1.0f, heatVal = 0.0f, prevDrive = 0.0f;

    int scopeDecim = 4, scopeCounter = 0, scopeW = 0;
    float scopeAcc = 0.0f;

    std::atomic<float> *pInGain = nullptr, *pDrive = nullptr, *pBias = nullptr, *pSag = nullptr,
                       *pRodPre = nullptr, *pLeak = nullptr, *pLeakDyn = nullptr,
                       *pMeltEn = nullptr, *pMeltThr = nullptr, *pMix = nullptr, *pOut = nullptr;
    std::array<std::atomic<float>*, 4> pRod {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReactorAudioProcessor)
};
