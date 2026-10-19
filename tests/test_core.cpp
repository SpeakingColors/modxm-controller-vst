#include "../src/MidiCore.h"
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
int main(int argc, char** argv)
{
    using namespace ctl;
    // CC encoding
    auto m = cc(1, 74, 100); CHECK(m.status == 0xB0 && m.d1 == 74 && m.d2 == 100);
    m = cc(16, 1, 300); CHECK(m.status == 0xBF && m.d2 == 127);
    m = cc(0, 1, -5); CHECK(m.status == 0xB0 && m.d2 == 0);
    // NRPN encoding
    auto n = nrpn(3, 0, 14, 9); CHECK(n.size() == 3 && n[0].d1 == 99 && n[0].d2 == 0 && n[1].d1 == 98 && n[1].d2 == 14 && n[2].d1 == 6 && n[2].d2 == 9 && n[0].status == 0xB2);
    // parsing
    const char* csv = "id,group,name,kind,a,b,min,max,def,range_src,hw\n"
        "cc_5,Voice,Glide Time,cc,5,0,0,127,0,manual,0\n"
        "nrpn_0_2,Voice,Voice Mode,nrpn,0,2,0,4,3,manual,0\n"
        "cc14_14_46,Osc,Coarse,pair,14,46,0,127,0,assumed,0\n";
    auto r = parseMap(csv); CHECK(r.errors.empty()); CHECK(r.params.size() == 3);
    CHECK(r.params[1].kind == Kind::NRPN && r.params[1].maxV == 4);
    CHECK(encode(r.params[2], 1, 5).empty());                 // pairs are not sent
    CHECK(encode(r.params[1], 1, 99).back().d2 == 4);         // clamped to the range
    // bad input is rejected, not crashed on
    CHECK(!parseMap("a,b\nx").errors.empty());
    CHECK(!parseMap("cc_1,g,n,cc,200,0,0,127,0,manual,0\n").errors.empty());
    CHECK(!parseMap("cc_1,g,n,cc,1,0,0,127,0,manual,0\ncc_2,g,n2,cc,1,0,0,127,0,manual,0\n").errors.empty());   // same address
    CHECK(!parseMap("cc_1,g,n,cc,1,0,5,3,0,manual,0\n").errors.empty());
    CHECK(!parseMap("cc_1,g,n,wat,1,0,0,127,0,manual,0\n").errors.empty());
    // decoding
    Decoder d(r.params);
    auto e = d.feedCC(5, 64); CHECK(e && e->paramIndex == 0 && e->value == 64);
    CHECK(!d.feedCC(99, 0)); CHECK(!d.feedCC(98, 2)); e = d.feedCC(6, 2); CHECK(e && e->paramIndex == 1 && e->value == 2);
    { auto e2 = d.feedCC(6, 1); CHECK(e2 && e2->paramIndex == 1 && e2->value == 1); }   // standard: a second data entry hits the same NRPN
    { Decoder fresh(r.params); CHECK(!fresh.feedCC(6, 1)); }                            // data entry with no NRPN selected is ignored
    CHECK(!d.feedCC(77, 1));                                  // unknown CC
    // round trip
    for (const auto& p : r.params) if (p.kind != Kind::Pair)
    {
        Decoder dd(r.params); std::optional<Event> last; for (auto& msg : encode(p, 1, p.maxV)) last = dd.feedCC(msg.d1, msg.d2);
        CHECK(last && r.params[size_t(last->paramIndex)].id == p.id && last->value == p.maxV);
    }
    // the real map of this repo
    if (argc > 1)
    {
        std::ifstream f(argv[1]); std::stringstream ss; ss << f.rdbuf(); auto real = parseMap(ss.str());
        for (auto& er : real.errors) std::printf("map error: %s\n", er.c_str());
        CHECK(real.errors.empty());
        int hw = 0, usable = 0; for (auto& p : real.params) { hw += p.hwVerified; usable += p.kind != Kind::Pair; }
        std::printf("map: %zu entries, %d usable, %d verified on hardware\n", real.params.size(), usable, hw);
        for (auto& p : real.params) if (p.kind != Kind::Pair)
        { Decoder dd(real.params); std::optional<Event> last; for (auto& msg : encode(p, 1, p.defV)) last = dd.feedCC(msg.d1, msg.d2); CHECK(last && real.params[size_t(last->paramIndex)].id == p.id); }
    }
    std::printf(fails ? "%d failures\n" : "all core tests passed\n", fails);
    return fails ? 1 : 0;
}
