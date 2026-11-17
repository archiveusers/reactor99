#pragma once
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <cstdint>

namespace r99
{
    // Centre frequencies of the four control rods (also used for UI labels).
    inline constexpr float rodFreqs[4] = { 120.0f, 600.0f, 2500.0f, 8000.0f };

    // Asymptotic soft limiter: transparent below 0.85, never exceeds 1.0.
    inline float softLimit (float x) noexcept
    {
        const float t = 0.85f;
        const float a = std::abs (x);
        if (a <= t) return x;
        const float over = (a - t) / (1.0f - t);
        return std::copysign (t + (1.0f - t) * std::tanh (over), x);
    }

    // Pentode-style asymmetric clipper: positive half clips harder than negative.
    inline float pentode (float y) noexcept
    {
        if (y >= 0.0f)
        {
            const float y2 = y * y;
            return y / std::sqrt (std::sqrt (1.0f + y2 * y2));
        }
        return y / std::sqrt (1.0f + y * y);
    }

    //==========================================================================
    // Zavalishin TPT state-variable filter, band-pass output with unity peak.
    struct SVF
    {
        float s1 = 0.0f, s2 = 0.0f, g = 0.1f, r2 = 1.0f, h = 1.0f;

        void set (float fc, float q, double fs) noexcept
        {
            const float f = juce::jmin (fc, (float) (0.45 * fs));
            g  = std::tan (juce::MathConstants<float>::pi * f / (float) fs);
            r2 = 1.0f / q;
            h  = 1.0f / (1.0f + r2 * g + g * g);
        }

        float bandpass (float x) noexcept
        {
            const float hp = (x - (r2 + g) * s1 - s2) * h;
            const float bp = g * hp + s1;
            s1 = g * hp + bp;
            const float lp = g * bp + s2;
            s2 = g * bp + lp;
            return bp * r2;
        }

        void reset() noexcept { s1 = s2 = 0.0f; }
    };

    //==========================================================================
    // One control rod: resonant band boost whose depth/Q follow the lever.
    struct Rod
    {
        float freq = 1000.0f;
        double sr = 44100.0;
        int counter = 0;
        std::array<SVF, 2> f;
        juce::SmoothedValue<float> pos;

        void prepare (double sampleRate, float centreHz)
        {
            sr = sampleRate;
            freq = centreHz;
            counter = 0;
            pos.reset (sampleRate, 0.03);
            pos.setCurrentAndTargetValue (0.0f);
            for (auto& s : f) s.reset();
        }

        void process (juce::AudioBuffer<float>& buf, int nCh, float target)
        {
            pos.setTargetValue (target);
            if (target <= 0.0001f && ! pos.isSmoothing())
                return;

            const int n = buf.getNumSamples();
            for (int i = 0; i < n; ++i)
            {
                const float p = pos.getNextValue();
                if ((counter++ & 15) == 0)
                {
                    const float q = 0.8f + 9.2f * p;
                    for (auto& s : f) s.set (freq, q, sr);
                }
                const float amt = 2.2f * p;
                for (int c = 0; c < nCh; ++c)
                {
                    float* d = buf.getWritePointer (c);
                    const float x = d[i];
                    d[i] = x + amt * f[(size_t) c].bandpass (x);
                }
            }
        }
    };

    //==========================================================================
    // Three-stage tube model, runs at the oversampled rate.
    struct Tube
    {
        struct Ch { float lp = 0.0f, env = 0.0f, dcx = 0.0f, dcy = 0.0f; };
        std::array<Ch, 2> ch;
        float lpCoef = 0.5f, envAtk = 0.01f, envRel = 0.001f, dcR = 0.999f;

        void prepare (double osRate)
        {
            const float twoPi = juce::MathConstants<float>::twoPi;
            lpCoef = 1.0f - std::exp (-twoPi * 12000.0f / (float) osRate);
            envAtk = 1.0f - std::exp (-1.0f / (0.005f * (float) osRate));
            envRel = 1.0f - std::exp (-1.0f / (0.120f * (float) osRate));
            dcR    = 1.0f - twoPi * 10.0f / (float) osRate;
            ch[0] = {}; ch[1] = {};
        }

        void process (juce::dsp::AudioBlock<float>& blk, float d0, float d1, float bias, float sag)
        {
            const size_t n = blk.getNumSamples();
            const size_t nCh = juce::jmin ((size_t) 2, blk.getNumChannels());
            const float b = bias * 0.8f;
            const float tb = std::tanh (b);

            for (size_t c = 0; c < nCh; ++c)
            {
                float* x = blk.getChannelPointer (c);
                auto& s = ch[c];

                for (size_t i = 0; i < n; ++i)
                {
                    const float d  = d0 + (d1 - d0) * ((float) i / (float) n);
                    const float k1 = 1.0f + 20.0f * d * d;
                    const float k2 = 1.0f + 5.0f * d * d;

                    // Stage 1: triode, bias shifts the operating point (even harmonics)
                    float y = std::tanh (k1 * x[i] + b) - tb;
                    s.lp += lpCoef * (y - s.lp);            // tame fizz between stages
                    y = s.lp;

                    // Stage 2: pentode / grid conduction (harder, asymmetric)
                    y = pentode (y * k2);

                    // Stage 3: power stage with supply sag
                    const float a = std::abs (y);
                    s.env += (a > s.env ? envAtk : envRel) * (a - s.env);
                    const float sagGain = 1.0f - sag * s.env * 0.35f;
                    y = pentode (y * sagGain * (1.0f + 1.5f * d));

                    // DC block (bias creates offset) + loudness compensation
                    const float out = y - s.dcx + dcR * s.dcy;
                    s.dcx = y;
                    s.dcy = out;
                    x[i] = out * (1.0f - 0.45f * d);
                }
            }
        }
    };

    //==========================================================================
    // Radiation leak: pink noise floor + bitcrusher + Poisson geiger clicks.
    struct Leak
    {
        struct Ch
        {
            float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
            float held = 0.0f, phase = 0.0f;
            uint32_t rng = 1u;
        };

        std::array<Ch, 2> ch;
        uint32_t clickRng = 2463534242u;
        float clickEnv = 0.0f, clickDecay = 0.99f;
        double sr = 44100.0;
        juce::SmoothedValue<float> amount;

        void prepare (double sampleRate)
        {
            sr = sampleRate;
            amount.reset (sampleRate, 0.05);
            amount.setCurrentAndTargetValue (0.0f);
            clickDecay = std::exp (-1.0f / (0.0015f * (float) sampleRate));
            clickEnv = 0.0f;
            ch[0] = {}; ch[1] = {};
            ch[0].rng = 0x1234567u;
            ch[1].rng = 0x7654321u;
        }

        static float white (uint32_t& s) noexcept
        {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            return (float) (int32_t) s * (1.0f / 2147483648.0f);
        }

        // returns number of geiger clicks generated in this block
        int process (juce::AudioBuffer<float>& buf, int nCh, float target, float heat)
        {
            amount.setTargetValue (target);
            if (target <= 0.0005f && ! amount.isSmoothing())
                return 0;

            int clicks = 0;
            const int n = buf.getNumSamples();

            for (int i = 0; i < n; ++i)
            {
                const float L = amount.getNextValue();
                if (L < 0.0005f) continue;

                const float noiseAmp = juce::Decibels::decibelsToGain (-70.0f + 40.0f * L);
                const float crushMix = juce::jmin (1.0f, L * 4.0f);
                const float levels   = std::pow (2.0f, (16.0f - 12.0f * L) - 1.0f);
                const float reduce   = 1.0f + 24.0f * L * L;
                const float rate     = 2.0f + 200.0f * L * (0.2f + heat);
                const float p        = rate / (float) sr;

                if ((white (clickRng) * 0.5f + 0.5f) < p)
                {
                    clickEnv = 1.0f;
                    ++clicks;
                }
                const float clickAmp = 0.35f * L;

                for (int c = 0; c < nCh; ++c)
                {
                    auto& s = ch[(size_t) c];
                    float* d = buf.getWritePointer (c);
                    float x = d[i];

                    // sample-rate + bit-depth reduction
                    s.phase += 1.0f;
                    if (s.phase >= reduce)
                    {
                        s.phase = std::fmod (s.phase, reduce);
                        s.held = std::round (x * levels) / levels;
                    }
                    x += crushMix * (s.held - x);

                    // pink noise (Paul Kellet)
                    const float w = white (s.rng);
                    s.b0 = 0.99886f * s.b0 + w * 0.0555179f;
                    s.b1 = 0.99332f * s.b1 + w * 0.0750759f;
                    s.b2 = 0.96900f * s.b2 + w * 0.1538520f;
                    s.b3 = 0.86650f * s.b3 + w * 0.3104856f;
                    s.b4 = 0.55000f * s.b4 + w * 0.5329522f;
                    s.b5 = -0.7616f * s.b5 - w * 0.0168980f;
                    const float pink = (s.b0 + s.b1 + s.b2 + s.b3 + s.b4 + s.b5 + s.b6 + w * 0.5362f) * 0.11f;
                    s.b6 = w * 0.115926f;

                    x += pink * noiseAmp + w * clickEnv * clickAmp;
                    d[i] = x;
                }
                clickEnv *= clickDecay;
            }
            return clicks;
        }
    };
}
