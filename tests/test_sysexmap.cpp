#include "../src/SysexMap.h"
#include <cstdio>
#include <fstream>
#include <sstream>
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
int main(int argc, char** argv)
{
    using namespace ctl;
    const char* csv = "id,group,name,address,size,min,max,def,hw\n"
        "s_06000206,Common,Performance Volume,06 00 02 06,2,0,127,127,0\n"
        "s_06000108,Common,Master Effect Switch,06 00 01 08,1,0,1,0,0\n"
        "s_1p000102,Part,Part Switch,1p 00 01 00,1,0,1,1,0\n"
        "s_tempo,Common,Tempo,06 00 02 1E,2,5,300,120,0\n";
    auto r = parseSysexMap(csv); CHECK(r.errors.empty()); CHECK(r.params.size() == 4);
    // Performance Volume 100 -> two data bytes 00 64, device number 0
    auto m = encodeSysex(r.params[0], 0, 1, 100);
    CHECK(m && *m == Bytes({ 0xF0, 0x43, 0x10, 0x7F, 0x1C, 0x0D, 0x06, 0x00, 0x02, 0x06, 0x00, 0x64, 0xF7 }));
    // Tempo 300 = 2*128 + 44 (the Data List prints 02 2C as the maximum)
    m = encodeSysex(r.params[3], 0, 1, 300); CHECK(m && (*m)[10] == 0x02 && (*m)[11] == 0x2C);
    m = encodeSysex(r.params[3], 0, 1, 999); CHECK(m && (*m)[10] == 0x02 && (*m)[11] == 0x2C);       // clamped
    // part address: part 3 -> first byte 0x12
    m = encodeSysex(r.params[2], 5, 3, 1); CHECK(m && (*m)[2] == 0x15 && (*m)[6] == 0x12 && (*m)[10] == 0x01);
    // round trip through the parser and decoder
    for (size_t i = 0; i < r.params.size(); ++i)
    {
        auto msg = encodeSysex(r.params[i], 2, 7, r.params[i].maxV); CHECK(msg);
        auto ev = decodeSysex(r.params, parse(*msg), 7); CHECK(ev && ev->paramIndex == int(i) && ev->value == r.params[i].maxV);
    }
    // a part parameter for another part is ignored
    { auto msg = encodeSysex(r.params[2], 0, 4, 1); CHECK(!decodeSysex(r.params, parse(*msg), 7)); }
    // wrong data length, non parameter change, unknown address
    { Bytes bad{ 0xF0, 0x43, 0x10, 0x7F, 0x1C, 0x0D, 0x06, 0x00, 0x02, 0x06, 0x01, 0xF7 }; (void)bad; }
    { Bytes one{ 0xF0, 0x43, 0x10, 0x7F, 0x1C, 0x0D, 0x06, 0x00, 0x02, 0x06, 0x40, 0xF7 }; CHECK(!decodeSysex(r.params, parse(one), 1)); }
    { auto rq = parameterRequest(0, { 6, 0, 2, 6 }); CHECK(!decodeSysex(r.params, parse(*rq), 1)); }
    { auto msg = parameterChange(0, { 0x06, 0x00, 0x7F, 0x00 }, { 1 }); CHECK(!decodeSysex(r.params, parse(*msg), 1)); }
    // bad maps are refused
    CHECK(!parseSysexMap("a,b,c,06 00 02 06,2,0,127,0\n").errors.empty());
    CHECK(!parseSysexMap("s,G,N,06 00 02,2,0,127,0,0\n").errors.empty());           // 3 address bytes
    CHECK(!parseSysexMap("s,G,N,06 00 02 80,2,0,127,0,0\n").errors.empty());       // byte above 7F
    CHECK(!parseSysexMap("s,G,N,06 00 02 06,3,0,127,0,0\n").errors.empty());       // size 3
    CHECK(!parseSysexMap("s,G,N,06 00 02 06,2,0,20000,0,0\n").errors.empty());     // out of 14 bit
    CHECK(!parseSysexMap("s,G,N,06 00 02 06,1,0,200,0,0\n").errors.empty());       // out of 7 bit
    CHECK(!parseSysexMap("s,G,N,06 00 02 06,2,0,127,200,0\n").errors.empty());     // default out of range
    CHECK(!parseSysexMap("s,G,N,06 00 02 06,2,0,127,0,0\ns2,G,N2,06 00 02 06,2,0,127,0,0\n").errors.empty());   // same address
    if (argc > 1)
    {
        std::ifstream f(argv[1]); std::stringstream ss; ss << f.rdbuf(); auto real = parseSysexMap(ss.str());
        for (auto& e : real.errors) std::printf("map error: %s\n", e.c_str());
        CHECK(real.errors.empty()); CHECK(real.params.size() > 100);
        for (size_t i = 0; i < real.params.size(); ++i)
        {
            auto msg = encodeSysex(real.params[i], 0, 1, real.params[i].defV); CHECK(msg);
            auto ev = decodeSysex(real.params, parse(*msg), 1); CHECK(ev && ev->paramIndex == int(i) && ev->value == real.params[i].defV);
        }
        std::printf("sysex map: %zu entries, all round trip\n", real.params.size());
    }
    std::printf(fails ? "%d failures\n" : "all sysex map tests passed\n", fails);
    return fails ? 1 : 0;
}
