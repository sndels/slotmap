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

TEST_CASE("Emplace ctor")
{
    class Struct
    {
      public:
        Struct(uint32_t data0, uint32_t data1)
        : data0{data0 + 1}
        , data1{data1 + 2} {};

        Struct(Struct const &) = delete;

        uint32_t data0;
        uint32_t data1;
    };

    SlotMap<Struct> map;

    auto h0 = map.emplace(0xDEADCAFE, 0xC0FFEEEE);
    REQUIRE(map.get(h0)->data0 == 0xDEADCAFE + 1);
    REQUIRE(map.get(h0)->data1 == 0xC0FFEEEE + 2);
}

TEST_CASE("Stale handle")
{
    SlotMap<uint32_t> map;

    auto h = map.insert(0xDEADCAFE);
    map.remove(h);

    // Burn through all generations for all handles in the initial allocation
    // with what seems like a safe margin.
    // This assumes that new handles aren't allocated until less than
    // SLOTMAP_MIN_AVAILABLE_HANDLES are available, and that removed handles are
    // reused as FIFO.
    for (auto i = 0u; i < (h.MAX_GENERATIONS + 1) * SLOTMAP_INITIAL_SIZE; ++i)
    {
        auto nh = map.insert(0xC0FFEEEE + i);
        REQUIRE(*map.get(nh) == 0xC0FFEEEE + i);
        REQUIRE(map.get(h) == nullptr);
        map.remove(nh);
    }
    REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE * SLOTMAP_RESIZE_MULTIPLIER);
    REQUIRE(map.get(h) == nullptr);
}

TEST_CASE("Size methods")
{
    SlotMap<uint32_t> map;
    REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE);
    REQUIRE(map.validCount() == 0);

    auto h = map.insert(0);
    REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE);
    REQUIRE(map.validCount() == 1);

    map.remove(h);
    REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE);
    REQUIRE(map.validCount() == 0);
}

TEST_CASE("Dead handle size methods")
{
    SlotMap<uint32_t> map;

    auto h = map.insert(0xDEADCAFE);
    map.remove(h);

    // Burn through all generations for all handles in the initial allocation
    for (auto i = 0u; i < h.MAX_GENERATIONS * SLOTMAP_INITIAL_SIZE; ++i)
        map.remove(map.insert(0));
    REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE * SLOTMAP_RESIZE_MULTIPLIER);
    REQUIRE(map.validCount() == 0);

    for (auto i = 0u; i < SLOTMAP_MIN_AVAILABLE_HANDLES; ++i)
        map.insert(0);
    REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE * SLOTMAP_RESIZE_MULTIPLIER);
    REQUIRE(map.validCount() == SLOTMAP_MIN_AVAILABLE_HANDLES);
}

TEST_CASE("Reallocation behavior")
{
    SlotMap<uint32_t> map;

    for (auto i = 0u; i <= SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES;
         ++i)
        map.insert(0);
    REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE);
    REQUIRE(
        map.validCount() ==
        SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES + 1);

    map.insert(0);
    REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE * SLOTMAP_RESIZE_MULTIPLIER);
    REQUIRE(
        map.validCount() ==
        SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES + 2);
}
