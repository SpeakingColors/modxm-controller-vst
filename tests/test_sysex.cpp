#include "../src/SysexCore.h"
#include <cstdio>
static int fails = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++fails; } } while (0)
int main()
{
    using namespace ctl;
    auto pc = parameterChange(2, { 0x06, 0x00, 0x01, 0x05 }, { 0x40 });
    CHECK(pc && *pc == Bytes({ 0xF0, 0x43, 0x12, 0x7F, 0x1C, 0x0D, 0x06, 0x00, 0x01, 0x05, 0x40, 0xF7 }));
    auto rq = parameterRequest(0, { 0x06, 0x00, 0x01, 0x00 });
    CHECK(rq && *rq == Bytes({ 0xF0, 0x43, 0x30, 0x7F, 0x1C, 0x0D, 0x06, 0x00, 0x01, 0x00, 0xF7 }));
    auto bq = bulkDumpRequest(15, { 0x04, 0x00, 0x00, 0x00 });
    CHECK(bq && (*bq)[2] == 0x2F && bq->size() == 11);
    CHECK(identityRequest(0) == Bytes({ 0xF0, 0x7E, 0x00, 0x06, 0x01, 0xF7 }));
    CHECK(!parameterChange(16, { 0, 0, 0, 0 }, { 1 }));
    CHECK(!parameterChange(0, { 0, 0, 0, 0 }, {}));
    CHECK(!parameterChange(0, { 0, 0, 0, 0 }, { 0x80 }));
    CHECK(!parameterChange(0, { 0x80, 0, 0, 0 }, { 1 }));
    CHECK(!parameterRequest(-1, { 0, 0, 0, 0 }));
    auto p = parse(*pc); CHECK(p.type == SysexType::ParameterChange && p.dev == 2 && p.addr[3] == 5 && p.data == Bytes({ 0x40 }));
    p = parse(*rq); CHECK(p.type == SysexType::ParameterRequest && p.addr[2] == 1);
    p = parse(*bq); CHECK(p.type == SysexType::BulkRequest && p.dev == 15);
    Addr a{ 0x06, 0x00, 0x00, 0x00 }; Bytes d{ 'A', 'B', 'C' };
    unsigned s = kModelModx; for (auto v : a) s += v; for (auto v : d) s += v; s += checksum(a, d); CHECK((s & 127) == 0);
    Bytes dump{ 0xF0, 0x43, 0x00, 0x7F, 0x1C, 0x00, 0x08, 0x0D, 6, 0, 0, 0, 'A', 'B', 'C', checksum(a, d), 0xF7 };   // Data List 3-5-4 order
    p = parse(dump); CHECK(p.type == SysexType::BulkDump && p.data == d && p.checksumOk);
    dump[13] ^= 1; p = parse(dump); CHECK(p.type == SysexType::BulkDump && !p.checksumOk);
    Bytes id{ 0xF0, 0x7E, 0x7F, 0x06, 0x02, 0x43, 0x00, 0x41, 0x06, 0x6E, 0x01, 0x00, 0x00, 0x7F, 0xF7 };
    p = parse(id); CHECK(p.type == SysexType::IdentityReply);
    CHECK(parse({}).type == SysexType::Other); CHECK(parse({ 0xF0 }).type == SysexType::Other); CHECK(parse({ 0xF0, 0xF7 }).type == SysexType::Other);
    Bytes foreign = *pc; foreign[5] = 0x07; CHECK(parse(foreign).type == SysexType::Other);   // original MODX model id is not ours
    for (size_t cut = 0; cut < dump.size(); ++cut) { Bytes t(dump.begin(), dump.begin() + long(cut)); (void)parse(t); }
    unsigned x = 12345; for (int i = 0; i < 20000; ++i) { Bytes g; int len = int(x % 20); for (int k = 0; k < len; ++k) { x = x * 1103515245u + 12345u; g.push_back(uint8_t(x >> 16)); } if (!g.empty() && i % 2) { g.front() = 0xF0; g.back() = 0xF7; } (void)parse(g); }
    std::printf(fails ? "%d failures\n" : "all sysex tests passed\n", fails);
    return fails ? 1 : 0;
}
