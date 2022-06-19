#ifndef SLOTMAP_HPP
#define SLOTMAP_HPP

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <vector>

#ifndef SLOTMAP_INITIAL_SIZE
#define SLOTMAP_INITIAL_SIZE 1024
#endif

#ifndef SLOTMAP_RESIZE_MULTIPLIER
#define SLOTMAP_RESIZE_MULTIPLIER 2
#endif

#ifndef SLOTMAP_MIN_AVAILABLE_HANDLES
#define SLOTMAP_MIN_AVAILABLE_HANDLES 256
#endif

template <typename T> class Handle
{
  public:
    static uint32_t const MAX_HANDLES = 0xFFFFFF;
    static uint32_t const MAX_GENERATIONS = 0xFF;

    Handle() = default;

  private:
    Handle(uint32_t index, uint32_t generation)
    : m_index{index}
    , m_generation{generation} {};

    uint32_t m_index : 24 {0};
    uint32_t m_generation : 8 {0};

    template <typename U> friend class SlotMap;
};
static_assert(sizeof(Handle<uint8_t>) == sizeof(uint32_t));

template <typename T> class SlotMap
{
  public:
    SlotMap();
    ~SlotMap();

    Handle<T> insert(T const &item);
    void remove(Handle<T> handle);

    T *get(Handle<T> handle);

  private:
    bool needNewHandles();
    void resize();

    uint32_t m_handle_count{SLOTMAP_INITIAL_SIZE};
    // Having a single allocation that's resized makes accessing elements
    // simple, but will make it impossible to free dead chunks of memory within
    // the allocation (except from the start).
    // Allocating standard sized pages would make freeing entire pages possible
    // Page allocation would also make pointers stable, though the added value
    // of stable pointers is a bit dubious since their generation might change
    // between accesses, effectively invalidating them anyway.
    T *m_data{nullptr};
    std::vector<uint32_t> m_generations;
    std::queue<uint32_t> m_freelist;
#ifndef NDEBUG
    uint32_t m_dead_indices{0};
#endif // NDEBUG
};

template <typename T> SlotMap<T>::SlotMap()
{
#ifdef WIN32
    m_data = reinterpret_cast<T *>(
        _aligned_malloc(sizeof(T) * m_handle_count, alignof(T)));
#else
    m_data = reinterpret_cast<T *>(
        std::aligned_alloc(alignof(T), sizeof(T) * m_handle_count));
#endif
    assert(m_data != nullptr);

    m_generations.reserve(m_handle_count);

    for (auto i = 0u; i < m_handle_count; ++i)
    {
        m_freelist.push(i);
        m_generations.push_back(0);
    }
}

template <typename T> SlotMap<T>::~SlotMap()
{
#ifdef WIN32
    _aligned_free(m_data);
#else
    free(m_data);
#endif
}

template <typename T> Handle<T> SlotMap<T>::insert(T const &item)
{
    if (needNewHandles())
        resize();

    auto index = m_freelist.front();
    m_freelist.pop();
    assert(m_generations[index] < Handle<T>::MAX_GENERATIONS);

    new (&m_data[index]) T{item};

    return Handle<T>(index, m_generations[index]);
}

template <typename T> void SlotMap<T>::remove(Handle<T> handle)
{
    m_data[handle.m_index].~T();

    auto gen = ++m_generations[handle.m_index];

    if (gen < Handle<T>::MAX_GENERATIONS)
        m_freelist.push(handle.m_index);
    else
        m_dead_indices++;
}

template <typename T> T *SlotMap<T>::get(Handle<T> handle)
{
    if (handle.m_generation == m_generations[handle.m_index])
        return &m_data[handle.m_index];
    else
        return nullptr;
}

template <typename T> bool SlotMap<T>::needNewHandles()
{
    // Pick a minimum number of handles in freelist to avoid burning through
    // them in worst case insert/remove patterns
    // https://twitter.com/dotstdy/status/1536629439961763842?s=20&t=IGuxyH4zxjkunDESRHAXDg
    return m_freelist.size() < SLOTMAP_MIN_AVAILABLE_HANDLES;
}

template <typename T> void SlotMap<T>::resize()
{
    auto old_handle_count = m_handle_count;
    m_handle_count *= SLOTMAP_RESIZE_MULTIPLIER;
    assert(m_handle_count <= Handle<T>::MAX_HANDLES);

#ifdef WIN32
    m_data = reinterpret_cast<T *>(
        _aligned_realloc(m_data, sizeof(T) * m_handle_count, alignof(T)));
    assert(m_data != nullptr);
#else
    auto *new_data = reinterpret_cast<T *>(
        aligned_alloc(alignof(T), sizeof(T) * m_handle_count));
    assert(new_data != nullptr);

    memcpy(new_data, m_data, sizeof(T) * old_handle_count);
    free(m_data);

    m_data = new_data;
#endif

    m_generations.reserve(m_handle_count);

    for (auto i = old_handle_count; i < m_handle_count; ++i)
    {
        m_freelist.push(i);
        m_generations.push_back(0);
    }
}

#endif // SLOTMAP_HPP
