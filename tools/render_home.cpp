// Dev tool: renders the real Home page (HomePage.h) offscreen to a PNG so it can be compared with the mockup screenshot.
// usage: render_home <fonts dir> <out.png> [scale]
#include <juce_gui_basics/juce_gui_basics.h>
#include "HomePage.h"

struct StubModel : modx::HomeModel
{
    bool has(const juce::String& id) const override { return find(id) >= 0; }
    float getNorm(const juce::String& id) const override
    {
        for (auto& k : modx::home::knobs) if (k.paramId && id == k.paramId) return k.frac;
        for (auto& b : modx::home::bars) if (b.paramId && id == b.paramId) return b.frac;
        for (auto& t : modx::home::toggles) if (id == t.paramId) return modx::home::rects[t.rect].bg == t.onBg ? 1.0f : 0.0f;
        return 0.0f;
    }
    float defaultNorm(const juce::String&) const override { return 0.5f; }
    juce::String valueText(const juce::String& id, float) const override { for (auto& k : modx::home::knobs) if (k.paramId && id == k.paramId) return k.val; return {}; }
    juce::String describe(const juce::String& id) const override { return id; }
    void beginEdit(const juce::String&) override {}
    void setNorm(const juce::String&, float) override {}
    void endEdit(const juce::String&) override {}
    int find(const juce::String& id) const
    {
        for (auto& k : modx::home::knobs) if (k.paramId && id == k.paramId) return 1;
        for (auto& b : modx::home::bars) if (b.paramId && id == b.paramId) return 1;
        for (auto& t : modx::home::toggles) if (id == t.paramId) return 1;
        if (id == "s_0600022a") return 1;
        return -1;
    }
};

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3) return 2;
    juce::File dir(argv[1]);
    auto load = [&](const char* n) { juce::MemoryBlock mb; dir.getChildFile(n).loadFileAsData(mb); return juce::Typeface::createSystemTypefaceFor(mb.getData(), mb.getSize()); };
    modx::HomePage::setFonts(load("RobotoCondensed-Regular.ttf"), load("RobotoCondensed-Medium.ttf"), load("RobotoCondensed-Bold.ttf"));
    StubModel m; modx::HomePage page(m);
    const float scale = argc > 3 ? float(std::atof(argv[3])) : 1.0f;
    page.setSize(juce::roundToInt(modx::HomePage::kTotalW * scale), juce::roundToInt(modx::HomePage::kTotalH * scale));
    auto img = page.createComponentSnapshot(page.getLocalBounds(), false, 1.0f);
    juce::File out(argv[2]); out.deleteFile(); juce::FileOutputStream os(out); juce::PNGImageFormat().writeImageToStream(img, os);
    return 0;
}
