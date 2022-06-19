#include <catch2/catch_test_macros.hpp>

#define SLOTMAP_INITIAL_SIZE 16
#define SLOTMAP_MIN_AVAILABLE_HANDLES 8
#include <slotmap.hpp>

TEST_CASE("Primitive")
{
    SlotMap<uint32_t> map;

    auto h0 = map.insert(0);
    REQUIRE(*map.get(h0) == 0);

    auto hcoffee = map.insert(0xC0FFEEEE);
    REQUIRE(*map.get(h0) == 0);
    REQUIRE(*map.get(hcoffee) == 0xC0FFEEEE);

    map.remove(h0);
    REQUIRE(map.get(h0) == nullptr);
    REQUIRE(map.get(hcoffee) != nullptr);

    map.remove(hcoffee);
    REQUIRE(map.get(h0) == nullptr);
    REQUIRE(map.get(hcoffee) == nullptr);
}

TEST_CASE("Struct")
{
    struct Struct
    {
        uint32_t data0;
        uint32_t data1;
    };

    SlotMap<Struct> map;

    auto h0 = map.insert(Struct{
        .data0 = 0,
        .data1 = 1,
    });
    REQUIRE(map.get(h0)->data0 == 0);
    REQUIRE(map.get(h0)->data1 == 1);

    auto hcafe = map.insert(Struct{
        .data0 = 0xDEADCAFE,
        .data1 = 0xC0FFEEEE,
    });
    REQUIRE(map.get(h0)->data0 == 0);
    REQUIRE(map.get(h0)->data1 == 1);
    REQUIRE(map.get(hcafe)->data0 == 0xDEADCAFE);
    REQUIRE(map.get(hcafe)->data1 == 0xC0FFEEEE);

    map.remove(h0);
    REQUIRE(map.get(h0) == nullptr);
    REQUIRE(map.get(hcafe) != nullptr);
    map.remove(hcafe);
    REQUIRE(map.get(h0) == nullptr);
    REQUIRE(map.get(hcafe) == nullptr);
}

TEST_CASE("Aligned struct")
{
    struct alignas(16) Struct
    {
        uint32_t data0;
        uint32_t data1;
        uint32_t _pad[2];
    };
    REQUIRE(alignof(Struct) == 16);

    SlotMap<Struct> map;

    auto h0 = map.insert(Struct{
        .data0 = 0,
        .data1 = 1,
        ._pad = {0},
    });
    REQUIRE((uint64_t)map.get(h0) % 16 == 0);
    REQUIRE(map.get(h0)->data0 == 0);
    REQUIRE(map.get(h0)->data1 == 1);

    auto hcafe = map.insert(Struct{
        .data0 = 0xDEADCAFE,
        .data1 = 0xC0FFEEEE,
        ._pad = {0},
    });
    REQUIRE((uint64_t)map.get(hcafe) % 16 == 0);
    REQUIRE(map.get(h0)->data0 == 0);
    REQUIRE(map.get(h0)->data1 == 1);
    REQUIRE(map.get(hcafe)->data0 == 0xDEADCAFE);
    REQUIRE(map.get(hcafe)->data1 == 0xC0FFEEEE);

    map.remove(h0);
    REQUIRE(map.get(h0) == nullptr);
    REQUIRE(map.get(hcafe) != nullptr);
    map.remove(hcafe);
    REQUIRE(map.get(h0) == nullptr);
    REQUIRE(map.get(hcafe) == nullptr);
}
