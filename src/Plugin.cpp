// MODX M Controller controller plugin (Studio 87). Sends and receives the MIDI CC / NRPN listed in map/params.csv.
// Parameters are automatable. Incoming CC from the device moves the parameters; the plugin never echoes it back.
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include <map>
#include <set>
#include <array>
#include "BinaryData.h"
#include "MidiCore.h"
#include "SysexMap.h"
#include "catui/CatFrame.h"
#include "catui/RenderGate.h"
#include "HomePage.h"

namespace
{
constexpr int kMaxParams = 400;     // keeps the host's parameter list reasonable
constexpr int kPerBlock = 16;       // at most this many parameters are sent per audio block, so a device is never flooded

ctl::ParseResult loadMap()
{
    auto r = ctl::parseMap(std::string(BinaryData::params_csv, size_t(BinaryData::params_csvSize)));
    auto sx = ctl::parseSysexMap(std::string(BinaryData::sysex_params_csv, size_t(BinaryData::sysex_params_csvSize)));
    for (auto& p : sx.params) r.params.push_back(p);
    for (auto& e : sx.errors) r.errors.push_back(e);
    return r;
}
}

class CtlProcessor : public juce::AudioProcessor, private juce::AudioProcessorParameter::Listener, private juce::Timer
{
public:
    CtlProcessor() : AudioProcessor(BusesProperties()), mapRes(loadMap()), decoder(mapRes.params)
    {
        auto ch = std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ "midi_channel", 1 }, "MIDI channel", 1, 16, 1);
        channelParam = ch.get(); addParameter(ch.release());
        auto pt = std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ "part", 1 }, "Part (for part controls)", 1, 16, 1);
        partParam = pt.get(); addParameter(pt.release());
        auto dv = std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ "device_number", 1 }, "Device number (SysEx)", 1, 16, 1);
        deviceParam = dv.get(); addParameter(dv.release());
        for (auto& p : mapRes.params)
        {
            if (p.kind == ctl::Kind::Pair || int(slots.size()) >= kMaxParams) continue;
            auto ip = std::make_unique<juce::AudioParameterInt>(juce::ParameterID{ p.id, 1 }, p.name, p.minV, p.maxV, p.defV);
            Slot s; s.param = ip.get(); s.map = int(&p - mapRes.params.data()); slots.push_back(s);
            addParameter(ip.release());
        }
        for (int k = 0; k < kBank; ++k)
            for (size_t i = 0; i < mapRes.params.size(); ++i) if (mapRes.params[i].id == kBankIds[k]) bankMap[k] = int(i);
        for (int part = 0; part < 16; ++part) for (int k = 0; k < kBank; ++k) bank[part][k] = bankMap[k] >= 0 ? mapRes.params[size_t(bankMap[k])].defV : 0;
        for (auto& sl : slots) if (sl.param->paramID == "s_0600021e") tempoParam = sl.param;
        pending = std::make_unique<std::atomic<int>[]>(slots.size()); lastRecv = std::make_unique<std::atomic<int>[]>(slots.size()); incoming = std::make_unique<std::atomic<int>[]>(slots.size());
        for (size_t i = 0; i < slots.size(); ++i) { pending[i] = -1; lastRecv[i] = -1; incoming[i] = -1; slots[i].param->addListener(this); }
        startTimerHz(30);
    }
    ~CtlProcessor() override { stopTimer(); for (auto& s : slots) s.param->removeListener(this); }

    const ctl::ParseResult mapRes;
    struct Slot { juce::AudioParameterInt* param = nullptr; int map = 0; };
    std::vector<Slot> slots;
    juce::AudioParameterInt* channelParam = nullptr;
    juce::AudioParameterInt* partParam = nullptr;     // which part the Part controls address (1 to 16)
    juce::AudioParameterInt* deviceParam = nullptr;   // MODX M "MIDI device number" (1 to 16 shown, 0 to 15 on the wire)

    void sendAll() { for (size_t i = 0; i < slots.size(); ++i) pending[i] = slots[i].param->get(); }

    // ---- per-part bank: mute, volume and pan of all 16 parts (the Home page part table). The device is the source of truth. ----
    static constexpr int kBank = 3;
    static constexpr const char* kBankIds[kBank] = { "s_1p000103", "s_1p000210", "s_1p000212" };
    int bankMap[kBank] = { -1, -1, -1 };
    std::atomic<int> bank[16][kBank];
    int bankGet(int part, int k) const { return bank[part - 1][k].load(); }
    void bankSet(int part, int k, int v)
    {
        const auto& p = mapRes.params[size_t(bankMap[k])]; v = juce::jlimit(p.minV, p.maxV, v);
        bank[part - 1][k] = v; pushOut({ bankMap[k], part, v, false });
    }
    // read the current values from the device: every Home control and the part bank
    void requestAll()
    {
        for (auto& s : slots)
        {
            const auto& p = mapRes.params[size_t(s.map)];
            if (p.kind == ctl::Kind::Sysex) pushOut({ s.map, p.perPart ? partParam->get() : 1, 0, true });
        }
        for (int part = 1; part <= 16; ++part) for (int k = 0; k < kBank; ++k) pushOut({ bankMap[k], part, 0, true });
    }
    juce::AudioParameterInt* tempoParam = nullptr;
    std::atomic<float> hostBpm{ 0.0f };
    std::atomic<bool> tempoSync{ false };

    struct Out { int map = 0, part = 1, value = 0; bool request = false; };
    void pushOut(const Out& o)
    {
        const juce::ScopedLock sl(outLock); if (outQueue.size() < 4096) outQueue.push_back(o);
    }

    void prepareToPlay(double, int) override { decoder.reset(); }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }

    void processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals nd; audio.clear();
        // 1. incoming from the device: CC messages are consumed here, everything else passes through unchanged
        juce::MidiBuffer passthru;
        for (const auto meta : midi)
        {
            const auto msg = meta.getMessage();
            if (msg.isController())
            {
                if (auto e = decoder.feedCC(msg.getControllerNumber(), msg.getControllerValue()))
                    for (size_t i = 0; i < slots.size(); ++i) if (slots[i].map == e->paramIndex) { incoming[i] = e->value; break; }
            }
            else if (msg.isSysEx())
            {
                // MODX M parameter changes move the matching parameter and are consumed; any other SysEx passes through
                ctl::Bytes raw; raw.push_back(0xF0); raw.insert(raw.end(), msg.getSysExData(), msg.getSysExData() + msg.getSysExDataSize()); raw.push_back(0xF7);
                const auto parsed = ctl::parse(raw);
                const bool partAddr = parsed.type == ctl::SysexType::ParameterChange && parsed.addr[0] >= 0x10 && parsed.addr[0] <= 0x1F;
                const int pp = partAddr ? parsed.addr[0] - 0x0F : partParam->get();
                bool consumed = false;
                if (auto e = ctl::decodeSysex(mapRes.params, parsed, pp))
                {
                    consumed = true;
                    if (partAddr) for (int k = 0; k < kBank; ++k) if (e->paramIndex == bankMap[k]) bank[pp - 1][k] = e->value;
                    if (!partAddr || pp == partParam->get())
                        for (size_t i = 0; i < slots.size(); ++i) if (slots[i].map == e->paramIndex) { incoming[i] = e->value; break; }
                }
                if (!consumed) passthru.addEvent(msg, meta.samplePosition);
            }
            else passthru.addEvent(msg, meta.samplePosition);
        }
        midi.swapWith(passthru);
        if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto bpm = pos->getBpm()) hostBpm = float(*bpm);
        // 2. outgoing: changed parameters, a few per block
        const int ch = channelParam->get(); const int n = int(slots.size()); int sent = 0, pos = 0;
        for (int k = 0; k < n && sent < kPerBlock; ++k)
        {
            const int i = (rotate + k) % n; const int v = pending[size_t(i)].exchange(-1); if (v < 0) continue;
            const auto& prm = mapRes.params[size_t(slots[size_t(i)].map)];
            if (prm.kind == ctl::Kind::Sysex)
            {
                if (auto sx = ctl::encodeSysex(prm, deviceParam->get() - 1, partParam->get(), v))   // F0 ... F7; JUCE wants the bytes between
                    midi.addEvent(juce::MidiMessage::createSysExMessage(sx->data() + 1, int(sx->size()) - 2), juce::jmin(pos++, juce::jmax(0, audio.getNumSamples() - 1)));
            }
            else
                for (const auto& m : ctl::encode(prm, ch, v))
                { const juce::uint8 bytes[3] = { m.status, m.d1, m.d2 }; midi.addEvent(bytes, 3, juce::jmin(pos++, juce::jmax(0, audio.getNumSamples() - 1))); }
            ++sent; rotate = (i + 1) % n;
        }
        // per-part values and read requests queued by the editor
        std::vector<Out> todo;
        { const juce::ScopedTryLock sl(outLock); if (sl.isLocked()) { const size_t take = std::min<size_t>(outQueue.size(), size_t(kPerBlock)); todo.assign(outQueue.begin(), outQueue.begin() + long(take)); outQueue.erase(outQueue.begin(), outQueue.begin() + long(take)); } }
        for (const auto& o : todo)
        {
            const auto& prm = mapRes.params[size_t(o.map)];
            auto msg = o.request ? ctl::requestSysex(prm, deviceParam->get() - 1, o.part) : ctl::encodeSysex(prm, deviceParam->get() - 1, o.part, o.value);
            if (msg) midi.addEvent(juce::MidiMessage::createSysExMessage(msg->data() + 1, int(msg->size()) - 2), juce::jmin(pos++, juce::jmax(0, audio.getNumSamples() - 1)));
        }
    }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "MODX M Controller"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return true; }
    double getTailLengthSeconds() const override { return 0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& dest) override
    {
        juce::XmlElement x("STATE"); x.setAttribute("v", 1); x.setAttribute("midi_channel", channelParam->get()); x.setAttribute("part", partParam->get()); x.setAttribute("device_number", deviceParam->get()); x.setAttribute("tempo_sync", tempoSync.load() ? 1 : 0);
        for (auto& s : slots) x.setAttribute(s.param->paramID, s.param->get());
        copyXmlToBinary(x, dest);
    }
    void setStateInformation(const void* data, int size) override
    {
        if (auto x = getXmlFromBinary(data, size))
        {
            if (!x->hasTagName("STATE")) return;
            const int ch = x->getIntAttribute("midi_channel", 1); *channelParam = juce::jlimit(1, 16, ch);
            *partParam = juce::jlimit(1, 16, x->getIntAttribute("part", 1)); *deviceParam = juce::jlimit(1, 16, x->getIntAttribute("device_number", 1)); tempoSync = x->getIntAttribute("tempo_sync", 0) != 0;
            for (auto& s : slots) if (x->hasAttribute(s.param->paramID)) setFromState(s, x->getIntAttribute(s.param->paramID));
        }
    }

private:
    void setFromState(Slot& s, int v)
    {
        const size_t i = size_t(&s - slots.data()); const auto r = s.param->getRange();
        v = juce::jlimit(r.getStart(), r.getEnd(), v); lastRecv[i] = v; *s.param = v;   // restoring a project does not send MIDI
    }
    void parameterValueChanged(int, float) override {}
    void parameterGestureChanged(int, bool) override {}
    void timerCallback() override
    {
        if (lastSeen.size() != slots.size()) { lastSeen.resize(slots.size()); for (size_t i = 0; i < slots.size(); ++i) lastSeen[i] = slots[i].param->get(); }
        if (tempoSync.load() && tempoParam != nullptr && hostBpm.load() > 0.0f)
        { const int t = juce::jlimit(tempoParam->getRange().getStart(), tempoParam->getRange().getEnd(), juce::roundToInt(hostBpm.load())); if (tempoParam->get() != t) *tempoParam = t; }
        // device to host: a parameter set from incoming MIDI is never sent back out
        for (size_t i = 0; i < slots.size(); ++i)
        {
            const int v = incoming[i].exchange(-1); if (v < 0 || slots[i].param->get() == v) continue;
            lastRecv[i] = v; *slots[i].param = v;
        }
        // host or UI to device
        for (size_t i = 0; i < slots.size(); ++i)
        {
            const int v = slots[i].param->get(); if (v == lastSeen[i]) continue;
            lastSeen[i] = v;
            if (lastRecv[i].load() == v) lastRecv[i] = -1; else pending[i] = v;
        }
    }

    ctl::Decoder decoder;
    juce::CriticalSection outLock; std::vector<Out> outQueue;
    std::unique_ptr<std::atomic<int>[]> pending, lastRecv, incoming;
    std::vector<int> lastSeen; int rotate = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CtlProcessor)
};

// ----- editor -----
class ParamRow : public juce::Component
{
public:
    ParamRow(juce::AudioParameterInt& p, const juce::String& info) : label(p.getName(100)), tip(info)
    {
        addAndMakeVisible(label); addAndMakeVisible(slider);
        slider.setSliderStyle(juce::Slider::LinearHorizontal); slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 48, 20);
        attach = std::make_unique<juce::SliderParameterAttachment>(p, slider);
        setTooltip(info);
    }
    void resized() override { auto r = getLocalBounds().reduced(4, 2); label.setBounds(r.removeFromLeft(r.getWidth() * 2 / 5)); slider.setBounds(r); }
    juce::Label label; juce::Slider slider; juce::String tip;
    void setTooltip(const juce::String& t) { label.setTooltip(t); }
    std::unique_ptr<juce::SliderParameterAttachment> attach;
};

// Developer list of every parameter (kept for testing the map; opened from the gear menu, never the main window)
class DevListEditor : public juce::Component, private juce::ListBoxModel
{
public:
    explicit DevListEditor(CtlProcessor& p) : proc(p), frame("MODX M Controller", 760, 560)
    {
        addAndMakeVisible(frame); auto& c = frame.content();
        std::set<juce::String> groups; for (auto& s : proc.slots) groups.insert(juce::String(proc.mapRes.params[size_t(s.map)].group));
        group.addItem("All groups", 1); int id = 2; for (auto& g : groups) group.addItem(g, id++);
        group.setSelectedId(1, juce::dontSendNotification); group.onChange = [this] { rebuild(); };
        filter.setTextToShowWhenEmpty("search", juce::Colours::grey); filter.onTextChange = [this] { rebuild(); };
        sendAll.setButtonText("Send all"); sendAll.setTooltip("Sends every parameter value to the device now."); sendAll.onClick = [this] { proc.sendAll(); };
        chLabel.setText("Ch", juce::dontSendNotification); partLabel.setText("Part", juce::dontSendNotification); devLabel.setText("Dev", juce::dontSendNotification);
        for (auto* sl : { &partSlider, &devSlider }) { sl->setSliderStyle(juce::Slider::IncDecButtons); sl->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 30, 24); }
        partAttach = std::make_unique<juce::SliderParameterAttachment>(*proc.partParam, partSlider); devAttach = std::make_unique<juce::SliderParameterAttachment>(*proc.deviceParam, devSlider);
        partSlider.setTooltip("Which part (1 to 16) the Part and Arp controls address."); devSlider.setTooltip("Must match the MODX M setting MIDI device number (Utility, MIDI I/O).");
        chSlider.setSliderStyle(juce::Slider::IncDecButtons); chSlider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 36, 24);
        chAttach = std::make_unique<juce::SliderParameterAttachment>(*proc.channelParam, chSlider);
        int hw = 0, manual = 0; for (auto& s : proc.slots) { hw += proc.mapRes.params[size_t(s.map)].hwVerified; manual++; }
        int pairs = 0; for (auto& q : proc.mapRes.params) pairs += q.kind == ctl::Kind::Pair;
        status.setText(juce::String(manual) + " parameters from the Yamaha data list, " + juce::String(hw) + " tested on the device" + (pairs ? ", " + juce::String(pairs) + " two-CC controls not supported yet" : "")
                       + ". SysEx goes to the MIDI port you route this plugin to (the data list says ESP uses USB port 4). Turn on CC receive and transmit on the device.", juce::dontSendNotification);
        status.setFont(juce::FontOptions(12.0f));
        for (auto* w : std::initializer_list<juce::Component*>{ &group, &filter, &sendAll, &chLabel, &chSlider, &partLabel, &partSlider, &devLabel, &devSlider, &status, &list }) c.addAndMakeVisible(w);
        list.setModel(this); list.setRowHeight(30);
        frame.header().onInfo = [this] { juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "MODX M Controller",
            "Free and open source by Studio 87. Values come from the maker's manual. Nothing is sent that is not in the map, and nothing is invented. Support: ko-fi.com/studio87"); };
        rebuild(); setSize(760, 560);
    }
    void resized() override
    {
        frame.setBounds(getLocalBounds()); auto r = frame.content().getLocalBounds().reduced(10);
        auto top = r.removeFromTop(28); group.setBounds(top.removeFromLeft(130)); top.removeFromLeft(6); filter.setBounds(top.removeFromLeft(110)); top.removeFromLeft(6);
        sendAll.setBounds(top.removeFromLeft(70)); top.removeFromLeft(6); chLabel.setBounds(top.removeFromLeft(24)); chSlider.setBounds(top.removeFromLeft(78)); top.removeFromLeft(4);
        partLabel.setBounds(top.removeFromLeft(34)); partSlider.setBounds(top.removeFromLeft(78)); top.removeFromLeft(4); devLabel.setBounds(top.removeFromLeft(30)); devSlider.setBounds(top.removeFromLeft(78));
        r.removeFromTop(6); status.setBounds(r.removeFromBottom(34)); list.setBounds(r);
    }
private:
    int getNumRows() override { return int(shown.size()); }
    void paintListBoxItem(int, juce::Graphics&, int, int, bool) override {}
    juce::Component* refreshComponentForRow(int row, bool, juce::Component* existing) override
    {
        delete existing; if (row < 0 || row >= int(shown.size())) return nullptr;
        auto& s = proc.slots[size_t(shown[size_t(row)])]; auto& m = proc.mapRes.params[size_t(s.map)];
        juce::String addr = m.kind == ctl::Kind::CC ? "CC " + juce::String(m.a)
            : m.kind == ctl::Kind::NRPN ? "NRPN " + juce::String(m.a) + ":" + juce::String(m.b)
            : "SysEx " + juce::String::toHexString(m.sysAddr[0]).paddedLeft('0', 2).toUpperCase() + " " + juce::String::toHexString(m.sysAddr[1]).paddedLeft('0', 2).toUpperCase()
              + " " + juce::String::toHexString(m.sysAddr[2]).paddedLeft('0', 2).toUpperCase() + " " + juce::String::toHexString(m.sysAddr[3]).paddedLeft('0', 2).toUpperCase() + (m.perPart ? " (part address)" : "");
        return new ParamRow(*s.param, addr + (m.rangeFromManual ? ", range from the manual" : ", range 0 to 127 assumed") + (m.hwVerified ? ", tested on hardware" : ", not tested on hardware yet"));
    }
    void rebuild()
    {
        shown.clear(); const auto g = group.getSelectedId() == 1 ? juce::String() : group.getText(); const auto f = filter.getText().toLowerCase();
        for (size_t i = 0; i < proc.slots.size(); ++i)
        {
            auto& m = proc.mapRes.params[size_t(proc.slots[i].map)];
            if (g.isNotEmpty() && juce::String(m.group) != g) continue;
            if (f.isNotEmpty() && !(juce::String(m.name).toLowerCase().contains(f) || juce::String(m.group).toLowerCase().contains(f))) continue;
            shown.push_back(int(i));
        }
        list.updateContent(); list.repaint();
    }
    CtlProcessor& proc; catui::CatFrame frame; juce::ComboBox group; juce::TextEditor filter; juce::TextButton sendAll; juce::Label chLabel, partLabel, devLabel, status; juce::Slider chSlider, partSlider, devSlider;
    std::unique_ptr<juce::SliderParameterAttachment> chAttach, partAttach, devAttach; juce::ListBox list; std::vector<int> shown;
};

//==============================================================================
// Bridges the Home page to the processor's parameters (by the ids in map/*.csv)
class ProcModel : public modx::HomeModel
{
public:
    explicit ProcModel(CtlProcessor& p) : proc(p) { for (size_t i = 0; i < p.slots.size(); ++i) byId[p.slots[i].param->paramID] = i; }
    // ids "p<part>:<id>" address the per-part bank (mute, volume, pan of one part); everything else is a normal parameter
    struct PartId { int part = 0, k = -1; };
    PartId partId(const juce::String& id) const
    {
        PartId r; if (!id.startsWithChar('p') || id.indexOfChar(':') < 0) return r;
        const int part = id.substring(1, id.indexOfChar(':')).getIntValue(); const auto base = id.fromFirstOccurrenceOf(":", false, false);
        for (int k = 0; k < CtlProcessor::kBank; ++k) if (base == CtlProcessor::kBankIds[k] && part >= 1 && part <= 16 && proc.bankMap[k] >= 0) { r.part = part; r.k = k; }
        return r;
    }
    const ctl::Param& bankParam(int k) const { return proc.mapRes.params[size_t(proc.bankMap[k])]; }
    bool has(const juce::String& id) const override { return byId.count(id) > 0 || partId(id).k >= 0; }
    float getNorm(const juce::String& id) const override
    {
        if (auto pi = partId(id); pi.k >= 0) { const auto& p = bankParam(pi.k); return p.maxV > p.minV ? float(proc.bankGet(pi.part, pi.k) - p.minV) / float(p.maxV - p.minV) : 0.0f; }
        return par(id).getValue();
    }
    float defaultNorm(const juce::String& id) const override
    {
        if (auto pi = partId(id); pi.k >= 0) { const auto& p = bankParam(pi.k); return p.maxV > p.minV ? float(p.defV - p.minV) / float(p.maxV - p.minV) : 0.0f; }
        return par(id).getDefaultValue();
    }
    juce::String valueText(const juce::String& id, float norm) const override
    {
        int v;
        if (auto pi = partId(id); pi.k >= 0) { const auto& p = bankParam(pi.k); v = p.minV + juce::roundToInt(norm * float(p.maxV - p.minV)); }
        else v = par(id).convertFrom0to1(norm);
        if (id == "s_06000208" || id.endsWith(":s_1p000212")) return v == 64 ? juce::String("C") : v < 64 ? "L" + juce::String(64 - v) : "R" + juce::String(v - 64);   // pan: 64 is centre
        return juce::String(v);
    }
    juce::String describe(const juce::String& id) const override
    {
        auto hex = [](int v) { return juce::String::toHexString(v).paddedLeft('0', 2).toUpperCase(); };
        if (auto pi = partId(id); pi.k >= 0)
        {
            const auto& m = bankParam(pi.k);
            return juce::String(m.name) + ", part " + juce::String(pi.part) + "\nRange " + juce::String(m.minV) + " to " + juce::String(m.maxV) + ", now " + juce::String(proc.bankGet(pi.part, pi.k)) + "\nSysEx " + hex(0x10 + pi.part - 1) + " " + hex(m.sysAddr[1]) + " " + hex(m.sysAddr[2]) + " " + hex(m.sysAddr[3]) + (m.hwVerified ? "" : ", not tested on the device yet");
        }
        const auto& s = proc.slots[byId.at(id)]; const auto& m = proc.mapRes.params[size_t(s.map)]; const auto r = s.param->getRange();
        juce::String a = m.kind == ctl::Kind::CC ? "CC " + juce::String(m.a) : m.kind == ctl::Kind::NRPN ? "NRPN " + juce::String(m.a) + ":" + juce::String(m.b)
            : "SysEx " + hex(m.sysAddr[0]) + " " + hex(m.sysAddr[1]) + " " + hex(m.sysAddr[2]) + " " + hex(m.sysAddr[3]) + (m.perPart ? " (part " + juce::String(proc.partParam->get()) + ")" : juce::String());
        return juce::String(m.name) + "\nRange " + juce::String(r.getStart()) + " to " + juce::String(r.getEnd()) + ", now " + juce::String(s.param->get()) + "\n" + a + (m.hwVerified ? "" : ", not tested on the device yet");
    }
    int tempoValue() const override { return proc.tempoParam != nullptr ? proc.tempoParam->get() : 128; }
    float hostBpm() const override { return proc.hostBpm.load(); }
    bool tempoSync() const override { return proc.tempoSync.load(); }
    void setTempoSync(bool b) override { proc.tempoSync = b; }
    void beginEdit(const juce::String& id) override { if (partId(id).k < 0) par(id).beginChangeGesture(); }
    void setNorm(const juce::String& id, float n) override
    {
        if (auto pi = partId(id); pi.k >= 0) { const auto& p = bankParam(pi.k); proc.bankSet(pi.part, pi.k, p.minV + juce::roundToInt(juce::jlimit(0.0f, 1.0f, n) * float(p.maxV - p.minV))); return; }
        par(id).setValueNotifyingHost(n);
    }
    void endEdit(const juce::String& id) override { if (partId(id).k < 0) par(id).endChangeGesture(); }
private:
    juce::RangedAudioParameter& par(const juce::String& id) const { return *static_cast<juce::RangedAudioParameter*>(proc.slots[byId.at(id)].param); }
    CtlProcessor& proc; std::map<juce::String, size_t> byId;
};

// The plugin window: the Home page ported from the approved mockup, resizable, double click the wood for 100 %
class HomeEditor : public juce::AudioProcessorEditor
{
public:
    explicit HomeEditor(CtlProcessor& p) : AudioProcessorEditor(p), proc(p), model(p), page(model)
    {
        static const bool once = []
        {
            modx::HomePage::setFonts(juce::Typeface::createSystemTypefaceFor(BinaryData::RobotoCondensedRegular_ttf, size_t(BinaryData::RobotoCondensedRegular_ttfSize)),
                                     juce::Typeface::createSystemTypefaceFor(BinaryData::RobotoCondensedMedium_ttf, size_t(BinaryData::RobotoCondensedMedium_ttfSize)),
                                     juce::Typeface::createSystemTypefaceFor(BinaryData::RobotoCondensedBold_ttf, size_t(BinaryData::RobotoCondensedBold_ttfSize)));
            return true;
        }();
        juce::ignoreUnused(once);
        addAndMakeVisible(page);
        page.onReset100 = page.onWoodDoubleClick = [this] { setSize(modx::HomePage::kTotalW, modx::HomePage::kTotalH); };
        page.onInfo = [] { juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "MODX M Controller",
            "Free and open source by Studio 87. Values come from the Yamaha MODX M Data List. Nothing is sent that is not in the map, and nothing is invented. No data is collected, ever. Support: ko-fi.com/studio87"); };
        page.onGear = [this] { showMenu(); };
        setResizable(true, false);   // edges and corners come from the border below
        getConstrainer()->setFixedAspectRatio(double(modx::HomePage::kTotalW) / double(modx::HomePage::kTotalH));
        setResizeLimits(modx::HomePage::kTotalW / 2, modx::HomePage::kTotalH / 2, modx::HomePage::kTotalW * 2, modx::HomePage::kTotalH * 2);
        border = std::make_unique<juce::ResizableBorderComponent>(this, getConstrainer()); border->setBorderThickness(juce::BorderSize<int>(4));
        addAndMakeVisible(*border);
        setSize(modx::HomePage::kTotalW, modx::HomePage::kTotalH);
    }
    void resized() override { page.setBounds(getLocalBounds()); border->setBounds(getLocalBounds()); }

private:
    void showMenu()
    {
        juce::PopupMenu m, ch, dv, pt;
        for (int i = 1; i <= 16; ++i) { ch.addItem(100 + i, juce::String(i), true, proc.channelParam->get() == i); dv.addItem(200 + i, juce::String(i), true, proc.deviceParam->get() == i); pt.addItem(300 + i, juce::String(i), true, proc.partParam->get() == i); }
        m.addSubMenu("MIDI channel (CC controls)", ch); m.addSubMenu("Device number (SysEx), must match the MODX M", dv); m.addSubMenu("Part for part controls", pt);
        m.addSeparator(); m.addItem(1, "Send all values to the device now"); m.addItem(4, "Read all values from the device now"); m.addItem(5, "Send the DAW tempo to the MODX M", true, proc.tempoSync.load()); m.addItem(2, "Info bubbles", true, page.bubblesOn); m.addItem(3, "Developer parameter list...");
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&page).withMinimumWidth(260), [this](int r)
        {
            if (r >= 101 && r <= 116) *proc.channelParam = r - 100; else if (r >= 201 && r <= 216) *proc.deviceParam = r - 200; else if (r >= 301 && r <= 316) *proc.partParam = r - 300;
            else if (r == 1) proc.sendAll(); else if (r == 4) proc.requestAll(); else if (r == 5) proc.tempoSync = !proc.tempoSync.load(); else if (r == 2) { page.bubblesOn = !page.bubblesOn; page.repaint(); }
            else if (r == 3)
            {
                juce::DialogWindow::LaunchOptions lo; lo.dialogTitle = "MODX M Controller: parameter list"; lo.content.setOwned(new DevListEditor(proc));
                lo.resizable = true; lo.useNativeTitleBar = true; lo.escapeKeyTriggersCloseButton = true; lo.launchAsync();
            }
        });
    }
    CtlProcessor& proc; ProcModel model; modx::HomePage page; std::unique_ptr<juce::ResizableBorderComponent> border;
};

juce::AudioProcessorEditor* CtlProcessor::createEditor() { return new HomeEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new CtlProcessor(); }
