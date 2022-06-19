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

#ifndef SLOTMAP_MIN_AVAILABLE_HANDLES
#define SLOTMAP_MIN_AVAILABLE_HANDLES 256
#endif
static_assert(
    SLOTMAP_INITIAL_SIZE > SLOTMAP_MIN_AVAILABLE_HANDLES,
    "Having less initial handles than the minimum available count causes extra "
    "allocations");

#define SLOTMAP_RESIZE_MULTIPLIER 2

template <typename T> class Handle
{
  public:
    // 24 bits, 0 is not a valid index
    static uint32_t const MAX_HANDLES = 0xFFFFFF;
    // 8 bits, 0xFF marks an exhausted handle
    static uint32_t const MAX_GENERATIONS = 0xFF;

    Handle() = default;

  private:
    Handle(uint32_t index, uint32_t generation)
    : m_index{index}
    , m_generation{generation} {};

    bool isValid() const
    {
        return m_index > 0 && m_index <= MAX_HANDLES &&
               m_generation < MAX_GENERATIONS;
    }

    // The default index of 0 is not a valid handle
    // As such, index + 1 should be stored for valid ones
    uint32_t m_index : 24 {0};
    // Value of MAX_GENERATIONS marks an exhausted handle
    uint32_t m_generation : 8 {0};

    template <typename U> friend class SlotMap;
};
static_assert(sizeof(Handle<uint8_t>) == sizeof(uint32_t));

template <typename T> class SlotMap
{
  public:
    // Creates a new slotmap for T with pre-allocated memory for
    // SLOTMAP_INITIAL_SIZE items
    SlotMap();
    ~SlotMap();

    // Inserts a new item into the map, returning a handle for it.
    // Will reallocate more space if less than SLOTMAP_MIN_AVAILABLE_HANDLES are
    // available internally.
    Handle<T> insert(T const &item);

    // Removes the handle from the map, invalidating it.
    void remove(Handle<T> handle);

    // Returns a pointer to the item referenced by the handle, or nullptr if the
    // handle is invalid.
    T *get(Handle<T> handle);

    // Returns the number of allocated items
    uint32_t capacity() const;
    // Returns the number of items with valid handles
    uint32_t validCount() const;

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
    uint32_t m_dead_indices{0};
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

    m_generations.resize(m_handle_count, 0);

    for (auto i = 0u; i < m_handle_count; ++i)
        m_freelist.push(i);
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
    assert(index < m_handle_count);
    assert(m_generations[index] < Handle<T>::MAX_GENERATIONS);

    new (&m_data[index]) T{item};

    // Store index + 1 in handle to honor 0 being invalid
    auto h = Handle<T>(index + 1, m_generations[index]);
    assert(h.isValid());

    return h;
}

template <typename T> void SlotMap<T>::remove(Handle<T> handle)
{
#ifndef NDEBUG
    if (!handle.isValid() || handle.m_index > m_handle_count)
        return;
#endif // NDEBUG

    auto index = handle.m_index - 1;

    m_data[index].~T();

    auto gen = ++m_generations[index];

    if (gen < Handle<T>::MAX_GENERATIONS)
        m_freelist.push(index);
    else
        m_dead_indices++;
}

template <typename T> T *SlotMap<T>::get(Handle<T> handle)
{
#ifndef NDEBUG
    if (!handle.isValid() || handle.m_index > m_handle_count)
        return nullptr;
#endif // NDEBUG

    auto index = handle.m_index - 1;

    if (handle.m_generation == m_generations[index])
        return &m_data[index];
    else
        return nullptr;
}

template <typename T> uint32_t SlotMap<T>::capacity() const
{
    return m_handle_count;
}

template <typename T> uint32_t SlotMap<T>::validCount() const
{
    return capacity() - static_cast<uint32_t>(m_freelist.size()) -
           m_dead_indices;
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
