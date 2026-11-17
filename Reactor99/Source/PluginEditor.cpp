#include "PluginEditor.h"
#include <cmath>

namespace
{
    const juce::Colour kAmber (0xffffb000), kRed (0xffff2a1a), kBlue (0xff2f8cff),
                       kGreen (0xff35e06a), kCyan (0xff3de7ff);

    constexpr float kPi    = juce::MathConstants<float>::pi;
    constexpr float kTwoPi = juce::MathConstants<float>::twoPi;

    juce::Font mkFont (float h, bool bold = false)
    {
        return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font mkMono (float h)
    {
        return juce::Font (juce::FontOptions ("Consolas", h, juce::Font::plain));
    }

    juce::Colour heatColour (float h)
    {
        h = juce::jlimit (0.0f, 1.2f, h);
        if (h < 0.55f) return kBlue.interpolatedWith (kAmber, h / 0.55f);
        return kAmber.interpolatedWith (kRed, juce::jlimit (0.0f, 1.0f, (h - 0.55f) / 0.35f));
    }

    void drawRivet (juce::Graphics& g, float x, float y)
    {
        juce::ColourGradient rg (juce::Colour (0xffb9c0c8), x - 1.5f, y - 1.5f,
                                 juce::Colour (0xff2a2e33), x + 3.0f, y + 3.0f, true);
        g.setGradientFill (rg);
        g.fillEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f, 0.8f);
    }

    void drawLamp (juce::Graphics& g, float cx, float cy, float r, juce::Colour c, bool on)
    {
        g.setColour (juce::Colour (0xff0b0c0e));
        g.fillEllipse (cx - r - 2.0f, cy - r - 2.0f, 2.0f * (r + 2.0f), 2.0f * (r + 2.0f));
        if (on)
        {
            juce::ColourGradient glow (c.withAlpha (0.55f), cx, cy, c.withAlpha (0.0f), cx + r * 3.2f, cy, true);
            g.setGradientFill (glow);
            g.fillEllipse (cx - r * 3.2f, cy - r * 3.2f, r * 6.4f, r * 6.4f);
            juce::ColourGradient core (c.brighter (0.6f), cx - r * 0.3f, cy - r * 0.3f, c.darker (0.3f), cx + r, cy + r, true);
            g.setGradientFill (core);
        }
        else
            g.setColour (c.darker (0.85f));
        g.fillEllipse (cx - r, cy - r, 2.0f * r, 2.0f * r);
    }
}

//==============================================================================
// Look and feel: metal knobs (and the big geiger knob with a radiation trefoil)
class ReactorLNF : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
        const float rad = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const bool big = s.getName() == "leak";
        const float angle = startAngle + pos * (endAngle - startAngle);

        juce::Path track;
        track.addCentredArc (c.x, c.y, rad - 3.0f, rad - 3.0f, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.strokePath (track, juce::PathStrokeType (4.0f));

        juce::Path val;
        val.addCentredArc (c.x, c.y, rad - 3.0f, rad - 3.0f, 0.0f, startAngle, angle, true);
        g.setColour (big ? heatColour (0.45f + 0.55f * pos) : kAmber);
        g.strokePath (val, juce::PathStrokeType (4.0f));

        const float bodyR = rad * 0.74f;
        juce::ColourGradient body (juce::Colour (0xff9aa2ab), c.x - bodyR * 0.5f, c.y - bodyR * 0.6f,
                                   juce::Colour (0xff1f2327), c.x + bodyR * 0.7f, c.y + bodyR * 0.8f, false);
        g.setGradientFill (body);
        g.fillEllipse (c.x - bodyR, c.y - bodyR, 2.0f * bodyR, 2.0f * bodyR);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (c.x - bodyR, c.y - bodyR, 2.0f * bodyR, 2.0f * bodyR, 1.5f);

        if (big)
        {
            // ribbed edge
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            for (int i = 0; i < 36; ++i)
            {
                const float a = angle + (float) i * kTwoPi / 36.0f;
                g.drawLine (c.x + std::sin (a) * bodyR * 0.88f, c.y - std::cos (a) * bodyR * 0.88f,
                            c.x + std::sin (a) * bodyR, c.y - std::cos (a) * bodyR, 1.5f);
            }
            // radiation trefoil, rotates with the knob
            const float tr = bodyR * 0.55f;
            juce::Path tre;
            for (int i = 0; i < 3; ++i)
            {
                const float from = angle + (float) i * kTwoPi / 3.0f - kPi / 6.0f;
                tre.addPieSegment (c.x - tr, c.y - tr, 2.0f * tr, 2.0f * tr, from, from + kPi / 3.0f, 0.28f);
            }
            g.setColour (juce::Colour (0xffe8c010));
            g.fillPath (tre);
            g.setColour (juce::Colour (0xff15171a));
            g.fillEllipse (c.x - tr * 0.2f, c.y - tr * 0.2f, tr * 0.4f, tr * 0.4f);
        }

        const float inner = big ? bodyR * 0.72f : bodyR * 0.25f;
        g.setColour (big ? kRed : juce::Colours::white);
        g.drawLine (c.x + std::sin (angle) * inner, c.y - std::cos (angle) * inner,
                    c.x + std::sin (angle) * bodyR * 0.95f, c.y - std::cos (angle) * bodyR * 0.95f, big ? 4.0f : 3.0f);
    }
};

//==============================================================================
// Static steel background (cached to an image by JUCE)
class Background : public juce::Component
{
public:
    Background()
    {
        setInterceptsMouseClicks (false, false);
        setOpaque (true);
        setBufferedToImage (true);
    }

    void paint (juce::Graphics& g) override
    {
        const float W = (float) getWidth(), H = (float) getHeight();

        juce::ColourGradient bg (juce::Colour (0xff4a5058), 0.0f, 0.0f, juce::Colour (0xff23262a), 0.0f, H, false);
        g.setGradientFill (bg);
        g.fillRect (getLocalBounds());

        juce::Random r (99);
        for (int i = 0; i < 1600; ++i)
        {
            const float y = r.nextFloat() * H, x = r.nextFloat() * W, len = 30.0f + r.nextFloat() * 260.0f;
            g.setColour ((i & 1 ? juce::Colours::white : juce::Colours::black).withAlpha (r.nextFloat() * 0.04f));
            g.drawHorizontalLine ((int) y, x, x + len);
        }

        // header
        g.setColour (juce::Colour (0xff121417));
        g.fillRect (0.0f, 0.0f, W, 46.0f);
        g.setFont (mkFont (30.0f, true));
        g.setColour (kAmber);
        g.drawText ("REACTOR 99", 24, 4, 240, 38, juce::Justification::centredLeft);
        g.setFont (mkFont (10.0f, true));
        g.setColour (kAmber.withAlpha (0.55f));
        g.drawText ("NUCLEAR SATURATION UNIT // MK-VII", 232, 16, 240, 14, juce::Justification::centredLeft);

        // hazard stripe
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (0, 44, (int) W, 8);
            g.setColour (juce::Colour (0xffd9a800));
            g.fillRect (0, 44, (int) W, 8);
            g.setColour (juce::Colour (0xff111111));
            for (float x = -20.0f; x < W; x += 24.0f)
            {
                juce::Path p;
                p.addQuadrilateral (x, 52.0f, x + 12.0f, 52.0f, x + 20.0f, 44.0f, x + 8.0f, 44.0f);
                g.fillPath (p);
            }
        }

        // scope bezel
        {
            juce::Rectangle<float> bz (32.0f, 56.0f, 936.0f, 146.0f);
            g.setColour (juce::Colour (0xff0d0f11));
            g.fillRoundedRectangle (bz, 10.0f);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.drawRoundedRectangle (bz.reduced (1.0f), 10.0f, 1.5f);
        }

        drawPanel (g, { 24.0f, 208.0f, 276.0f, 330.0f }, "CONTROL RODS");
        drawPanel (g, { 310.0f, 208.0f, 380.0f, 330.0f }, "CORE TEMP");
        drawPanel (g, { 700.0f, 208.0f, 276.0f, 330.0f }, "RADIATION LEAK");
        drawPanel (g, { 24.0f, 546.0f, 952.0f, 88.0f }, {});

        // rod labels
        g.setFont (mkFont (11.0f, true));
        const char* freqs[4] = { "120 Hz", "600 Hz", "2.5 kHz", "8 kHz" };
        for (int i = 0; i < 4; ++i)
        {
            const float cx = 24.0f + 69.0f * (float) i + 34.5f;
            g.setColour (kAmber);
            g.drawText ("ROD " + juce::String (i + 1), (int) cx - 34, 474, 68, 14, juce::Justification::centred);
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.drawText (freqs[i], (int) cx - 34, 488, 68, 14, juce::Justification::centred);
        }

        // knob labels
        knobLabel (g, 400.0f, 524.0f, "DRIVE");
        knobLabel (g, 500.0f, 524.0f, "BIAS");
        knobLabel (g, 600.0f, 524.0f, "SAG");
        knobLabel (g, 838.0f, 436.0f, "GEIGER");
        knobLabel (g,  80.0f, 614.0f, "INPUT");
        knobLabel (g, 230.0f, 614.0f, "MIX");
        knobLabel (g, 340.0f, 614.0f, "MELT THR");
        knobLabel (g, 880.0f, 614.0f, "OUTPUT");
    }

private:
    static void knobLabel (juce::Graphics& g, float cx, float y, const juce::String& t)
    {
        g.setFont (mkFont (10.0f, true));
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawText (t, (int) cx - 40, (int) y, 80, 12, juce::Justification::centred);
    }

    static void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title)
    {
        g.setColour (juce::Colour (0xff15171a).withAlpha (0.55f));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.drawRoundedRectangle (r, 6.0f, 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (r.reduced (2.0f), 5.0f, 1.0f);
        drawRivet (g, r.getX() + 9.0f, r.getY() + 9.0f);
        drawRivet (g, r.getRight() - 9.0f, r.getY() + 9.0f);
        drawRivet (g, r.getX() + 9.0f, r.getBottom() - 9.0f);
        drawRivet (g, r.getRight() - 9.0f, r.getBottom() - 9.0f);

        if (title.isNotEmpty())
        {
            const juce::Rectangle<float> plate (r.getCentreX() - 70.0f, r.getY() + 5.0f, 140.0f, 18.0f);
            g.setColour (juce::Colour (0xff0b0c0e));
            g.fillRoundedRectangle (plate, 3.0f);
            g.setColour (kAmber);
            g.setFont (mkFont (11.0f, true));
            g.drawText (title, plate.toNearestInt(), juce::Justification::centred);
        }
    }
};

//==============================================================================
// Oscilloscope / seismograph
class ScopeView : public juce::Component
{
public:
    explicit ScopeView (ReactorAudioProcessor& p) : proc (p) { setInterceptsMouseClicks (true, false); }

    void update (float h) { heat = h; repaint(); }
    void mouseDown (const juce::MouseEvent&) override { seismic = ! seismic; }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        g.setColour (juce::Colour (0xff031208));
        g.fillRoundedRectangle (r, 6.0f);

        g.setColour (juce::Colour (0xff35ff7a).withAlpha (0.07f));
        for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * (float) i / 8.0f), r.getY(), r.getBottom());
        for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * (float) i / 4.0f), r.getX(), r.getRight());

        constexpr int kSamples = 2048;
        const int total = ReactorAudioProcessor::scopeSize;
        const int w = proc.scopeWrite.load();
        std::array<float, kSamples> tmp;
        for (int i = 0; i < kSamples; ++i)
            tmp[(size_t) i] = proc.scope[(size_t) ((w - kSamples + i + total) % total)].load (std::memory_order_relaxed);

        const int N = seismic ? kSamples : 1024;
        int trig = 0;
        if (! seismic)
            for (int i = 1; i < kSamples - N; ++i)
                if (tmp[(size_t) i - 1] < 0.0f && tmp[(size_t) i] >= 0.0f) { trig = i; break; }

        const float mid = r.getCentreY(), amp = r.getHeight() * 0.45f;
        juce::Path path;
        for (int j = 0; j < N; ++j)
        {
            const float x = r.getX() + r.getWidth() * (float) j / (float) (N - 1);
            const float y = mid - juce::jlimit (-1.0f, 1.0f, tmp[(size_t) (trig + j)]) * amp;
            if (j == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
        }

        const auto col = juce::Colour (0xff35ff7a).interpolatedWith (kRed, juce::jlimit (0.0f, 1.0f, (heat - 0.6f) / 0.4f));
        g.setColour (col.withAlpha (0.12f)); g.strokePath (path, juce::PathStrokeType (7.0f));
        g.setColour (col.withAlpha (0.28f)); g.strokePath (path, juce::PathStrokeType (3.5f));
        g.setColour (col);                   g.strokePath (path, juce::PathStrokeType (1.4f));

        g.setFont (mkMono (11.0f));
        g.setColour (col.withAlpha (0.7f));
        g.drawText (seismic ? "SEISMIC  [click: scope]" : "CH1 CORE OUT  [click: seismic]",
                    getLocalBounds().reduced (8, 4), juce::Justification::topLeft);

        // glass sheen
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.07f), 0.0f, r.getY(),
                                    juce::Colours::white.withAlpha (0.0f), 0.0f, r.getCentreY(), false);
        g.setGradientFill (sheen);
        g.fillRoundedRectangle (r, 6.0f);
    }

private:
    ReactorAudioProcessor& proc;
    float heat = 0.0f;
    bool seismic = false;
};

//==============================================================================
// CORE TEMP gauge + glowing tube array
class GaugeView : public juce::Component
{
public:
    GaugeView() { setInterceptsMouseClicks (false, false); }

    void update (float heatIn, float driveIn, float dt, bool reduceMotion, double now)
    {
        heat = heatIn; drive = driveIn; reduce = reduceMotion; time = now;

        float target = -130.0f + juce::jlimit (0.0f, 1.2f, heat) / 1.2f * 260.0f;
        if (heat > 0.8f && ! reduce)
            target += (rng.nextFloat() - 0.5f) * (heat - 0.8f) * 24.0f;

        const float k = 140.0f, c = 12.0f, h = dt * 0.5f;
        for (int s = 0; s < 2; ++s)
        {
            vel += (k * (target - angle) - c * vel) * h;
            angle += vel * h;
            if (angle > 135.0f)  { angle = 135.0f;  vel *= -0.4f; }
            if (angle < -135.0f) { angle = -135.0f; vel *= -0.4f; }
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const float cx = 180.0f, cy = 146.0f, R = 84.0f;
        const auto hc = heatColour (heat);

        float pulse = 1.0f;
        if (heat > 0.8f && ! reduce)
        {
            const float f = juce::jmin (3.0f, 1.5f + 1.5f * (heat - 0.8f) / 0.2f);
            pulse = 0.75f + 0.25f * std::sin (kTwoPi * f * (float) time);
        }

        // big glow behind everything
        {
            juce::ColourGradient glow (hc.withAlpha ((0.12f + 0.38f * juce::jmin (1.0f, heat)) * pulse), cx, cy,
                                       hc.withAlpha (0.0f), cx + 135.0f, cy, true);
            g.setGradientFill (glow);
            g.fillEllipse (cx - 135.0f, cy - 135.0f, 270.0f, 270.0f);
        }

        // tubes (glow pass, then glass pass)
        const auto fc = juce::Colour (0xffffa040).interpolatedWith (kRed, juce::jlimit (0.0f, 1.0f, heat));
        std::array<float, 5> bright {};
        for (int i = 0; i < 5; ++i)
        {
            float b = juce::jlimit (0.12f, 1.0f, drive * 1.4f - 0.18f * (float) i + 0.2f);
            if (heat > 0.9f && ! reduce) b *= 0.8f + 0.2f * rng.nextFloat();
            bright[(size_t) i] = b;
            const float tx = 180.0f + (float) (i - 2) * 68.0f;
            juce::ColourGradient tg (fc.withAlpha (b * 0.45f), tx, 34.0f, fc.withAlpha (0.0f), tx + 46.0f, 34.0f, true);
            g.setGradientFill (tg);
            g.fillEllipse (tx - 46.0f, -12.0f, 92.0f, 92.0f);
        }
        for (int i = 0; i < 5; ++i)
        {
            const float tx = 180.0f + (float) (i - 2) * 68.0f, b = bright[(size_t) i];
            const juce::Rectangle<float> glass (tx - 17.0f, 4.0f, 34.0f, 52.0f);
            g.setColour (juce::Colour (0xff0a0c0e).withAlpha (0.88f));
            g.fillRoundedRectangle (glass, 15.0f);
            g.setColour (fc.withAlpha (b * 0.35f));
            g.fillRoundedRectangle (glass.reduced (3.0f), 12.0f);
            g.setColour (fc.brighter (0.4f).withAlpha (0.35f + 0.65f * b));
            g.drawLine (tx - 4.0f, 18.0f, tx - 4.0f, 48.0f, 2.0f);
            g.drawLine (tx + 4.0f, 18.0f, tx + 4.0f, 48.0f, 2.0f);
            g.drawLine (tx - 4.0f, 18.0f, tx + 4.0f, 18.0f, 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillRoundedRectangle (tx - 13.0f, 8.0f, 5.0f, 38.0f, 2.5f);
            g.setColour (juce::Colour (0xff6b737b));
            g.drawRoundedRectangle (glass, 15.0f, 1.2f);
            g.setColour (juce::Colour (0xff2b2f34));
            g.fillRoundedRectangle (tx - 19.0f, 54.0f, 38.0f, 10.0f, 3.0f);
        }

        // bezel + face
        {
            juce::ColourGradient bz (juce::Colour (0xff8a9199), cx - R, cy - R, juce::Colour (0xff2a2e33), cx + R, cy + R, false);
            g.setGradientFill (bz);
            g.fillEllipse (cx - R - 7.0f, cy - R - 7.0f, 2.0f * (R + 7.0f), 2.0f * (R + 7.0f));
            juce::ColourGradient fg (juce::Colour (0xff1d2227), cx, cy, juce::Colour (0xff060708), cx + R, cy, true);
            g.setGradientFill (fg);
            g.fillEllipse (cx - R, cy - R, 2.0f * R, 2.0f * R);
        }

        auto ang = [] (float v) { return juce::degreesToRadians (-130.0f + v / 1.2f * 260.0f); };

        // zone arcs
        struct Zone { float a, b; juce::Colour c; };
        const Zone zones[3] = { { 0.0f, 0.5f, kBlue }, { 0.5f, 0.8f, kAmber }, { 0.8f, 1.2f, kRed } };
        for (const auto& z : zones)
        {
            juce::Path p;
            p.addCentredArc (cx, cy, R - 12.0f, R - 12.0f, 0.0f, ang (z.a), ang (z.b), true);
            g.setColour (z.c.withAlpha (heat >= z.a ? 0.95f : 0.45f));
            g.strokePath (p, juce::PathStrokeType (6.0f));
        }

        // ticks + numbers
        for (int k = 0; k <= 12; ++k)
        {
            const float a = ang ((float) k * 0.1f), s = std::sin (a), co = std::cos (a);
            const bool major = (k % 2 == 0);
            const float r0 = R - (major ? 27.0f : 23.0f), r1 = R - 19.0f;
            g.setColour (juce::Colours::white.withAlpha (major ? 0.85f : 0.45f));
            g.drawLine (cx + s * r0, cy - co * r0, cx + s * r1, cy - co * r1, major ? 2.0f : 1.0f);
            if (major)
            {
                g.setFont (mkFont (9.0f));
                g.setColour (juce::Colours::white.withAlpha (0.7f));
                g.drawText (juce::String (k * 10), (int) (cx + s * 44.0f) - 12, (int) (cy - co * 44.0f) - 6, 24, 12, juce::Justification::centred);
            }
        }

        g.setFont (mkFont (10.0f, true));
        g.setColour (kAmber.withAlpha (0.85f));
        g.drawText ("CORE TEMP", (int) cx - 40, (int) cy + 14, 80, 12, juce::Justification::centred);

        // LCD readout
        {
            const juce::Rectangle<float> lcd (cx - 28.0f, cy + 32.0f, 56.0f, 17.0f);
            g.setColour (juce::Colour (0xff050a06));
            g.fillRoundedRectangle (lcd, 3.0f);
            g.setColour (hc);
            g.setFont (mkMono (13.0f));
            g.drawText (juce::String ((int) (juce::jlimit (0.0f, 1.2f, heat) * 1000.0f)).paddedLeft ('0', 4) + " C",
                        lcd.toNearestInt(), juce::Justification::centred);
        }

        // needle
        {
            const float a = juce::degreesToRadians (angle), s = std::sin (a), co = std::cos (a);
            const float nx = cx + s * (R - 14.0f), ny = cy - co * (R - 14.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f));
            g.drawLine (cx + 2.0f, cy + 2.0f, nx + 2.0f, ny + 2.0f, 3.0f);
            g.setColour (hc.brighter (0.3f));
            g.drawLine (cx, cy, nx, ny, 2.5f);
            g.drawLine (cx, cy, cx - s * 14.0f, cy + co * 14.0f, 4.0f);
            g.setColour (juce::Colour (0xff7d858d));
            g.fillEllipse (cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);
            g.setColour (juce::Colour (0xff15171a));
            g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
        }

        // glass highlight
        juce::ColourGradient gl (juce::Colours::white.withAlpha (0.12f), cx, cy - R, juce::Colours::white.withAlpha (0.0f), cx, cy, false);
        g.setGradientFill (gl);
        g.fillEllipse (cx - R + 6.0f, cy - R + 4.0f, 2.0f * R - 12.0f, R);
    }

private:
    float heat = 0.0f, drive = 0.0f, angle = -130.0f, vel = 0.0f;
    bool reduce = false;
    double time = 0.0;
    juce::Random rng;
};

//==============================================================================
// Heavy metal lever: drag down to insert a rod
class RodLever : public juce::Component
{
public:
    explicit RodLever (juce::RangedAudioParameter& p)
        : attach (p, [this] (float v) { value = v; repaint(); }, nullptr)
    {
        attach.sendInitialUpdate();
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        const float cx = r.getCentreX(), top = 22.0f, bottom = r.getHeight() - 22.0f;
        const float y = top + value * (bottom - top);

        // slot
        const juce::Rectangle<float> slot (cx - 5.0f, top - 6.0f, 10.0f, bottom - top + 12.0f);
        g.setColour (juce::Colour (0xff050607));
        g.fillRoundedRectangle (slot, 4.0f);
        // glowing rod shaft (depth indicator)
        const auto gc = kCyan.interpolatedWith (kAmber, value);
        g.setColour (gc.withAlpha (0.15f + 0.65f * value));
        g.fillRoundedRectangle (cx - 3.0f, top - 4.0f, 6.0f, juce::jmax (2.0f, y - top + 4.0f), 2.0f);

        // scale
        g.setColour (juce::Colours::white.withAlpha (0.5f));
        for (int i = 0; i <= 4; ++i)
        {
            const float ty = top + (bottom - top) * (float) i / 4.0f;
            g.drawLine (cx - 22.0f, ty, cx - 12.0f, ty, i % 2 == 0 ? 1.5f : 1.0f);
            g.drawLine (cx + 12.0f, ty, cx + 22.0f, ty, i % 2 == 0 ? 1.5f : 1.0f);
        }
        g.setFont (mkFont (8.0f, true));
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.drawText ("OUT", (int) cx - 20, 4, 40, 10, juce::Justification::centred);
        g.drawText ("IN", (int) cx - 20, (int) r.getHeight() - 13, 40, 10, juce::Justification::centred);

        // handle
        const juce::Rectangle<float> hnd (cx - 22.0f, y - 14.0f, 44.0f, 28.0f);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (hnd.translated (2.0f, 3.0f), 5.0f);
        juce::ColourGradient hg (juce::Colour (0xffb7bec6), hnd.getX(), hnd.getY(), juce::Colour (0xff3a3f45), hnd.getX(), hnd.getBottom(), false);
        g.setGradientFill (hg);
        g.fillRoundedRectangle (hnd, 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        for (int i = 0; i < 3; ++i)
            g.drawLine (hnd.getX() + 6.0f, hnd.getY() + 7.0f + 5.0f * (float) i, hnd.getRight() - 6.0f, hnd.getY() + 7.0f + 5.0f * (float) i, 1.2f);
        g.setColour (kRed.withAlpha (0.9f));
        g.fillRoundedRectangle (hnd.getX() + 4.0f, hnd.getBottom() - 6.0f, hnd.getWidth() - 8.0f, 3.0f, 1.5f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawRoundedRectangle (hnd, 5.0f, 1.3f);
    }

    void mouseDown (const juce::MouseEvent& e) override { attach.beginGesture(); drag (e); }
    void mouseDrag (const juce::MouseEvent& e) override { drag (e); }
    void mouseUp (const juce::MouseEvent&) override { attach.endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { attach.setValueAsCompleteGesture (0.0f); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        attach.setValueAsCompleteGesture (juce::jlimit (0.0f, 1.0f, value - w.deltaY * 0.1f));
    }

private:
    void drag (const juce::MouseEvent& e)
    {
        const float top = 22.0f, bottom = (float) getHeight() - 22.0f;
        float v = juce::jlimit (0.0f, 1.0f, ((float) e.y - top) / (bottom - top));
        for (int k = 0; k <= 4; ++k)                      // soft detents
            if (std::abs (v - 0.25f * (float) k) < 0.025f) v = 0.25f * (float) k;
        attach.setValueAsPartOfGesture (v);
    }

    juce::ParameterAttachment attach;
    float value = 0.0f;
};

//==============================================================================
class LampBank : public juce::Component
{
public:
    LampBank() { setInterceptsMouseClicks (false, false); }

    void update (int s, double t, bool reduce) { st = s; time = t; reduceMotion = reduce; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const char* names[4] = { "OK", "WARN", "CRIT", "SCRAM" };
        const juce::Colour cols[4] = { kGreen, kAmber, kRed, kCyan };
        const bool blink = reduceMotion || std::fmod (time * 2.0, 1.0) < 0.5;
        bool on[4] = { false, false, false, false };

        switch (st)
        {
            case 0: on[0] = true; break;
            case 1: on[1] = true; break;
            case 2: on[1] = true; on[2] = blink; break;
            case 3: on[3] = true; break;
            case 4: on[1] = true; on[0] = blink; break;
            default: break;
        }

        const float w = (float) getWidth() / 4.0f;
        for (int i = 0; i < 4; ++i)
        {
            const float cx = w * ((float) i + 0.5f);
            drawLamp (g, cx, 11.0f, 6.0f, cols[i], on[i]);
            g.setFont (mkFont (9.0f, true));
            g.setColour (juce::Colours::white.withAlpha (0.7f));
            g.drawText (names[i], (int) cx - 30, 21, 60, 11, juce::Justification::centred);
        }
    }

private:
    int st = 0;
    double time = 0.0;
    bool reduceMotion = false;
};

//==============================================================================
class LedMeter : public juce::Component
{
public:
    LedMeter() { setInterceptsMouseClicks (false, false); }

    void push (float peak)
    {
        level = juce::jmax (peak, level * 0.88f);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        const float norm = juce::jlimit (0.0f, 1.0f, juce::jmap (db, -60.0f, 0.0f, 0.0f, 1.0f));
        constexpr int segs = 14;
        const float h = (float) getHeight() / (float) segs;
        for (int i = 0; i < segs; ++i)
        {
            const float f = (float) i / (float) segs;
            const bool lit = norm > f;
            const juce::Colour c = f < 0.65f ? kGreen : (f < 0.88f ? kAmber : kRed);
            g.setColour (lit ? c : c.darker (0.85f));
            g.fillRect ((float) 0, (float) getHeight() - h * (float) (i + 1) + 1.0f, (float) getWidth(), h - 1.5f);
        }
    }

private:
    float level = 0.0f;
};

//==============================================================================
// Geiger readout (clicks) + noise / crush LEDs, drawn inside the leak panel
class LeakDisplay : public juce::Component
{
public:
    LeakDisplay() { setInterceptsMouseClicks (false, false); }

    void update (uint32_t clickDelta, float dt, float leakEff)
    {
        if (clickDelta > 0) flash = 1.0f;
        flash = juce::jmax (0.0f, flash - dt * 12.0f);
        cps = cps * 0.9f + 0.1f * ((float) clickDelta / juce::jmax (0.001f, dt));
        leak = leakEff;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const juce::Rectangle<float> box (20.0f, 30.0f, 236.0f, 36.0f);
        g.setColour (juce::Colour (0xff050a06));
        g.fillRoundedRectangle (box, 4.0f);
        drawLamp (g, 38.0f, 48.0f, 6.0f, juce::Colour (0xffe8c010), flash > 0.05f);
        g.setFont (mkMono (14.0f));
        g.setColour (kAmber);
        g.drawText ("GEIGER  " + juce::String ((int) cps).paddedLeft (' ', 4) + " CPS", 54, 30, 200, 36, juce::Justification::centredLeft);

        drawLeds (g, 278.0f, "NOISE", leak);
        drawLeds (g, 300.0f, "CRUSH", leak * leak * 1.4f);
    }

private:
    static void drawLeds (juce::Graphics& g, float y, const juce::String& name, float v)
    {
        g.setFont (mkFont (9.0f, true));
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawText (name, 20, (int) y, 50, 12, juce::Justification::centredLeft);
        for (int i = 0; i < 8; ++i)
        {
            const bool lit = v * 8.0f > (float) i + 0.2f;
            const juce::Colour c = i < 5 ? kGreen : (i < 7 ? kAmber : kRed);
            g.setColour (lit ? c : c.darker (0.85f));
            g.fillRect (74.0f + 21.0f * (float) i, y, 18.0f, 12.0f);
        }
    }

    float flash = 0.0f, cps = 0.0f, leak = 0.0f;
};

//==============================================================================
class ScramButton : public juce::Button
{
public:
    ScramButton() : juce::Button ("scram") {}
    void setPulse (float p) { if (std::abs (p - pulse) > 0.001f) { pulse = p; repaint(); } }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced (4.0f);
        const float d = juce::jmin (r.getWidth(), r.getHeight());
        const auto c = r.getCentre();

        // hazard ring
        g.setColour (juce::Colour (0xffd9a800));
        g.fillEllipse (c.x - d * 0.5f, c.y - d * 0.5f, d, d);
        g.setColour (juce::Colour (0xff111111));
        for (int i = 0; i < 12; i += 2)
        {
            juce::Path p;
            p.addPieSegment (c.x - d * 0.5f, c.y - d * 0.5f, d, d, (float) i * kPi / 6.0f, (float) (i + 1) * kPi / 6.0f, 0.0f);
            g.fillPath (p);
        }

        if (pulse > 0.0f)
        {
            juce::ColourGradient glow (kRed.withAlpha (0.65f * pulse), c.x, c.y, kRed.withAlpha (0.0f), c.x + d * 0.9f, c.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (c.x - d * 0.9f, c.y - d * 0.9f, d * 1.8f, d * 1.8f);
        }

        const float br = d * 0.38f, off = down ? 2.0f : 0.0f;
        juce::ColourGradient body (down ? juce::Colour (0xffc02218) : juce::Colour (0xffff5a46), c.x - br * 0.4f, c.y - br * 0.5f + off,
                                   juce::Colour (0xff7a0a0a), c.x + br * 0.6f, c.y + br * 0.8f + off, false);
        g.setGradientFill (body);
        g.fillEllipse (c.x - br, c.y - br + off, 2.0f * br, 2.0f * br);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (c.x - br, c.y - br + off, 2.0f * br, 2.0f * br, 1.5f);
        if (over)
        {
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.fillEllipse (c.x - br, c.y - br + off, 2.0f * br, 2.0f * br);
        }
        g.setColour (juce::Colours::white);
        g.setFont (mkFont (13.0f, true));
        g.drawText ("SCRAM", (int) (c.x - br), (int) (c.y - 8.0f + off), (int) (2.0f * br), 16, juce::Justification::centred);
    }

private:
    float pulse = 0.0f;
};

//==============================================================================
// Lockdown overlay: vignette + header banner
class OverlayView : public juce::Component
{
public:
    OverlayView() { setInterceptsMouseClicks (false, false); }

    void update (int s, double t, bool r)
    {
        const bool needs = (s != 0) || (st != 0);
        st = s; time = t; reduce = r;
        if (needs) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (st == 0) return;
        const float W = (float) getWidth(), H = (float) getHeight();

        if (st == 2)
        {
            const float pulse = reduce ? 0.6f : 0.5f + 0.5f * std::sin (kTwoPi * 2.0f * (float) time);
            juce::ColourGradient vg (kRed.withAlpha (0.0f), W * 0.5f, H * 0.5f, kRed.withAlpha (0.22f + 0.28f * pulse), W * 0.5f + 620.0f, H * 0.5f, true);
            vg.addColour (0.55, kRed.withAlpha (0.0f));
            g.setGradientFill (vg);
            g.fillRect (getLocalBounds());
        }
        else if (st == 3)
        {
            juce::ColourGradient vg (kCyan.withAlpha (0.0f), W * 0.5f, H * 0.5f, kCyan.withAlpha (0.16f), W * 0.5f + 620.0f, H * 0.5f, true);
            vg.addColour (0.6, kCyan.withAlpha (0.0f));
            g.setGradientFill (vg);
            g.fillRect (getLocalBounds());
        }

        juce::String txt; juce::Colour c = kAmber;
        switch (st)
        {
            case 1: txt = "WARNING - CORE HEATING"; break;
            case 2: txt = "!! CORE CRITICAL !!"; c = kRed; break;
            case 3: txt = "SCRAM - SHUTDOWN"; c = kCyan; break;
            case 4: txt = "COOLING..."; break;
            default: break;
        }
        const bool blink = reduce || std::fmod (time * 2.0, 1.0) < 0.6;
        if (st != 2 || blink)
        {
            g.setFont (mkFont (20.0f, true));
            g.setColour (c);
            g.drawText (txt, 440, 8, 250, 32, juce::Justification::centred);
        }
    }

private:
    int st = 0;
    double time = 0.0;
    bool reduce = false;
};

//==============================================================================
class ReactorContent : public juce::Component, private juce::Timer
{
public:
    explicit ReactorContent (ReactorAudioProcessor& p)
        : proc (p), scopeView (p)
    {
        setSize (1000, 640);
        auto& ap = proc.apvts;

        addAndMakeVisible (background);
        addAndMakeVisible (scopeView);
        addAndMakeVisible (gauge);
        addAndMakeVisible (lamps);
        addAndMakeVisible (leakDisplay);
        addAndMakeVisible (meterIn);
        addAndMakeVisible (meterOut);

        for (int i = 0; i < 4; ++i)
        {
            levers[(size_t) i] = std::make_unique<RodLever> (*ap.getParameter ("rod" + juce::String (i + 1) + "_pos"));
            addAndMakeVisible (*levers[(size_t) i]);
        }

        initKnob (driveK, "drive");  initKnob (biasK, "bias");  initKnob (sagK, "sag");
        initKnob (leakK, "leak");    initKnob (inK, "in_gain"); initKnob (outK, "out_gain");
        initKnob (mixK, "mix");      initKnob (thrK, "meltdown_thr");

        aDrive = std::make_unique<SA> (ap, "drive", driveK);
        aBias  = std::make_unique<SA> (ap, "bias", biasK);
        aSag   = std::make_unique<SA> (ap, "sag", sagK);
        aLeak  = std::make_unique<SA> (ap, "leak", leakK);
        aIn    = std::make_unique<SA> (ap, "in_gain", inK);
        aOut   = std::make_unique<SA> (ap, "out_gain", outK);
        aMix   = std::make_unique<SA> (ap, "mix", mixK);
        aThr   = std::make_unique<SA> (ap, "meltdown_thr", thrK);

        initToggle (routeBtn, "ROUTE: POST-TUBE");
        initToggle (dynBtn,   "LEAK: DYNAMIC");
        initToggle (meltBtn,  "MELTDOWN SEQ");
        initToggle (reduceBtn, "REDUCE MOTION");
        aRoute  = std::make_unique<BA> (ap, "rod_pre", routeBtn);
        aDyn    = std::make_unique<BA> (ap, "leak_dyn", dynBtn);
        aMelt   = std::make_unique<BA> (ap, "meltdown_en", meltBtn);
        aReduce = std::make_unique<BA> (ap, "reduce_motion", reduceBtn);

        addAndMakeVisible (scram);
        scram.onClick = [this] { proc.triggerScram(); };

        addAndMakeVisible (overlay);

        layout();
        lastTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
        startTimerHz (60);
    }

    ~ReactorContent() override
    {
        stopTimer();
        for (auto* s : { &driveK, &biasK, &sagK, &leakK, &inK, &outK, &mixK, &thrK })
            s->setLookAndFeel (nullptr);
    }

    void setScale (float s) { scale = s; applyTransform(); }

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void applyTransform()
    {
        setTransform (juce::AffineTransform::scale (scale).translated (offX * scale, offY * scale));
    }

    void initKnob (juce::Slider& s, const juce::String& name)
    {
        s.setName (name);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (1.2f * kPi, 2.8f * kPi, true);
        s.setMouseDragSensitivity (220);
        s.setLookAndFeel (&lnf);
        addAndMakeVisible (s);
    }

    void initToggle (juce::TextButton& b, const juce::String& text)
    {
        b.setButtonText (text);
        b.setClickingTogglesState (true);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1a1d21));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff8a5a00));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.65f));
        b.setColour (juce::TextButton::textColourOnId, kAmber.brighter (0.3f));
        addAndMakeVisible (b);
    }

    void layout()
    {
        background.setBounds (0, 0, 1000, 640);
        scopeView.setBounds (44, 66, 912, 126);
        gauge.setBounds (320, 236, 360, 232);
        lamps.setBounds (700, 8, 276, 34);
        leakDisplay.setBounds (700, 208, 276, 330);

        for (int i = 0; i < 4; ++i)
        {
            const int cx = (int) (24.0f + 69.0f * (float) i + 34.5f);
            levers[(size_t) i]->setBounds (cx - 24, 240, 48, 230);
        }
        routeBtn.setBounds (50, 506, 200, 22);

        driveK.setBounds (374, 472, 52, 52);
        biasK.setBounds  (474, 472, 52, 52);
        sagK.setBounds   (574, 472, 52, 52);

        leakK.setBounds (763, 285, 150, 150);
        dynBtn.setBounds (738, 452, 200, 24);

        inK.setBounds (54, 560, 52, 52);
        meterIn.setBounds (118, 560, 10, 56);
        mixK.setBounds (204, 560, 52, 52);
        thrK.setBounds (314, 560, 52, 52);
        scram.setBounds (452, 550, 96, 80);
        meltBtn.setBounds (580, 558, 140, 22);
        reduceBtn.setBounds (580, 588, 140, 22);
        meterOut.setBounds (836, 560, 10, 56);
        outK.setBounds (854, 560, 52, 52);

        overlay.setBounds (0, 0, 1000, 640);
    }

    void timerCallback() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const float dt = juce::jlimit (0.001f, 0.1f, (float) (now - lastTime));
        lastTime = now;

        const float heat = proc.heat.load();
        const int st = proc.state.load();
        const bool reduce = proc.apvts.getRawParameterValue ("reduce_motion")->load() > 0.5f;

        if (st == 2 && lastState != 2) shakeT = 0.35f;
        lastState = st;

        scopeView.update (heat);
        gauge.update (heat, proc.driveEffUI.load(), dt, reduce, now);
        lamps.update (st, now, reduce);
        meterIn.push (proc.peakIn.exchange (0.0f));
        meterOut.push (proc.peakOut.exchange (0.0f));

        const uint32_t clicks = proc.clickCount.load();
        leakDisplay.update (clicks - lastClicks, dt, proc.leakEffUI.load());
        lastClicks = clicks;

        scram.setPulse (st == 2 ? (reduce ? 0.7f : 0.5f + 0.5f * std::sin (kTwoPi * 2.0f * (float) now)) : 0.0f);
        overlay.update (st, now, reduce);

        routeBtn.setButtonText (routeBtn.getToggleState() ? "ROUTE: PRE-TUBE" : "ROUTE: POST-TUBE");
        dynBtn.setButtonText (dynBtn.getToggleState() ? "LEAK: DYNAMIC" : "LEAK: STATIC");

        if (shakeT > 0.0f && ! reduce)
        {
            shakeT -= dt;
            const float a = 4.0f * juce::jmax (0.0f, shakeT) / 0.35f;
            auto& r = juce::Random::getSystemRandom();
            offX = (r.nextFloat() - 0.5f) * 2.0f * a;
            offY = (r.nextFloat() - 0.5f) * 2.0f * a;
            applyTransform();
        }
        else if (offX != 0.0f || offY != 0.0f)
        {
            offX = offY = 0.0f;
            applyTransform();
        }
    }

    ReactorAudioProcessor& proc;
    ReactorLNF lnf;   // declared early so it outlives the sliders

    Background background;
    ScopeView scopeView;
    GaugeView gauge;
    LampBank lamps;
    LeakDisplay leakDisplay;
    LedMeter meterIn, meterOut;
    std::array<std::unique_ptr<RodLever>, 4> levers;
    juce::Slider driveK, biasK, sagK, leakK, inK, outK, mixK, thrK;
    juce::TextButton routeBtn, dynBtn, meltBtn, reduceBtn;
    ScramButton scram;
    OverlayView overlay;

    std::unique_ptr<SA> aDrive, aBias, aSag, aLeak, aIn, aOut, aMix, aThr;
    std::unique_ptr<BA> aRoute, aDyn, aMelt, aReduce;

    float scale = 1.0f, shakeT = 0.0f, offX = 0.0f, offY = 0.0f;
    int lastState = 0;
    uint32_t lastClicks = 0;
    double lastTime = 0.0;
};

//==============================================================================
ReactorAudioProcessorEditor::ReactorAudioProcessorEditor (ReactorAudioProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    content = std::make_unique<ReactorContent> (proc);
    addAndMakeVisible (*content);

    setResizable (true, true);
    setResizeLimits (500, 320, 2000, 1280);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (1000.0 / 640.0);
    setSize (1000, 640);
}

ReactorAudioProcessorEditor::~ReactorAudioProcessorEditor() = default;

void ReactorAudioProcessorEditor::resized()
{
    if (content != nullptr)
        content->setScale ((float) getWidth() / 1000.0f);
}
