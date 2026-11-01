// SysexCore: pure C++ (no JUCE). Yamaha MODX M system exclusive messages as printed in docs/MIDI-DATA-FORMAT.md
// (source: MODX M Data List, "MIDI Data Format"). Status of every rule: official doc only, NOT verified on the real device.
// Safety: this file builds and parses messages. It never decides to send anything; the plugin sends only on user action.
#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ctl
{
constexpr uint8_t kYamahaId = 0x43, kGroupHi = 0x7F, kGroupLo = 0x1C, kModelModx = 0x0D;

using Bytes = std::vector<uint8_t>;
using Addr = std::array<uint8_t, 4>;

inline bool is7bit(const Bytes& b) { for (auto v : b) if (v > 0x7F) return false; return true; }

// F0 43 1n 7F 1C 0D a1 a2 a3 a4 dd... F7
inline std::optional<Bytes> parameterChange(int dev, const Addr& a, const Bytes& data)
{
    if (dev < 0 || dev > 15 || data.empty() || !is7bit(data) || !is7bit(Bytes(a.begin(), a.end()))) return std::nullopt;
    Bytes m{ 0xF0, kYamahaId, uint8_t(0x10 | dev), kGroupHi, kGroupLo, kModelModx, a[0], a[1], a[2], a[3] };
    m.insert(m.end(), data.begin(), data.end()); m.push_back(0xF7); return m;
}

// F0 43 3n 7F 1C 0D a1 a2 a3 a4 F7
inline std::optional<Bytes> parameterRequest(int dev, const Addr& a)
{
    if (dev < 0 || dev > 15 || !is7bit(Bytes(a.begin(), a.end()))) return std::nullopt;
    return Bytes{ 0xF0, kYamahaId, uint8_t(0x30 | dev), kGroupHi, kGroupLo, kModelModx, a[0], a[1], a[2], a[3], 0xF7 };
}

// F0 43 2n 7F 1C 0D a1 a2 a3 a4 F7   (a whole Performance: request its Bulk Header address, 04 .. .. ..)
inline std::optional<Bytes> bulkDumpRequest(int dev, const Addr& a)
{
    auto m = parameterRequest(dev, a); if (m) (*m)[2] = uint8_t(0x20 | dev); return m;
}

// F0 7E 0n 06 01 F7
inline Bytes identityRequest(int dev) { return { 0xF0, 0x7E, uint8_t(dev & 15), 0x06, 0x01, 0xF7 }; }

// Checksum as printed in the Data List: low 7 bits of (model ID + address + data + checksum) equal 0.
// The doc does not say whether the byte count is included; this is UNVERIFIED and is used only to check received dumps.
inline uint8_t checksum(const Addr& a, const Bytes& data)
{
    unsigned s = kModelModx; for (auto v : a) s += v; for (auto v : data) s += v;
    return uint8_t((128 - (s & 127)) & 127);
}

enum class SysexType { ParameterChange, BulkDump, BulkRequest, ParameterRequest, IdentityReply, Other };

struct Parsed
{
    SysexType type = SysexType::Other; int dev = 0; Addr addr{}; Bytes data;
    bool checksumOk = false;          // bulk dump only
    std::string model;                // identity reply only: "MODX M6", "MODX M7", "MODX M8" or ""
};

// Never throws, never reads out of range; anything unexpected becomes SysexType::Other.
inline Parsed parse(const Bytes& m)
{
    Parsed p; const size_t n = m.size();
    if (n < 6 || m.front() != 0xF0 || m.back() != 0xF7) return p;
    for (size_t i = 1; i + 1 < n; ++i) if (m[i] > 0x7F) return p;
    if (m[1] == 0x7E && n == 15 && m[3] == 0x06 && m[4] == 0x02 && m[5] == kYamahaId)
    {
        p.type = SysexType::IdentityReply; p.dev = m[2] & 15;
        // device family bytes (printed): M6 6E 06, M7 6F 06, M8 70 06
        if (m[8] == 0x06 && m[9] == 0x6E) p.model = "MODX M6"; else if (m[8] == 0x06 && m[9] == 0x6F) p.model = "MODX M7"; else if (m[8] == 0x06 && m[9] == 0x70) p.model = "MODX M8";
        return p;
    }
    if (m[1] != kYamahaId || n < 11 || m[3] != kGroupHi || m[4] != kGroupLo) return p;
    const int kind = m[2] >> 4; p.dev = m[2] & 15;
    auto readAddr = [&](size_t at) { for (size_t i = 0; i < 4; ++i) p.addr[i] = m[at + i]; };
    // Data List (3-5-4): a bulk dump is F0 43 0n 7F 1C bc bc 0D a1..a4 data cs F7 (byte count BEFORE the model id).
    // Every other message has the model id right after the group id.
    if (kind == 0)
    {
        if (n < 14 || m[7] != kModelModx) return p;
        const size_t dataLen = n - 14;                               // 12 header bytes (through address), then data, checksum, F7
        p.type = SysexType::BulkDump; readAddr(8);
        p.data.assign(m.begin() + 12, m.begin() + 12 + long(dataLen));
        // the two byte count (m[5], m[6]) is not interpreted: the doc does not say exactly what it counts
        p.checksumOk = m[n - 2] == checksum(p.addr, p.data);
        return p;
    }
    if (m[5] != kModelModx) return p;
    if (kind == 1 && n >= 12) { p.type = SysexType::ParameterChange; readAddr(6); p.data.assign(m.begin() + 10, m.end() - 1); }
    else if (kind == 3 && n == 11) { p.type = SysexType::ParameterRequest; readAddr(6); }
    else if (kind == 2 && n == 11) { p.type = SysexType::BulkRequest; readAddr(6); }
    return p;
}
}
