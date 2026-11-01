// The Home page of the MODX M Controller window, ported natively from docs/mockups/MODX2_Home.dc.html (mockup is the plugin).
// All boxes, texts, icons, knobs and bars come from the generated HomeLayout.h (tools/gen_home_layout.py), so the window
// matches the mockup. Drawn in the mockup's own pixel space and scaled to the window. No web view, no data leaves the plugin.
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <map>
#include "HomeLayout.h"
#include "catui/CatLookAndFeel.h"

namespace modx
{
// What the page needs from the plugin (or from the render test). Parameter ids are the ids in map/*.csv.
struct HomeModel
{
    virtual ~HomeModel() = default;
    virtual bool has(const juce::String& id) const = 0;
    virtual float getNorm(const juce::String& id) const = 0;                       // 0..1
    virtual float defaultNorm(const juce::String& id) const = 0;
    virtual juce::String valueText(const juce::String& id, float norm) const = 0;  // shown under a knob
    virtual juce::String describe(const juce::String& id) const = 0;               // hover bubble text
    virtual void beginEdit(const juce::String& id) = 0;
    virtual void setNorm(const juce::String& id, float norm) = 0;
    virtual void endEdit(const juce::String& id) = 0;
};

class HomePage : public juce::Component, private juce::Timer
{
public:
    static constexpr int kTotalW = home::W + 2 * home::kCheek, kTotalH = home::H;

    explicit HomePage(HomeModel& m) : model(m)
    {
        for (const auto& s : home::svgs)
        {
            std::unique_ptr<juce::Drawable> d;
            if (auto xml = juce::XmlDocument::parse(s.svg)) d = juce::Drawable::createFromSVG(*xml);
            drawables.push_back(std::move(d));
        }
        for (size_t i = 0; i < std::size(home::knobs); ++i) lastKnob.push_back(-1.0f);
        for (size_t i = 0; i < std::size(home::bars); ++i) lastBar.push_back(-1.0f);
        setMouseCursor(juce::MouseCursor::NormalCursor);
        setOpaque(true);
        startTimerHz(20);   // picks up changes from host automation and from the device
    }

    // Fonts: Roboto Condensed (SIL Open Font License), embedded in the plugin, so it looks the same on every machine.
    static void setFonts(juce::Typeface::Ptr regular, juce::Typeface::Ptr medium, juce::Typeface::Ptr bold) { fonts()[0] = regular; fonts()[1] = medium; fonts()[2] = bold; }

    std::function<void()> onReset100, onInfo, onGear, onWoodDoubleClick;
    bool bubblesOn = true;

    void paint(juce::Graphics& g) override
    {
        const float s = float(getWidth()) / float(kTotalW);
        g.fillAll(juce::Colour(0xff0b0b0c));
        g.addTransform(juce::AffineTransform::scale(s));
        catui::drawWoodCheeks(g, { 0, 0, kTotalW, kTotalH }, home::kCheek);
        g.saveState();
        g.addTransform(juce::AffineTransform::translation(float(home::kCheek), 0.0f));
        g.reduceClipRegion(0, 0, home::W, home::H);
        for (const auto& op : home::ops)
        {
            switch (op.kind)
            {
                case 0: paintRect(g, op.idx); break;
                case 1: paintText(g, op.idx); break;
                case 2: paintSvg(g, op.idx); break;
                case 3: paintKnob(g, op.idx); break;
                case 4: paintBar(g, op.idx); break;
                default: break;
            }
        }
        g.restoreState();
        if (bubblesOn && bubble.isNotEmpty()) paintBubble(g);
    }

    void resized() override { repaint(); }

    //==============================================================================
    void mouseMove(const juce::MouseEvent& e) override { updateBubble(e); }
    void mouseExit(const juce::MouseEvent&) override { bubble = {}; repaint(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        bubble = {}; drag = {};
        const auto p = toVirtual(e.position);
        if (auto h = hitTest(p))
        {
            if (h->type == Knob || h->type == Bar)
            {
                const auto id = h->id; if (id.isEmpty() || !model.has(id)) return;
                drag = *h; dragStartNorm = model.getNorm(id); model.beginEdit(id); dragging = true;
            }
            else if (h->type == Toggle)
            {
                const auto id = h->id; if (id.isNotEmpty() && model.has(id)) { model.beginEdit(id); model.setNorm(id, model.getNorm(id) > 0.5f ? 0.0f : 1.0f); model.endEdit(id); repaint(); }
            }
            else if (h->type == Scene)
            {
                const auto id = juce::String("s_0600022a");
                if (model.has(id)) { model.beginEdit(id); model.setNorm(id, float(h->index) / 7.0f); model.endEdit(id); repaint(); }
            }
            else if (h->type == Reset) { if (onReset100) onReset100(); }
            else if (h->type == Info) { if (onInfo) onInfo(); }
            else if (h->type == Gear) { if (onGear) onGear(); }
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (!dragging) return;
        float n;
        if (drag.type == Bar)
        {
            const auto& b = home::bars[size_t(drag.index)];
            n = juce::jlimit(0.0f, 1.0f, (toVirtual(e.position).x - b.x) / b.w);
        }
        else
        {
            const float range = e.mods.isShiftDown() ? 800.0f : 160.0f;   // pixels for the full range; Shift = fine
            n = juce::jlimit(0.0f, 1.0f, dragStartNorm - float(e.getDistanceFromDragStartY()) / range);
        }
        model.setNorm(drag.id, n); repaint();
    }

    void mouseUp(const juce::MouseEvent&) override { if (dragging) { model.endEdit(drag.id); dragging = false; } }

    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        const auto p = toVirtual(e.position);
        if (auto h = hitTest(p))
        {
            if ((h->type == Knob || h->type == Bar) && h->id.isNotEmpty() && model.has(h->id))
            { model.beginEdit(h->id); model.setNorm(h->id, model.defaultNorm(h->id)); model.endEdit(h->id); repaint(); }
            return;
        }
        // double click on the wood cheeks or the header background: back to 100 %
        if (e.position.x < home::kCheek * scaleNow() || e.position.x > float(getWidth()) - home::kCheek * scaleNow() || p.y < 46.0f)
            if (onWoodDoubleClick) onWoodDoubleClick();
    }

    void mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (auto h = hitTest(toVirtual(e.position)))
            if ((h->type == Knob || h->type == Bar) && h->id.isNotEmpty() && model.has(h->id))
            {
                const float step = (e.mods.isShiftDown() ? 0.004f : 0.02f) * (w.deltaY > 0 ? 1.0f : -1.0f);
                model.beginEdit(h->id); model.setNorm(h->id, juce::jlimit(0.0f, 1.0f, model.getNorm(h->id) + step)); model.endEdit(h->id); repaint();
            }
    }

private:
    enum Type { Knob, Bar, Toggle, Scene, Reset, Info, Gear };
    struct Hit { Type type; int index; juce::Rectangle<float> r; juce::String id, tip; };

    HomeModel& model;
    std::vector<std::unique_ptr<juce::Drawable>> drawables;
    std::vector<float> lastKnob, lastBar;
    juce::String bubble; juce::Point<float> bubblePos;
    Hit drag; float dragStartNorm = 0.0f; bool dragging = false;

    static std::array<juce::Typeface::Ptr, 3>& fonts() { static std::array<juce::Typeface::Ptr, 3> f; return f; }
    float scaleNow() const { return float(getWidth()) / float(kTotalW); }
    juce::Point<float> toVirtual(juce::Point<float> p) const { return { p.x / scaleNow() - float(home::kCheek), p.y / scaleNow() }; }

    static juce::Font makeFont(float size, int weight, float ls)
    {
        const int i = weight >= 700 ? 2 : weight >= 500 ? 1 : 0;
        juce::FontOptions o = fonts()[size_t(i)] != nullptr ? juce::FontOptions(fonts()[size_t(i)]) : juce::FontOptions();
        o = o.withHeight(size * 1.172f);   // Roboto: ascent + descent is 1.172 of the em size the mockup uses
        if (ls != 0.0f) o = o.withKerningFactor(ls / size);
        return juce::Font(o);
    }

    //==============================================================================
    std::optional<Hit> hitTest(juce::Point<float> p) const
    {
        for (const auto& h : hits()) if (h.r.contains(p)) return h;
        return std::nullopt;
    }

    const std::vector<Hit>& hits() const
    {
        if (!hitList.empty()) return hitList;
        auto& v = hitList;
        v.push_back({ Reset, 0, { home::reset100X, home::reset100Y, home::reset100W, home::reset100H }, {}, "Back to 100 % size. Double click the wood to do the same." });
        v.push_back({ Info, 0, { 1458, 11, 24, 24 }, {}, "About this plugin." });
        v.push_back({ Gear, 0, { 1486, 11, 24, 24 }, {}, "Settings: MIDI channel, device number, part, send all, info bubbles." });
        for (size_t i = 0; i < std::size(home::knobs); ++i)
        {
            const auto& k = home::knobs[i]; auto r = juce::Rectangle<float>(k.x, k.y, k.w, k.h).expanded(k.big ? 2.0f : 6.0f, k.big ? 2.0f : 4.0f);
            v.push_back({ Knob, int(i), r, k.paramId ? juce::String(k.paramId) : juce::String(), k.tip });
        }
        for (size_t i = 0; i < std::size(home::bars); ++i)
        {
            const auto& b = home::bars[i]; v.push_back({ Bar, int(i), juce::Rectangle<float>(b.x, b.y, b.w, b.h).expanded(0.0f, 5.0f), b.paramId ? juce::String(b.paramId) : juce::String(), b.tip });
        }
        for (size_t i = 0; i < std::size(home::toggles); ++i)
        {
            const auto& t = home::toggles[i]; const auto& r = home::rects[t.rect]; v.push_back({ Toggle, int(i), { r.x, r.y, r.w, r.h }, t.paramId, t.tip });
        }
        for (int i = 0; i < 8; ++i)
        {
            const auto& r = home::rects[home::sceneRect[i]]; v.push_back({ Scene, i, { r.x, r.y, r.w, r.h }, "s_0600022a", "Scene " + juce::String(i + 1) + ": Scene Select" });
        }
        return v;
    }
    mutable std::vector<Hit> hitList;

    void updateBubble(const juce::MouseEvent& e)
    {
        juce::String t; const auto p = toVirtual(e.position);
        if (auto h = hitTest(p))
        {
            t = h->tip;
            if (h->id.isNotEmpty() && model.has(h->id)) t = h->tip.upToFirstOccurrenceOf(":", false, false) + "\n" + model.describe(h->id);
        }
        if (t != bubble || t.isNotEmpty()) { bubble = t; bubblePos = e.position; repaint(); }
    }

    void paintBubble(juce::Graphics& g)
    {
        juce::Font f = makeFont(12.0f, 400, 0.0f);
        g.setFont(f);
        juce::AttributedString as; as.append(bubble, f, juce::Colour(0xffe4e6ea)); as.setWordWrap(juce::AttributedString::byWord);
        juce::TextLayout tl; tl.createLayout(as, 280.0f);
        const float w = tl.getWidth() + 16.0f, h = tl.getHeight() + 12.0f;
        const float k = scaleNow();
        float x = bubblePos.x / k + 14.0f, y = bubblePos.y / k + 18.0f;   // virtual units (g still has the window scale applied)
        const float vw = float(getWidth()) / k, vh = float(getHeight()) / k;
        if (x + w > vw - 4.0f) x = vw - w - 4.0f;
        if (y + h > vh - 4.0f) y = bubblePos.y / k - h - 10.0f;
        g.setColour(juce::Colour(0xf0121316)); g.fillRoundedRectangle(x, y, w, h, 5.0f);
        g.setColour(juce::Colour(0xff2ecbd5)); g.drawRoundedRectangle(x + 0.5f, y + 0.5f, w - 1.0f, h - 1.0f, 5.0f, 1.0f);
        tl.draw(g, { x + 8.0f, y + 6.0f, w, h });
    }

    //==============================================================================
    void paintRect(juce::Graphics& g, int i) const
    {
        const auto& r = home::rects[i]; juce::Colour bg(r.bg);
        for (size_t t = 0; t < std::size(home::toggles); ++t) if (home::toggles[t].rect == i) bg = juce::Colour(toggleOn(t) ? home::toggles[t].onBg : home::toggles[t].offBg);
        for (int s = 0; s < 8; ++s) if (home::sceneRect[s] == i) bg = juce::Colour(sceneSel() == s ? home::sceneSelBg : home::sceneOffBg);
        const juce::Rectangle<float> rc(r.x, r.y, r.w, r.h);
        if (bg.getAlpha() > 0) { g.setColour(bg); if (r.rad > 0.0f) g.fillRoundedRectangle(rc, r.rad); else g.fillRect(rc); }
        const bool uniform = r.bw[0] == r.bw[1] && r.bw[1] == r.bw[2] && r.bw[2] == r.bw[3];
        g.setColour(juce::Colour(r.bc));
        if (uniform && r.bw[0] > 0.0f) { if (r.rad > 0.0f) g.drawRoundedRectangle(rc.reduced(r.bw[0] * 0.5f), r.rad, r.bw[0]); else { g.drawRect(rc, r.bw[0]); } }
        else
        {
            if (r.bw[0] > 0) g.fillRect(rc.getX(), rc.getY(), rc.getWidth(), r.bw[0]);
            if (r.bw[1] > 0) g.fillRect(rc.getRight() - r.bw[1], rc.getY(), r.bw[1], rc.getHeight());
            if (r.bw[2] > 0) g.fillRect(rc.getX(), rc.getBottom() - r.bw[2], rc.getWidth(), r.bw[2]);
            if (r.bw[3] > 0) g.fillRect(rc.getX(), rc.getY(), r.bw[3], rc.getHeight());
        }
    }

    void paintText(juce::Graphics& g, int i) const
    {
        const auto& t = home::texts[i]; juce::Colour c(t.c);
        for (size_t k = 0; k < std::size(home::toggles); ++k) if (home::toggles[k].text == i) c = juce::Colour(toggleOn(k) ? home::toggles[k].onTx : home::toggles[k].offTx);
        for (int s = 0; s < 8; ++s) if (home::sceneText[s] == i) c = juce::Colour(sceneSel() == s ? home::sceneSelTx : home::sceneOffTx);
        g.setColour(c); g.setFont(makeFont(t.fs, t.fw, t.ls));
        g.drawText(juce::String::fromUTF8(t.s), juce::Rectangle<float>(t.x, t.y, t.w + 6.0f, t.h), juce::Justification::centredLeft, false);
    }

    void paintSvg(juce::Graphics& g, int i) const
    {
        if (auto* d = drawables[size_t(i)].get()) d->drawWithin(g, { home::svgs[i].x, home::svgs[i].y, home::svgs[i].w, home::svgs[i].h }, juce::RectanglePlacement::stretchToFit, 1.0f);
    }

    float knobNorm(const home::Knob& k) const { return k.paramId && model.has(k.paramId) ? model.getNorm(k.paramId) : k.frac; }

    void paintKnob(juce::Graphics& g, int i) const
    {
        const auto& k = home::knobs[i]; const float n = juce::jlimit(0.0f, 1.0f, knobNorm(k));
        const float cx = k.x + k.w * 0.5f, cy = k.y + k.h * 0.5f, r = k.r, sw = k.sw;
        g.setColour(juce::Colour(k.body)); g.fillEllipse(cx - r, cy - r, 2 * r, 2 * r);
        g.setColour(juce::Colour(k.track)); g.drawEllipse(cx - r, cy - r, 2 * r, 2 * r, sw);
        const float a0 = juce::degreesToRadians(-135.0f), a1 = a0 + juce::degreesToRadians(270.0f) * n;
        if (n > 0.001f)
        {
            juce::Path arc; arc.addCentredArc(cx, cy, r, r, 0.0f, a0, a1, true);
            g.setColour(juce::Colour(k.arc)); g.strokePath(arc, juce::PathStrokeType(sw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        const float hub = r * 0.375f;
        g.setColour(juce::Colour(k.hub)); g.fillEllipse(cx - hub, cy - hub, 2 * hub, 2 * hub);
        g.setColour(juce::Colour(k.needle));
        g.drawLine(cx, cy, cx + r * std::sin(a1), cy - r * std::cos(a1), k.big ? 2.4f : 1.6f);
        if (k.vw > 0.0f)
        {
            juce::String txt = k.val;
            if (k.paramId && model.has(k.paramId)) txt = model.valueText(k.paramId, n);
            g.setColour(juce::Colour(k.valCol)); g.setFont(makeFont(k.valFs, 400, 0.0f));
            const float cxv = k.vx + k.vw * 0.5f; g.drawText(txt, juce::Rectangle<float>(cxv - 20.0f, k.vy, 40.0f, k.vh), juce::Justification::centred, false);
        }
    }

    void paintBar(juce::Graphics& g, int i) const
    {
        const auto& b = home::bars[i]; float n = b.frac;
        if (b.paramId && model.has(b.paramId)) n = model.getNorm(b.paramId);
        g.setColour(juce::Colour(b.bg)); g.fillRoundedRectangle(b.x, b.y, b.w, b.h, b.rad);
        g.setColour(juce::Colour(b.fill)); g.fillRoundedRectangle(b.x, b.y, juce::jmax(0.0f, b.w * n), b.h, b.rad);
    }

    bool toggleOn(size_t t) const { const auto id = juce::String(home::toggles[t].paramId); return model.has(id) ? model.getNorm(id) > 0.5f : home::toggles[t].onBg == home::rects[home::toggles[t].rect].bg; }
    int sceneSel() const { const juce::String id("s_0600022a"); return model.has(id) ? juce::roundToInt(model.getNorm(id) * 7.0f) : 0; }

    void timerCallback() override
    {
        if (!isShowing()) return;
        bool changed = false;
        for (size_t i = 0; i < std::size(home::knobs); ++i)
        {
            const auto& k = home::knobs[i]; if (!k.paramId || !model.has(k.paramId)) continue;
            const float v = model.getNorm(k.paramId); if (v != lastKnob[i]) { lastKnob[i] = v; changed = true; }
        }
        for (size_t i = 0; i < std::size(home::bars); ++i)
        {
            const auto& b = home::bars[i]; if (!b.paramId || !model.has(b.paramId)) continue;
            const float v = model.getNorm(b.paramId); if (v != lastBar[i]) { lastBar[i] = v; changed = true; }
        }
        if (changed) repaint();
    }
};
}
