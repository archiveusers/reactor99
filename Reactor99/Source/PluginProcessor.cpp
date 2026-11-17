#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ReactorAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto f = [&] (const String& id, const String& name, float lo, float hi, float def)
    {
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name,
                                                            NormalisableRange<float> (lo, hi, 0.001f), def));
    };
    auto b = [&] (const String& id, const String& name, bool def)
    {
        p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def));
    };

    f ("in_gain",  "Input",           -24.0f, 36.0f, 0.0f);
    f ("drive",    "Core Drive",        0.0f,  1.0f, 0.35f);
    f ("bias",     "Bias",             -1.0f,  1.0f, 0.0f);
    f ("sag",      "Sag",               0.0f,  1.0f, 0.3f);
    for (int i = 1; i <= 4; ++i)
        f ("rod" + String (i) + "_pos", "Control Rod " + String (i), 0.0f, 1.0f, 0.0f);
    b ("rod_pre",  "Rods Pre-Tube", false);
    f ("leak",     "Radiation Leak",    0.0f,  1.0f, 0.0f);
    b ("leak_dyn", "Leak Dynamic",  true);
    b ("meltdown_en", "Meltdown Sequence", true);
    f ("meltdown_thr", "Meltdown Threshold", 0.5f, 1.0f, 0.85f);
    f ("mix",      "Mix",               0.0f,  1.0f, 1.0f);
    f ("out_gain", "Output",          -24.0f, 12.0f, 0.0f);
    b ("reduce_motion", "Reduce Motion", false);

    AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& x : p) layout.add (std::move (x));
    return layout;
}

ReactorAudioProcessor::ReactorAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "REACTOR99", createLayout())
{
    for (auto& s : scope) s.store (0.0f);

    pInGain   = apvts.getRawParameterValue ("in_gain");
    pDrive    = apvts.getRawParameterValue ("drive");
    pBias     = apvts.getRawParameterValue ("bias");
    pSag      = apvts.getRawParameterValue ("sag");
    pRodPre   = apvts.getRawParameterValue ("rod_pre");
    pLeak     = apvts.getRawParameterValue ("leak");
    pLeakDyn  = apvts.getRawParameterValue ("leak_dyn");
    pMeltEn   = apvts.getRawParameterValue ("meltdown_en");
    pMeltThr  = apvts.getRawParameterValue ("meltdown_thr");
    pMix      = apvts.getRawParameterValue ("mix");
    pOut      = apvts.getRawParameterValue ("out_gain");
    for (int i = 0; i < 4; ++i)
        pRod[(size_t) i] = apvts.getRawParameterValue ("rod" + juce::String (i + 1) + "_pos");
}

bool ReactorAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == l.getMainInputChannelSet();
}

//==============================================================================
void ReactorAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = juce::jmax (32, samplesPerBlock);

    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampler->initProcessing ((size_t) maxBlock);
    oversampler->reset();

    const int latency = juce::roundToInt (oversampler->getLatencyInSamples());
    setLatencySamples (latency);

    const auto chans = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());
    dryDelay.prepare ({ sampleRate, (juce::uint32) maxBlock, chans });
    dryDelay.setDelay ((float) latency);
    dryDelay.reset();
    dryBuf.setSize (2, maxBlock);
    dryBuf.clear();

    tube.prepare (sampleRate * 4.0);
    for (int i = 0; i < 4; ++i) rods[(size_t) i].prepare (sampleRate, r99::rodFreqs[i]);
    leak.prepare (sampleRate);

    inGainSm.reset (sampleRate, 0.02);
    mixSm.reset (sampleRate, 0.02);
    outSm.reset (sampleRate, 0.02);
    inGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    mixSm.setCurrentAndTargetValue (pMix->load());
    outSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOut->load()));

    scopeDecim = juce::jmax (1, (int) (sampleRate / 12000.0));
    scopeCounter = 0;

    st = ReactorState::Nominal;
    stTimer = holdTimer = 0.0f;
    scramMulV = 1.0f;
    heatVal = 0.0f;
    prevDrive = pDrive->load();
}

//==============================================================================
void ReactorAudioProcessor::updateState (float dt, bool meltEnabled, float thr)
{
    const bool req = scramRequest.exchange (false);
    if (req && st != ReactorState::Scram)
    {
        st = ReactorState::Scram;
        stTimer = 0.0f;
    }

    const float warnThr = juce::jmin (0.7f, thr - 0.1f);

    switch (st)
    {
        case ReactorState::Nominal:
            holdTimer = 0.0f;
            if (meltEnabled && heatVal > warnThr) st = ReactorState::Warning;
            break;

        case ReactorState::Warning:
            if (! meltEnabled || heatVal < warnThr - 0.1f)
            {
                st = ReactorState::Nominal;
                holdTimer = 0.0f;
            }
            else if (heatVal > thr)
            {
                holdTimer += dt;
                if (holdTimer > 0.15f) { st = ReactorState::Meltdown; stTimer = 0.0f; }
            }
            else holdTimer = 0.0f;
            break;

        case ReactorState::Meltdown:
            stTimer += dt;
            if (stTimer > 6.0f) { st = ReactorState::Scram; stTimer = 0.0f; }   // auto-SCRAM
            break;

        case ReactorState::Scram:
            stTimer += dt;
            if (stTimer > 2.5f) { st = ReactorState::Cooldown; stTimer = 0.0f; }
            break;

        case ReactorState::Cooldown:
            stTimer += dt;
            if (stTimer > 1.0f && (heatVal < 0.6f || stTimer > 6.0f))
            {
                st = ReactorState::Nominal;
                holdTimer = 0.0f;
            }
            break;
    }

    // SCRAM override multiplier: drops to 0 in ~40 ms, returns over ~300 ms.
    if (st == ReactorState::Scram) scramMulV = juce::jmax (0.0f, scramMulV - dt / 0.04f);
    else                           scramMulV = juce::jmin (1.0f, scramMulV + dt / 0.30f);
}

//==============================================================================
void ReactorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    const int total = buffer.getNumSamples();
    const int nCh = juce::jmin (2, buffer.getNumChannels());
    if (nCh <= 0 || ! oversampler) return;

    int pos = 0;
    while (pos < total)
    {
        const int len = juce::jmin (maxBlock, total - pos);
        juce::AudioBuffer<float> chunk (buffer.getArrayOfWritePointers(), nCh, pos, len);
        processChunk (chunk);
        pos += len;
    }
}

void ReactorAudioProcessor::processChunk (juce::AudioBuffer<float>& buf)
{
    const int n = buf.getNumSamples();
    const int nCh = juce::jmin (2, buf.getNumChannels());
    const float dt = (float) n / (float) sr;

    // ---- meltdown state machine (block rate) ---------------------------------
    updateState (dt, pMeltEn->load() > 0.5f, pMeltThr->load());
    const float scramMul = scramMulV;

    const float driveEff = pDrive->load() * scramMul;
    const float heatN = juce::jlimit (0.0f, 1.0f, heatVal);

    float leakEff = pLeak->load();
    if (pLeakDyn->load() > 0.5f) leakEff *= (0.3f + 0.7f * heatN);
    if (st == ReactorState::Meltdown) leakEff = juce::jmax (leakEff, 0.4f);   // meltdown gimmick
    leakEff *= scramMul;                                                      // SCRAM kills the leak

    // ---- dry copy (latency compensated later) ------------------------------------
    for (int c = 0; c < nCh; ++c) dryBuf.copyFrom (c, 0, buf, c, 0, n);
    {
        juce::dsp::AudioBlock<float> dryBlock (dryBuf.getArrayOfWritePointers(), (size_t) nCh, (size_t) n);
        dryDelay.process (juce::dsp::ProcessContextReplacing<float> (dryBlock));
    }

    // ---- input gain + metering ----------------------------------------------
    inGainSm.setTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    float* ch[2] = { buf.getWritePointer (0), nCh > 1 ? buf.getWritePointer (1) : nullptr };
    double sumSq = 0.0;
    float pkIn = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = inGainSm.getNextValue();
        for (int c = 0; c < nCh; ++c)
        {
            const float v = ch[c][i] * g;
            ch[c][i] = v;
            sumSq += (double) v * (double) v;
            pkIn = juce::jmax (pkIn, std::abs (v));
        }
    }

    // ---- control rods (pre) --------------------------------------------------
    const bool rodPre = pRodPre->load() > 0.5f;
    if (rodPre)
        for (int r = 0; r < 4; ++r) rods[(size_t) r].process (buf, nCh, pRod[(size_t) r]->load());

    // ---- oversampled tube stages --------------------------------------------
    {
        juce::dsp::AudioBlock<float> block (buf.getArrayOfWritePointers(), (size_t) nCh, (size_t) n);
        auto osBlock = oversampler->processSamplesUp (block);
        tube.process (osBlock, prevDrive, driveEff, pBias->load(), pSag->load());
        oversampler->processSamplesDown (block);
        prevDrive = driveEff;
    }

    // ---- control rods (post) -------------------------------------------------
    if (! rodPre)
        for (int r = 0; r < 4; ++r) rods[(size_t) r].process (buf, nCh, pRod[(size_t) r]->load());

    // ---- radiation leak -------------------------------------------------------
    const int clicks = leak.process (buf, nCh, leakEff, heatN);
    if (clicks > 0) clickCount.fetch_add ((uint32_t) clicks);

    // ---- mix, output gain, safety limiter, scope ------------------------------
    mixSm.setTargetValue (pMix->load());
    outSm.setTargetValue (juce::Decibels::decibelsToGain (pOut->load()));
    float pkOut = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float m = mixSm.getNextValue();
        const float g = outSm.getNextValue();
        float mono = 0.0f;
        for (int c = 0; c < nCh; ++c)
        {
            const float wet = ch[c][i];
            const float dry = dryBuf.getSample (c, i);
            float y = r99::softLimit ((dry * (1.0f - m) + wet * m) * g);
            if (! std::isfinite (y)) y = 0.0f;
            ch[c][i] = y;
            pkOut = juce::jmax (pkOut, std::abs (y));
            mono += y;
        }
        mono /= (float) nCh;

        if (++scopeCounter >= scopeDecim)
        {
            scopeCounter = 0;
            scope[(size_t) scopeW].store (mono, std::memory_order_relaxed);
            scopeW = (scopeW + 1) % scopeSize;
        }
    }
    scopeWrite.store (scopeW);

    // ---- heat model ----------------------------------------------------------
    const float rms = (float) std::sqrt (sumSq / (double) juce::jmax (1, n * nCh));
    const float load = rms * (0.4f + 1.6f * driveEff);
    const float target = juce::jlimit (0.0f, 1.2f, load * 1.15f);
    const float tau = target > heatVal ? 0.25f : 1.8f;
    heatVal += (target - heatVal) * (1.0f - std::exp (-dt / tau));
    if (! std::isfinite (heatVal)) heatVal = 0.0f;

    // ---- publish telemetry -----------------------------------------------------
    heat.store (heatVal);
    state.store ((int) st);
    driveEffUI.store (driveEff);
    leakEffUI.store (leakEff);
    peakIn.store (juce::jmax (peakIn.load(), pkIn));
    peakOut.store (juce::jmax (peakOut.load(), pkOut));
}

//==============================================================================
juce::AudioProcessorEditor* ReactorAudioProcessor::createEditor()
{
    return new ReactorAudioProcessorEditor (*this);
}

void ReactorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto s = apvts.copyState();
    if (auto xml = s.createXml()) copyXmlToBinary (*xml, destData);
}

void ReactorAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReactorAudioProcessor();
}
