// SysexMap: pure C++. Loads map/sysex_params.csv (generated from the Yamaha MODX M Data List by tools/gen_sysex_map.py)
// and turns parameter values into MODX M parameter change messages and back. Official doc only, not verified on hardware.
#pragma once
#include "MidiCore.h"
#include "SysexCore.h"

namespace ctl
{
inline int hexByte(const std::string& t)
{
    if (t.size() != 2) return -1;
    int v = 0;
    for (char c : t) { int d = (c >= '0' && c <= '9') ? c - '0' : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : -1; if (d < 0) return -1; v = v * 16 + d; }
    return v;
}

// Columns: id,group,name,address,size,min,max,def,hw
inline ParseResult parseSysexMap(const std::string& text)
{
    ParseResult r; std::istringstream in(text); std::string line; int ln = 0; std::set<std::string> ids, addrs;
    while (std::getline(in, line))
    {
        ++ln; if (line.empty() || line[0] == '#' || line.rfind("id,", 0) == 0) continue;
        auto f = splitCsv(line); auto err = [&](const std::string& m) { r.errors.push_back("sysex line " + std::to_string(ln) + ": " + m); };
        if (f.size() != 9) { err("expected 9 columns"); continue; }
        Param p; p.kind = Kind::Sysex; p.id = f[0]; p.group = f[1]; p.name = f[2];
        std::istringstream as(f[3]); std::string tok; int n = 0; bool ok = true;
        while (as >> tok && n < 4)
        {
            if (n == 0 && tok.size() == 2 && tok[0] == '1' && tok[1] == 'p') { p.perPart = true; p.sysAddr[0] = 0x10; }
            else { int b = hexByte(tok); if (b < 0 || b > 0x7F) { ok = false; break; } p.sysAddr[n] = b; }
            ++n;
        }
        if (!ok || n != 4 || (as >> tok)) { err("bad address " + f[3]); continue; }
        int hw = 0;
        if (!toInt(f[4], p.sysSize) || !toInt(f[5], p.minV) || !toInt(f[6], p.maxV) || !toInt(f[7], p.defV) || !toInt(f[8], hw)) { err("bad number"); continue; }
        if (p.sysSize < 1 || p.sysSize > 2) { err("size must be 1 or 2"); continue; }
        const int top = p.sysSize == 1 ? 127 : 16383;
        if (p.minV < 0 || p.maxV > top || p.minV > p.maxV || p.defV < p.minV || p.defV > p.maxV) { err("bad range for " + p.id); continue; }
        p.hwVerified = hw == 1; p.rangeFromManual = true;
        if (p.id.empty() || p.name.empty()) { err("empty id or name"); continue; }
        if (!ids.insert(p.id).second) { err("duplicate id " + p.id); continue; }
        if (!addrs.insert(f[3]).second) { err("duplicate address " + f[3]); continue; }
        r.params.push_back(p);
    }
    return r;
}

// The message for one value. part is 1 to 16 and is used only when the address has the 1p byte.
inline std::optional<Bytes> encodeSysex(const Param& p, int dev, int part, int val)
{
    if (p.kind != Kind::Sysex) return std::nullopt;
    val = std::clamp(val, p.minV, p.maxV);
    Addr a{ uint8_t(p.sysAddr[0]), uint8_t(p.sysAddr[1]), uint8_t(p.sysAddr[2]), uint8_t(p.sysAddr[3]) };
    if (p.perPart) a[0] = uint8_t(0x10 | (std::clamp(part, 1, 16) - 1));
    Bytes d; if (p.sysSize == 1) d = { uint8_t(val) }; else d = { uint8_t(val >> 7), uint8_t(val & 127) };
    return parameterChange(dev, a, d);
}

// Finds the parameter an incoming parameter change belongs to. Part addresses only match the selected part.
inline std::optional<Event> decodeSysex(const std::vector<Param>& map, const Parsed& m, int part)
{
    if (m.type != SysexType::ParameterChange) return std::nullopt;
    for (size_t i = 0; i < map.size(); ++i)
    {
        const auto& p = map[i]; if (p.kind != Kind::Sysex) continue;
        const int a0 = p.perPart ? (0x10 | (std::clamp(part, 1, 16) - 1)) : p.sysAddr[0];
        if (m.addr[0] != a0 || m.addr[1] != p.sysAddr[1] || m.addr[2] != p.sysAddr[2] || m.addr[3] != p.sysAddr[3]) continue;
        if (int(m.data.size()) != p.sysSize) return std::nullopt;
        const int v = p.sysSize == 1 ? m.data[0] : m.data[0] * 128 + m.data[1];
        return Event{ int(i), std::clamp(v, p.minV, p.maxV) };
    }
    return std::nullopt;
}
}
