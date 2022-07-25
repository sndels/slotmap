#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/benchmark/catch_constructor.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators_range.hpp>

#include <slotmap.hpp>

#include <atomic>
#include <unordered_map>
#include <vector>

class Obj
{
  public:
    static const size_t DATA_BYTES = 256;

    Obj(uint8_t v) { std::memset(m_data, v, DATA_BYTES); }

  private:
    uint8_t m_data[DATA_BYTES];
};

TEST_CASE("Bench", "[bench]")
{
    // SlotMap won't reallocate during the first but will during the rest
    // The final one will also reallocate the internal page array
    const uint32_t object_count = GENERATE(512, 2048, 8096, 65536);

    SECTION("SlotMap")
    {
        WARN("Object count " << object_count);

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
            return std::make_tuple(std::move(handles), std::move(maps));
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

            return std::make_tuple(std::move(handles), std::move(maps));
        };

        BENCHMARK_ADVANCED("access uint32_t")
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

            std::vector<uint32_t> sums(meter.runs(), 0);
            meter.measure(
                [&](int i)
                {
                    for (auto j = 0u; j < object_count; ++j)
                        sums[i] += *maps[i].get(handles[i][j]);
                });

            return sums;
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
            return std::make_tuple(std::move(handles), std::move(maps));
        };

        BENCHMARK_ADVANCED("insert 256B object")
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
            return std::make_tuple(std::move(handles), std::move(maps));
        };

        BENCHMARK_ADVANCED("emplace 256B object")
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
            return std::make_tuple(std::move(handles), std::move(maps));
        };

        BENCHMARK_ADVANCED("remove 256B object")
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
            return std::make_tuple(std::move(handles), std::move(maps));
        };
    }

    SECTION("std::unordered_map")
    {
        WARN("Object count " << object_count);

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
            return std::make_tuple(std::move(handles), std::move(maps));
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

            return std::make_tuple(std::move(handles), std::move(maps));
        };

        BENCHMARK_ADVANCED("access uint32_t")
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

            std::vector<uint32_t> sums(meter.runs(), 0);
            meter.measure(
                [&](int i)
                {
                    for (auto j = 0u; j < object_count; ++j)
                        sums[i] += maps[i][handles[i][j]];
                });

            return sums;
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
            return std::make_tuple(std::move(handles), std::move(maps));
        };

        BENCHMARK_ADVANCED("insert 256B object")
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
            return std::make_tuple(std::move(handles), std::move(maps));
        };

        BENCHMARK_ADVANCED("emplace 256B object")
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
            return std::make_tuple(std::move(handles), std::move(maps));
        };

        BENCHMARK_ADVANCED("remove 256B object")
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
            return std::make_tuple(std::move(handles), std::move(maps));
        };
    }
}
