#include <slotmap.hpp>

#include <cassert>
#include <cstdio>

struct Blob
{
    uint32_t data[4];
};

struct NonTrivial
{
    Blob data;
};

int main()
{
    SlotMap<uint32_t> map_u32;
    auto h0 = map_u32.insert(0);
    auto hC0FFEEEE = map_u32.insert(0xC0FFEEEE);
    printf("h0 %u\n", *map_u32.get(h0));
    printf("h1 0x%X\n", *map_u32.get(hC0FFEEEE));
    map_u32.remove(h0);
    map_u32.remove(hC0FFEEEE);
    assert(map_u32.get(h0) == nullptr);
    assert(map_u32.get(hC0FFEEEE) == nullptr);

    SlotMap<NonTrivial> map_nontrivial;
    NonTrivial nt;
    nt.data.data[2] = 0xDEADCAFE;
    auto hcafe = map_nontrivial.insert(nt);
    nt.data.data[2] = 0xCAFEBABE;
    auto hbabe = map_nontrivial.insert(nt);
    printf("hcafe 0x%X\n", map_nontrivial.get(hcafe)->data.data[2]);
    printf("hcafe 0x%X\n", map_nontrivial.get(hbabe)->data.data[2]);

    return 0;
}
