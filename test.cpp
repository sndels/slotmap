#include <catch2/catch_test_macros.hpp>

#include <slotmap.hpp>

#include <vector>

static const uint32_t SLOTMAP_INITIAL_SIZE = 16;
static const uint32_t SLOTMAP_MIN_AVAILABLE_HANDLES = 8;

TEST_CASE("Primitive (insert)", "[test]")
{
    SlotMap<uint32_t> map{SLOTMAP_INITIAL_SIZE, SLOTMAP_MIN_AVAILABLE_HANDLES};

    SECTION("Insert")
    {
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

    SECTION("Emplace")
    {
        // Single allocation succeeds
        auto h0 = map.emplace(0u);
        REQUIRE(*map.get(h0) == 0);

        // Second allocation succeeds and doesn't overwrite the first one
        auto hcoffee = map.emplace(0xC0FFEEEEu);
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
}

TEST_CASE("Struct", "[test]")
{
    struct Struct
    {
        uint32_t data0;
        uint32_t data1;
    };

    SlotMap<Struct> map{SLOTMAP_INITIAL_SIZE, SLOTMAP_MIN_AVAILABLE_HANDLES};

    SECTION("Insert")
    {
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

    SECTION("Emplace")
    {
        // Single allocation succeeds
        auto h0 = map.emplace(0u, 1u);
        REQUIRE(map.get(h0)->data0 == 0);
        REQUIRE(map.get(h0)->data1 == 1);

        // Second allocation succeeds and doesn't overwrite the first one
        auto hcafe = map.emplace(0xDEADCAFEu, 0xC0FFEEEEu);
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
}

TEST_CASE("Aligned struct", "[test]")
{
    struct alignas(16) Struct
    {
        uint32_t data0;
        uint32_t data1;
        uint32_t _pad0;
        uint32_t _pad1;
    };
    REQUIRE(alignof(Struct) == 16);

    SlotMap<Struct> map{SLOTMAP_INITIAL_SIZE, SLOTMAP_MIN_AVAILABLE_HANDLES};

    SECTION("Insert")
    {
        // Single allocation succeeds
        auto h0 = map.insert(Struct{
            .data0 = 0,
            .data1 = 1,
            ._pad0 = 0,
            ._pad1 = 0,
        });
        REQUIRE((uint64_t)map.get(h0) % 16 == 0);
        REQUIRE(map.get(h0)->data0 == 0);
        REQUIRE(map.get(h0)->data1 == 1);

        // Second allocation succeeds and doesn't overwrite the first one
        auto hcafe = map.insert(Struct{
            .data0 = 0xDEADCAFE,
            .data1 = 0xC0FFEEEE,
            ._pad0 = 0,
            ._pad1 = 0,
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

    SECTION("Emplace")
    {
        // Single allocation succeeds
        auto h0 = map.emplace(0u, 1u, 0u, 0u);
        REQUIRE((uint64_t)map.get(h0) % 16 == 0);
        REQUIRE(map.get(h0)->data0 == 0);
        REQUIRE(map.get(h0)->data1 == 1);

        // Second allocation succeeds and doesn't overwrite the first one
        auto hcafe = map.emplace(0xDEADCAFEu, 0xC0FFEEEEu, 0u, 0u);
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
}

TEST_CASE("Remove dtor", "[test]")
{
    class Struct
    {
      public:
        Struct(uint32_t *data)
        : data{data}
        {
        }

        ~Struct() { *data += 1; }

        uint32_t *data;
    };

    SlotMap<Struct> map{SLOTMAP_INITIAL_SIZE, SLOTMAP_MIN_AVAILABLE_HANDLES};

    uint32_t data = 0xDEADCAFE;

    SECTION("Insert")
    {
        auto h0 = map.insert(Struct{&data});
        REQUIRE(map.get(h0)->data == &data);
        // Insert copies and original struct dtor is called
        REQUIRE(data == 0xDEADCAFE + 1);
        map.remove(h0);
        // Slotmap should have called the dtor
        REQUIRE(data == 0xDEADCAFE + 2);
    }

    SECTION("Emplace")
    {
        auto h0 = map.emplace(&data);
        REQUIRE(map.get(h0)->data == &data);
        // Emplace shouldn't cause dtor calls
        REQUIRE(data == 0xDEADCAFE);
        map.remove(h0);
        // SlotMap should have called the dtor
        REQUIRE(data == 0xDEADCAFE + 1);
    }
}

TEST_CASE("Move ctor", "[test]")
{
    const auto capacity = SLOTMAP_INITIAL_SIZE * 2;
    const auto min_available = capacity / 2;
    SlotMap<uint32_t> map{capacity, min_available};

    auto h = map.insert(0xDEADCAFE);

    SlotMap<uint32_t> map_copy{std::move(map)};

    REQUIRE(map_copy.capacity() == capacity);
    REQUIRE(*map_copy.get(h) == 0xDEADCAFE);
    REQUIRE_NOTHROW(map_copy.remove(h));
}

TEST_CASE("Move assign", "[test]")
{
    const auto capacity = SLOTMAP_INITIAL_SIZE * 2;
    const auto min_available = capacity / 2;
    SlotMap<uint32_t> map{capacity, min_available};

    auto h = map.insert(0xDEADCAFE);

    SlotMap<uint32_t> map_copy{2, 1};
    map_copy = std::move(map);

    REQUIRE(map_copy.capacity() == capacity);
    REQUIRE(*map_copy.get(h) == 0xDEADCAFE);
    REQUIRE_NOTHROW(map_copy.remove(h));
}

TEST_CASE("Stale handle", "[test]")
{
    SlotMap<uint32_t> map{SLOTMAP_INITIAL_SIZE, SLOTMAP_MIN_AVAILABLE_HANDLES};

    SECTION("Insert")
    {
        // Get our initial handle that should not be valid for the rest of the
        // test
        auto h = map.insert(0xDEADCAFE);
        REQUIRE(*map.get(h) == 0xDEADCAFE);
        map.remove(h);
        REQUIRE(map.get(h) == nullptr);

        // Burn through all generations for all handles in the initial
        // allocation. This assumes that removed and newly allocated handles are
        // used as FIFO.
        for (auto i = 0u; i < h.MAX_GENERATIONS * SLOTMAP_INITIAL_SIZE; ++i)
        {
            auto nh = map.insert(0xC0FFEEEE + i);
            REQUIRE(*map.get(nh) == 0xC0FFEEEE + i);
            REQUIRE(map.get(h) == nullptr);
            map.remove(nh);
        }

        // Now that the first batch is burned through, use up the fresh ones. Do
        // this so that we can be reasonably sure that the implementation has
        // killed our initial handle for good.
        for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE; ++i)
        {
            map.insert(0xC0FFEEEE + i);
        }

        // Map should now have SLOTMAP_INITIAL_SIZE valid handles, and our
        // handle should still be invalid
        REQUIRE(map.validCount() == SLOTMAP_INITIAL_SIZE);
        REQUIRE(map.get(h) == nullptr);
    }

    SECTION("Emplace")
    {
        // Get our initial handle that should not be valid for the rest of the
        // test
        auto h = map.emplace(0xDEADCAFE);
        REQUIRE(*map.get(h) == 0xDEADCAFE);
        map.remove(h);
        REQUIRE(map.get(h) == nullptr);

        // Burn through all generations for all handles in the initial
        // allocation. This assumes that removed and newly allocated handles are
        // used as FIFO.
        for (auto i = 0u; i < h.MAX_GENERATIONS * SLOTMAP_INITIAL_SIZE; ++i)
        {
            auto nh = map.emplace(0xC0FFEEEE + i);
            REQUIRE(*map.get(nh) == 0xC0FFEEEE + i);
            REQUIRE(map.get(h) == nullptr);
            map.remove(nh);
        }

        // Now that the first batch is burned through, use up the fresh ones. Do
        // this so that we can be reasonably sure that the implementation has
        // killed our initial handle for good.
        for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE; ++i)
        {
            map.emplace(0xC0FFEEEE + i);
        }

        // Map should now have SLOTMAP_INITIAL_SIZE valid handles, and our
        // handle should still be invalid
        REQUIRE(map.validCount() == SLOTMAP_INITIAL_SIZE);
        REQUIRE(map.get(h) == nullptr);
    }
}

TEST_CASE("Size methods", "[test]")
{
    SlotMap<uint32_t> map{SLOTMAP_INITIAL_SIZE, SLOTMAP_MIN_AVAILABLE_HANDLES};
    REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE);
    REQUIRE(map.validCount() == 0);

    SECTION("Insert")
    {
        auto h = map.insert(0);
        REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE);
        REQUIRE(map.validCount() == 1);

        map.remove(h);
        REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE);
        REQUIRE(map.validCount() == 0);
    }

    SECTION("Emplace")
    {
        auto h = map.emplace(0u);
        REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE);
        REQUIRE(map.validCount() == 1);

        map.remove(h);
        REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE);
        REQUIRE(map.validCount() == 0);
    }
}

TEST_CASE("Dead handle size methods", "[test]")
{
    SlotMap<uint32_t> map{SLOTMAP_INITIAL_SIZE, SLOTMAP_MIN_AVAILABLE_HANDLES};

    SECTION("Insert")
    {
        auto h = map.insert(0xDEADCAFE);
        map.remove(h);

        // Burn through all generations for all handles in the initial
        // allocation.
        for (auto i = 0u; i < h.MAX_GENERATIONS * SLOTMAP_INITIAL_SIZE; ++i)
            map.remove(map.insert(0));
        // Should be left with a bigger allocation and no valid handles.
        REQUIRE(
            map.capacity() == SLOTMAP_INITIAL_SIZE * SLOTMAP_RESIZE_MULTIPLIER);
        REQUIRE(map.validCount() == 0);

        // Use up some of the new handles.
        for (auto i = 0u;
             i < SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES; ++i)
            map.insert(0);
        // Should be left with the same allocation and the correct number of
        // valid handles.
        REQUIRE(
            map.capacity() == SLOTMAP_INITIAL_SIZE * SLOTMAP_RESIZE_MULTIPLIER);
        REQUIRE(map.validCount() == SLOTMAP_MIN_AVAILABLE_HANDLES);
    }

    SECTION("Emplace")
    {
        auto h = map.emplace(0xDEADCAFEu);
        map.remove(h);

        // Burn through all generations for all handles in the initial
        // allocation.
        for (auto i = 0u; i < h.MAX_GENERATIONS * SLOTMAP_INITIAL_SIZE; ++i)
            map.remove(map.emplace(0u));
        // Should be left with a bigger allocation and no valid handles.
        REQUIRE(
            map.capacity() == SLOTMAP_INITIAL_SIZE * SLOTMAP_RESIZE_MULTIPLIER);
        REQUIRE(map.validCount() == 0);

        // Use up some of the new handles.
        for (auto i = 0u;
             i < SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES; ++i)
            map.emplace(0u);
        // Should be left with the same allocation and the correct number of
        // valid handles.
        REQUIRE(
            map.capacity() == SLOTMAP_INITIAL_SIZE * SLOTMAP_RESIZE_MULTIPLIER);
        REQUIRE(map.validCount() == SLOTMAP_MIN_AVAILABLE_HANDLES);
    }
}

TEST_CASE("Reallocation behavior", "[test]")
{
    SlotMap<uint32_t> map{SLOTMAP_INITIAL_SIZE, SLOTMAP_MIN_AVAILABLE_HANDLES};

    SECTION("Insert")
    {
        // We shouldn't hit a realloc if we have at least
        // SLOTMAP_MIN_AVAILABLE_HANDLES handles available
        for (auto i = 0u;
             i <= SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES; ++i)
            map.insert(0);
        REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE);
        REQUIRE(
            map.validCount() ==
            SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES + 1);

        // The next insertion should then allocate more
        map.insert(0);
        REQUIRE(
            map.capacity() == SLOTMAP_INITIAL_SIZE * SLOTMAP_RESIZE_MULTIPLIER);
        REQUIRE(
            map.validCount() ==
            SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES + 2);
    }

    SECTION("Emplace")
    {
        // We shouldn't hit a realloc if we have at least
        // SLOTMAP_MIN_AVAILABLE_HANDLES handles available
        for (auto i = 0u;
             i <= SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES; ++i)
            map.emplace(0u);
        REQUIRE(map.capacity() == SLOTMAP_INITIAL_SIZE);
        REQUIRE(
            map.validCount() ==
            SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES + 1);

        // The next emplacement should then allocate more
        map.emplace(0u);
        REQUIRE(
            map.capacity() == SLOTMAP_INITIAL_SIZE * SLOTMAP_RESIZE_MULTIPLIER);
        REQUIRE(
            map.validCount() ==
            SLOTMAP_INITIAL_SIZE - SLOTMAP_MIN_AVAILABLE_HANDLES + 2);
    }
}

TEST_CASE("Handle equality", "[test]")
{
    // Null handles should match
    REQUIRE(Handle<uint32_t>() == Handle<uint32_t>());

    SlotMap<uint32_t> map{SLOTMAP_INITIAL_SIZE, SLOTMAP_MIN_AVAILABLE_HANDLES};

    SECTION("Insert")
    {
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

    SECTION("Emplace")
    {
        // Valid handle shouldn't match null
        auto h0 = map.emplace(0xCAFEBABE);
        REQUIRE(h0 != Handle<uint32_t>());

        // Copy should match its source
        auto hcopy = h0;
        REQUIRE(hcopy == h0);

        // New handle shouldn't match a previous one
        auto h1 = map.emplace(0xDEADCAFE);
        REQUIRE(h1 != h0);
    }
}

TEST_CASE("FreeList push/pop simple", "[test]")
{
    FreeList list{SLOTMAP_INITIAL_SIZE};
    REQUIRE(list.empty());

    list.push(0xDEADCAFE);
    REQUIRE(!list.empty());
    REQUIRE(list.pop() == 0xDEADCAFE);
    REQUIRE(list.empty());
}

TEST_CASE("FreeList size", "[test]")
{
    FreeList list{SLOTMAP_INITIAL_SIZE};
    REQUIRE(list.empty());
    REQUIRE(list.size() == 0);

    const auto half_minus_one = SLOTMAP_INITIAL_SIZE / 2 - 1;

    // head < tail
    for (auto i = 0u; i < half_minus_one; ++i)
        list.push(0);
    REQUIRE(list.size() == half_minus_one);
    for (auto i = 0u; i < half_minus_one; ++i)
        list.push(0);
    REQUIRE(list.size() == half_minus_one * 2);

    // tail < head
    for (auto i = 0u; i < half_minus_one; ++i)
        list.pop();
    REQUIRE(list.size() == half_minus_one);
    for (auto i = 0u; i < half_minus_one; ++i)
        list.push(0);
    REQUIRE(list.size() == half_minus_one * 2);
}

TEST_CASE("FreeList push/pop tail<head", "[test]")
{
    FreeList list{SLOTMAP_INITIAL_SIZE};
    REQUIRE(list.empty());

    for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE / 2; ++i)
    {
        list.push(0xDEADCAFE);
        list.pop();
    }
    REQUIRE(list.empty());
    for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE / 2 - 1; ++i)
        list.push(0xDEADCAFE + i);
    for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE / 2 - 1; ++i)
        list.push(0xC0FFEEEE + i);
    for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE / 2 - 1; ++i)
        REQUIRE(list.pop() == 0xDEADCAFE + i);
    for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE / 2 - 1; ++i)
        REQUIRE(list.pop() == 0xC0FFEEEE + i);
    REQUIRE(list.empty());
}

TEST_CASE("FreeList resize", "[test]")
{
    FreeList list{SLOTMAP_INITIAL_SIZE};
    REQUIRE(list.empty());

    SECTION("Head 0, tail end")
    {
        for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE; ++i)
            list.push(0xDEADCAFE + i);
        for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE; ++i)
            list.push(0xC0FFEEEE + i);
        for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE; ++i)
            REQUIRE(list.pop() == 0xDEADCAFE + i);
        for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE; ++i)
            REQUIRE(list.pop() == 0xC0FFEEEE + i);
        REQUIRE(list.empty());
    }

    SECTION("Head middle, tail middle")
    {
        for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE / 2; ++i)
        {
            list.push(0xCAFEBABE + i);
            list.pop();
        }
        for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE; ++i)
            list.push(0xDEADCAFE + i);
        for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE; ++i)
            list.push(0xC0FFEEEE + i);
        for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE; ++i)
            REQUIRE(list.pop() == 0xDEADCAFE + i);
        for (auto i = 0u; i < SLOTMAP_INITIAL_SIZE; ++i)
            REQUIRE(list.pop() == 0xC0FFEEEE + i);
        REQUIRE(list.empty());
    }
}
