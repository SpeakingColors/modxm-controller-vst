// MidiCore: pure C++ (no JUCE). Parameter map parsing, MIDI encoding and decoding for CC and NRPN.
// Only things listed in map/params.csv are ever sent. Nothing here invents SysEx.
#pragma once
#include <algorithm>
#include <cstdint>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace ctl
{
enum class Kind { CC, NRPN, Pair };   // Pair = two CCs that form one value; the manual does not say how, so it is not sent yet

struct Param
{
    std::string id, group, name;
    Kind kind = Kind::CC;
    int a = 0;        // CC number, or NRPN MSB, or first CC of a pair
    int b = 0;        // NRPN LSB, or second CC of a pair
    int minV = 0, maxV = 127, defV = 0;
    bool rangeFromManual = false;   // false: 0 to 127 assumed, not printed in the manual
    bool hwVerified = false;        // true only after the value was tested on the real device
    std::string idFor() const { return id; }
};

struct ParseResult { std::vector<Param> params; std::vector<std::string> errors; };

inline std::vector<std::string> splitCsv(const std::string& line)
{
    std::vector<std::string> out; std::string cur; bool q = false;
    for (size_t i = 0; i < line.size(); ++i)
    {
        char c = line[i];
        if (q) { if (c == '"') { if (i + 1 < line.size() && line[i + 1] == '"') { cur += '"'; ++i; } else q = false; } else cur += c; }
        else if (c == '"') q = true;
        else if (c == ',') { out.push_back(cur); cur.clear(); }
        else if (c != '\r') cur += c;
    }
    out.push_back(cur); return out;
}

inline bool toInt(const std::string& s, int& v)
{
    if (s.empty() || s.size() > 6) return false;
    size_t i = 0; if (s[0] == '-') i = 1; if (i >= s.size()) return false;
    for (size_t k = i; k < s.size(); ++k) if (s[k] < '0' || s[k] > '9') return false;
    v = std::stoi(s); return true;
}

// Columns: id,group,name,kind,a,b,min,max,def,range_src,hw
inline ParseResult parseMap(const std::string& text)
{
    ParseResult r; std::istringstream in(text); std::string line; int ln = 0; std::set<std::string> ids; std::set<std::string> addr;
    while (std::getline(in, line))
    {
        ++ln; if (line.empty() || line[0] == '#' || line.rfind("id,", 0) == 0) continue;
        auto f = splitCsv(line); auto err = [&](const std::string& m) { r.errors.push_back("line " + std::to_string(ln) + ": " + m); };
        if (f.size() != 11) { err("expected 11 columns"); continue; }
        Param p; p.id = f[0]; p.group = f[1]; p.name = f[2];
        if (p.id.empty() || p.name.empty()) { err("empty id or name"); continue; }
        if (f[3] == "cc") p.kind = Kind::CC; else if (f[3] == "nrpn") p.kind = Kind::NRPN; else if (f[3] == "pair") p.kind = Kind::Pair; else { err("unknown kind"); continue; }
        if (!toInt(f[4], p.a) || !toInt(f[5], p.b) || !toInt(f[6], p.minV) || !toInt(f[7], p.maxV) || !toInt(f[8], p.defV)) { err("bad number"); continue; }
        if (p.a < 0 || p.a > 127 || p.b < 0 || p.b > 127) { err("number outside 0..127"); continue; }
        if (p.minV < 0 || p.maxV > 127 || p.minV > p.maxV || p.defV < p.minV || p.defV > p.maxV) { err("bad range"); continue; }
        p.rangeFromManual = f[9] == "manual"; p.hwVerified = f[10] == "1";
        if (!ids.insert(p.id).second) { err("duplicate id " + p.id); continue; }
        std::string key = std::to_string(int(p.kind)) + ":" + std::to_string(p.a) + ":" + std::to_string(p.kind == Kind::CC ? 0 : p.b);
        if (!addr.insert(key).second) { err("duplicate address for " + p.id); continue; }
        r.params.push_back(p);
    }
    return r;
}

struct Msg { uint8_t status = 0, d1 = 0, d2 = 0; };   // always a 3 byte channel message here

inline Msg cc(int ch /*1..16*/, int num, int val)
{
    Msg m; m.status = uint8_t(0xB0 | ((std::clamp(ch, 1, 16) - 1) & 15)); m.d1 = uint8_t(std::clamp(num, 0, 127)); m.d2 = uint8_t(std::clamp(val, 0, 127)); return m;
}

// NRPN with 7 bit data: CC 99 (MSB), CC 98 (LSB), CC 6 (data). No null message is sent afterwards.
inline std::vector<Msg> nrpn(int ch, int msb, int lsb, int val)
{
    return { cc(ch, 99, msb), cc(ch, 98, lsb), cc(ch, 6, val) };
}

// What to send for one parameter value. Pair parameters send nothing (not verified how the two CCs combine).
inline std::vector<Msg> encode(const Param& p, int ch, int val)
{
    val = std::clamp(val, p.minV, p.maxV);
    if (p.kind == Kind::CC) return { cc(ch, p.a, val) };
    if (p.kind == Kind::NRPN) return nrpn(ch, p.a, p.b, val);
    return {};
}

struct Event { int paramIndex = -1; int value = 0; };

// Turns incoming CC messages into parameter events. Follows the NRPN CC 99 / 98 / 6 sequence.
class Decoder
{
public:
    explicit Decoder(const std::vector<Param>& map) : params(map) {}
    void reset() { msb = lsb = -1; }
    std::optional<Event> feedCC(int num, int val)
    {
        if (num == 99) { msb = val; return std::nullopt; }
        if (num == 98) { lsb = val; return std::nullopt; }
        if (num == 6)
        {
            if (msb < 0 || lsb < 0) return std::nullopt;
            return find(Kind::NRPN, msb, lsb, val);   // standard MIDI: later data entries keep addressing the same NRPN
        }
        return find(Kind::CC, num, 0, val);
    }
private:
    std::optional<Event> find(Kind k, int a, int b, int val) const
    {
        for (size_t i = 0; i < params.size(); ++i)
        {
            const auto& p = params[i];
            if (p.kind == k && p.a == a && (k == Kind::CC || p.b == b))
                return Event{ int(i), std::clamp(val, p.minV, p.maxV) };
        }
        return std::nullopt;
    }
    const std::vector<Param>& params;
    int msb = -1, lsb = -1;
};
}
