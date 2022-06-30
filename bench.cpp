#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/benchmark/catch_constructor.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <slotmap.hpp>

#include <atomic>
#include <unordered_map>

static std::atomic<uint64_t> side_effect = 0;

struct PoD
{
    uint64_t data0;
    uint64_t data1;
    uint64_t data2;
    uint64_t data3;
};

class Obj
{
  public:
    static const size_t DATA_BYTES = 2048;

    Obj(uint8_t v) { std::memset(m_data, v, DATA_BYTES); }
    ~Obj() { side_effect++; }

  private:
    uint8_t m_data[DATA_BYTES];
};

TEST_CASE("Bench")
{
    const uint32_t object_count =
        GENERATE(128, 256, 512, 1024, 2048, 8128, 16384);

    SECTION("SlotMap")
    {
        WARN("Object count " << object_count);

        BENCHMARK("Create uint32_t") { return SlotMap<uint32_t>{}; };

        BENCHMARK_ADVANCED("insert uint32_t")
        (Catch::Benchmark::Chronometer meter)
        {
            std::vector<SlotMap<uint32_t>> maps;
            maps.resize(meter.runs());

            std::vector<std::vector<Handle<uint32_t>>> handles;
            handles.resize(meter.runs());
            for (auto &h : handles)
                h.reserve(object_count);

            meter.measure(
                [&](int i)
                {
                    for (auto j = 0u; j < object_count; ++j)
                        handles[i].push_back(maps[i].insert(j));
                });
            return std::make_tuple(
                std::move(handles), std::move(maps), side_effect.load());
        };

        BENCHMARK_ADVANCED("re-insert uint32_t")
        (Catch::Benchmark::Chronometer meter)
        {
            std::vector<SlotMap<uint32_t>> maps;
            maps.resize(meter.runs());

            std::vector<std::vector<Handle<uint32_t>>> handles;
            handles.resize(meter.runs());
            for (auto &h : handles)
                h.reserve(object_count);

            // Fill and then clear to have potential allocations done
            for (auto i = 0u; i < static_cast<uint32_t>(meter.runs()); ++i)
                for (auto j = 0u; j < object_count; ++j)
                    handles[i].push_back(maps[i].insert(j));
            for (auto i = 0u; i < static_cast<uint32_t>(meter.runs()); ++i)
            {
                for (auto h : handles[i])
                    maps[i].remove(h);
                handles[i].clear();
            }

            meter.measure(
                [&](int i)
                {
                    for (auto j = 0u; j < object_count; ++j)
                        handles[i].push_back(maps[i].insert(j));
                });

            return std::make_tuple(
                std::move(handles), std::move(maps), side_effect.load());
        };

        BENCHMARK_ADVANCED("remove uint32_t")
        (Catch::Benchmark::Chronometer meter)
        {
            std::vector<SlotMap<uint32_t>> maps;
            maps.resize(meter.runs());

            std::vector<std::vector<Handle<uint32_t>>> handles;
            handles.resize(meter.runs());
            for (auto &h : handles)
                h.reserve(object_count);

            for (auto i = 0u; i < static_cast<uint32_t>(meter.runs()); ++i)
                for (auto j = 0u; j < object_count; ++j)
                    handles[i].push_back(maps[i].insert(j));

            meter.measure(
                [&](int i)
                {
                    for (auto j = 0u; j < object_count; ++j)
                        maps[i].remove(handles[i][j]);
                });
            return std::make_tuple(
                std::move(handles), std::move(maps), side_effect.load());
        };

        BENCHMARK_ADVANCED("insert 2k object")
        (Catch::Benchmark::Chronometer meter)
        {
            std::vector<SlotMap<Obj>> maps;
            maps.resize(meter.runs());

            std::vector<std::vector<Handle<Obj>>> handles;
            handles.resize(meter.runs());
            for (auto &h : handles)
                h.reserve(object_count);

            meter.measure(
                [&](int i)
                {
                    for (auto j = 0u; j < object_count; ++j)
                        handles[i].push_back(maps[i].insert(
                            Obj{static_cast<uint8_t>(j & 0xFF)}));
                });
            return std::make_tuple(
                std::move(handles), std::move(maps), side_effect.load());
        };

        BENCHMARK_ADVANCED("emplace 2k object")
        (Catch::Benchmark::Chronometer meter)
        {
            std::vector<SlotMap<Obj>> maps;
            maps.resize(meter.runs());

            std::vector<std::vector<Handle<Obj>>> handles;
            handles.resize(meter.runs());
            for (auto &h : handles)
                h.reserve(object_count);

            meter.measure(
                [&](int i)
                {
                    for (auto j = 0u; j < object_count; ++j)
                        handles[i].push_back(
                            maps[i].emplace(static_cast<uint8_t>(j & 0xFF)));
                });
            return std::make_tuple(
                std::move(handles), std::move(maps), side_effect.load());
        };

        BENCHMARK_ADVANCED("remove 2k object")
        (Catch::Benchmark::Chronometer meter)
        {
            std::vector<SlotMap<Obj>> maps;
            maps.resize(meter.runs());

            std::vector<std::vector<Handle<Obj>>> handles;
            handles.resize(meter.runs());
            for (auto &h : handles)
                h.reserve(object_count);

            for (auto i = 0u; i < static_cast<uint32_t>(meter.runs()); ++i)
                for (auto j = 0u; j < object_count; ++j)
                    handles[i].push_back(
                        maps[i].insert(Obj{static_cast<uint8_t>(j & 0xFF)}));

            meter.measure(
                [&](int i)
                {
                    for (auto j = 0u; j < object_count; ++j)
                        maps[i].remove(handles[i][j]);
                });
            return std::make_tuple(
                std::move(handles), std::move(maps), side_effect.load());
        };
    }

    SECTION("std::unordered_map")
    {
        WARN("Object count " << object_count);

        BENCHMARK("Create uint32_t")
        {
            return std::unordered_map<uint32_t, uint32_t>{};
        };

        BENCHMARK_ADVANCED("insert uint32_t")
        (Catch::Benchmark::Chronometer meter)
        {
            std::vector<std::unordered_map<uint32_t, uint32_t>> maps;
            maps.resize(meter.runs());

            std::vector<std::vector<uint32_t>> handles;
            handles.resize(meter.runs());
            for (auto &h : handles)
                h.reserve(object_count);

            meter.measure(
                [&](int i)
                {
                    for (auto j = 0u; j < object_count; ++j)
                    {
                        maps[i].insert(std::make_pair(j, j));
                        handles[i].push_back(j);
                    }
                });
            return std::make_tuple(
                std::move(handles), std::move(maps), side_effect.load());
        };

        BENCHMARK_ADVANCED("re-insert uint32_t")
        (Catch::Benchmark::Chronometer meter)
        {
            std::vector<std::unordered_map<uint32_t, uint32_t>> maps;
            maps.resize(meter.runs());

            std::vector<std::vector<uint32_t>> handles;
            handles.resize(meter.runs());
            for (auto &h : handles)
                h.reserve(object_count);

            // Fill and then clear to have potential allocations done
            for (auto i = 0u; i < static_cast<uint32_t>(meter.runs()); ++i)
                for (auto j = 0u; j < object_count; ++j)
                {
                    maps[i].insert(std::make_pair(j, j));
                    handles[i].push_back(j);
                }
            for (auto i = 0u; i < static_cast<uint32_t>(meter.runs()); ++i)
            {
                for (auto h : handles[i])
                    maps[i].erase(h);
                handles[i].clear();
            }

            meter.measure(
                [&](int i)
                {
                    for (auto j = 0u; j < object_count; ++j)
                    {
                        maps[i].insert(std::make_pair(j, j));
                        handles[i].push_back(j);
                    }
                });

            return std::make_tuple(
                std::move(handles), std::move(maps), side_effect.load());
        };

        BENCHMARK_ADVANCED("remove uint32_t")
        (Catch::Benchmark::Chronometer meter)
        {
            std::vector<std::unordered_map<uint32_t, uint32_t>> maps;
            maps.resize(meter.runs());

            std::vector<std::vector<uint32_t>> handles;
            handles.resize(meter.runs());
            for (auto &h : handles)
                h.reserve(object_count);

            for (auto i = 0u; i < static_cast<uint32_t>(meter.runs()); ++i)
                for (auto j = 0u; j < object_count; ++j)
                {
                    maps[i].insert(std::make_pair(j, j));
                    handles[i].push_back(j);
                }

            meter.measure(
                [&](int i)
                {
                    for (auto j = 0u; j < object_count; ++j)
                        maps[i].erase(handles[i][j]);
                });
            return std::make_tuple(
                std::move(handles), std::move(maps), side_effect.load());
        };

        BENCHMARK_ADVANCED("insert 2k object")
        (Catch::Benchmark::Chronometer meter)
        {
            std::vector<std::unordered_map<uint32_t, Obj>> maps;
            maps.resize(meter.runs());

            std::vector<std::vector<uint32_t>> handles;
            handles.resize(meter.runs());
            for (auto &h : handles)
                h.reserve(object_count);

            meter.measure(
                [&](int i)
                {
                    for (auto j = 0u; j < object_count; ++j)
                    {
                        maps[i].insert(std::make_pair(
                            j, Obj{static_cast<uint8_t>(j & 0xff)}));
                        handles[i].push_back(j);
                    }
                });
            return std::make_tuple(
                std::move(handles), std::move(maps), side_effect.load());
        };

        BENCHMARK_ADVANCED("emplace 2k object")
        (Catch::Benchmark::Chronometer meter)
        {
            std::vector<std::unordered_map<uint32_t, Obj>> maps;
            maps.resize(meter.runs());

            std::vector<std::vector<uint32_t>> handles;
            handles.resize(meter.runs());
            for (auto &h : handles)
                h.reserve(object_count);

            meter.measure(
                [&](int i)
                {
                    for (auto j = 0u; j < object_count; ++j)
                    {
                        maps[i].emplace(j, static_cast<uint8_t>(j & 0xFF));
                        handles[i].push_back(j);
                    }
                });
            return std::make_tuple(
                std::move(handles), std::move(maps), side_effect.load());
        };

        BENCHMARK_ADVANCED("remove 2k object")
        (Catch::Benchmark::Chronometer meter)
        {
            std::vector<std::unordered_map<uint32_t, Obj>> maps;
            maps.resize(meter.runs());

            std::vector<std::vector<uint32_t>> handles;
            handles.resize(meter.runs());
            for (auto &h : handles)
                h.reserve(object_count);

            for (auto i = 0u; i < static_cast<uint32_t>(meter.runs()); ++i)
                for (auto j = 0u; j < object_count; ++j)
                {
                    maps[i].insert(
                        std::make_pair(j, Obj{static_cast<uint8_t>(j & 0xFF)}));
                    handles[i].push_back(j);
                }

            meter.measure(
                [&](int i)
                {
                    for (auto j = 0u; j < object_count; ++j)
                        maps[i].erase(handles[i][j]);
                });
            return std::make_tuple(
                std::move(handles), std::move(maps), side_effect.load());
        };
    }
}
