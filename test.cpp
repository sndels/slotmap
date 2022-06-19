#include <catch2/catch_test_macros.hpp>

#define SLOTMAP_INITIAL_SIZE 16
#define SLOTMAP_MIN_AVAILABLE_HANDLES 8
#include <slotmap.hpp>

TEST_CASE("Primitive")
{
    SlotMap<uint32_t> map;

    // Single allocation succeeds
    auto h0 = map.insert(0);
    REQUIRE(*map.get(h0) == 0);

    // Second allocation succeeds and doesn't overwrite the first one
    auto hcoffee = map.insert(0xC0FFEEEE);
    REQUIRE(*map.get(h0) == 0);
    REQUIRE(*map.get(hcoffee) == 0xC0FFEEEE);

    // Removes work and invalidate the correct handles
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

    // Single allocation succeeds
    auto h0 = map.insert(Struct{
        .data0 = 0,
        .data1 = 1,
    });
    REQUIRE(map.get(h0)->data0 == 0);
    REQUIRE(map.get(h0)->data1 == 1);

    // Second allocation succeeds and doesn't overwrite the first one
    auto hcafe = map.insert(Struct{
        .data0 = 0xDEADCAFE,
        .data1 = 0xC0FFEEEE,
    });
    REQUIRE(map.get(h0)->data0 == 0);
    REQUIRE(map.get(h0)->data1 == 1);
    REQUIRE(map.get(hcafe)->data0 == 0xDEADCAFE);
    REQUIRE(map.get(hcafe)->data1 == 0xC0FFEEEE);

    // Removes work and invalidate the correct handles
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

    // Single allocation succeeds
    auto h0 = map.insert(Struct{
        .data0 = 0,
        .data1 = 1,
        ._pad = {0},
    });
    REQUIRE((uint64_t)map.get(h0) % 16 == 0);
    REQUIRE(map.get(h0)->data0 == 0);
    REQUIRE(map.get(h0)->data1 == 1);

    // Second allocation succeeds and doesn't overwrite the first one
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

    // Removes work and invalidate the correct handles
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

    // Ctor is called with given arguments
    auto h0 = map.emplace(0xDEADCAFE, 0xC0FFEEEE);
    REQUIRE(map.get(h0)->data0 == 0xDEADCAFE + 1);
    REQUIRE(map.get(h0)->data1 == 0xC0FFEEEE + 2);
}

TEST_CASE("Remove dtor")
{
    static uint32_t dtor_val = 0;
    class Struct
    {
      public:
        Struct(uint32_t data0)
        : data0{data0}
        {
        }

        ~Struct() { dtor_val = data0 + 1; }

        uint32_t data0;
    };

    SlotMap<Struct> map;

    // Dtor is called
    auto h0 = map.emplace(0xDEADCAFE);
    REQUIRE(map.get(h0)->data0 == 0xDEADCAFE);
    REQUIRE(dtor_val == 0);
    map.remove(h0);
    REQUIRE(dtor_val == 0xDEADCAFE + 1);
}

TEST_CASE("Stale handle")
{
    SlotMap<uint32_t> map;

    // Get our initial handle that should not be valid for the rest of the test
    auto h = map.insert(0xDEADCAFE);
    REQUIRE(*map.get(h) == 0xDEADCAFE);
    map.remove(h);
    REQUIRE(map.get(h) == nullptr);

    // Burn through all generations for all handles in the initial allocation.
    // This assumes that removed and newly allocated handles are used as FIFO.
    for (auto i = 0u; i < h.MAX_GENERATIONS * SLOTMAP_INITIAL_SIZE; ++i)
    {
        auto nh = map.insert(0xC0FFEEEE + i);
        REQUIRE(*map.get(nh) == 0xC0FFEEEE + i);
        REQUIRE(map.get(h) == nullptr);
        map.remove(nh);
    }

    // Now that the first batch is burned through, use up the fresh ones. Do
    // this so that we can be reasonably sure that the implementation has killed
    // our initial handle for good.
    for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE; ++i)
    {
        map.insert(0xC0FFEEEE + i);
    }

    // Map should now have SLOTMAP_INITIAL_SIZE valid handles, and our handle
    // should still be invalid
    REQUIRE(map.validCount() == SLOTMAP_INITIAL_SIZE);
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

    // Burn through all generations for all handles in the initial allocation.
    for (auto i = 0u; i < h.MAX_GENERATIONS * SLOTMAP_INITIAL_SIZE; ++i)
        map.remove(map.insert(0));
    // Should be left with a bigger allocation and no valid handles.
    REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE * SLOTMAP_RESIZE_MULTIPLIER);
    REQUIRE(map.validCount() == 0);

    // Use up some of the new handles.
    for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES;
         ++i)
        map.insert(0);
    // Should be left with the same allocation and the correct number of valid
    // handles.
    REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE * SLOTMAP_RESIZE_MULTIPLIER);
    REQUIRE(map.validCount() == SLOTMAP_MIN_AVAILABLE_HANDLES);
}

TEST_CASE("Reallocation behavior")
{
    SlotMap<uint32_t> map;

    // We shouldn't hit a realloc if we have at least
    // SLOTMAP_MIN_AVAILABLE_HANDLES handles available
    for (auto i = 0u; i <= SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES;
         ++i)
        map.insert(0);
    REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE);
    REQUIRE(
        map.validCount() ==
        SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES + 1);

    // The next insertion should then allocate more
    map.insert(0);
    REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE * SLOTMAP_RESIZE_MULTIPLIER);
    REQUIRE(
        map.validCount() ==
        SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES + 2);
}

TEST_CASE("Handle equality")
{
    // Null handles should match
    REQUIRE(Handle<uint32_t>() == Handle<uint32_t>());

    SlotMap<uint32_t> map;

    // Valid handle shouldn't match null
    auto h0 = map.insert(0xCAFEBABE);
    REQUIRE(h0 != Handle<uint32_t>());

    // Copy should match its source
    auto hcopy = h0;
    REQUIRE(hcopy == h0);

    // New handle shouldn't match a previous one
    auto h1 = map.insert(0xDEADCAFE);
    REQUIRE(h1 != h0);
}
