#ifndef SLOTMAP_HPP
#define SLOTMAP_HPP

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <queue>

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

    bool operator==(Handle<T> other) const
    {
        return *(reinterpret_cast<uint32_t const *>(this)) ==
               *(reinterpret_cast<uint32_t const *>(&other));
    }

    bool operator!=(Handle<T> other) const { return !(*this == other); }

  private:
    Handle(uint32_t index, uint32_t generation)
    : m_index{index}
    , m_generation{generation}
    {
        assert(m_index <= MAX_HANDLES);
        assert(m_generation < MAX_GENERATIONS);
    };

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

    SlotMap(SlotMap const &) = delete;
    SlotMap &operator=(SlotMap const &) = delete;

    SlotMap(SlotMap &&other)
    : m_data{other.m_data}
    , m_generations{other.m_generations}
    , m_freelist{std::move(other.m_freelist)}
    , m_dead_indices{other.m_dead_indices}
    {
        other.m_data = nullptr;
        other.m_generations = nullptr;
    }

    SlotMap &operator=(SlotMap &&other)
    {
        if (this != &other)
        {
            m_data = other.m_data;
            m_generations = other.m_generations;
            m_freelist = std::move(other.m_freelist);
            m_dead_indices = other.m_dead_indices;

            other.m_data = nullptr;
            other.m_generations = nullptr;
        }
        return *this;
    }

    // Inserts a new item into the map, returning a handle for it.
    // Will reallocate more space if less than SLOTMAP_MIN_AVAILABLE_HANDLES are
    // available internally.
    Handle<T> insert(T const &item);
    // Constructs a new item in the map in-place, returning a handle for it.
    // Will reallocate more space if less than SLOTMAP_MIN_AVAILABLE_HANDLES are
    // available internally.
    template <typename... Args> Handle<T> emplace(Args const &...args);

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
    uint32_t *m_generations{nullptr};
    // TODO
    // Could avoid dependency to queue here pretty easily. We only need a
    // uint32_t queue with queryable size. Could maybe even use a ring buffer as
    // the size of the freelist would be at most that of the generations array.
    std::queue<uint32_t> m_freelist;
    uint32_t m_dead_indices{0};
};

template <typename T> SlotMap<T>::SlotMap()
{
    m_data = reinterpret_cast<T *>(std::malloc(sizeof(T) * m_handle_count));
    assert(m_data != nullptr);
    m_generations = reinterpret_cast<uint32_t *>(
        std::calloc(m_handle_count, sizeof(uint32_t)));
    assert(m_generations != nullptr);

    for (auto i = 0u; i < m_handle_count; ++i)
        m_freelist.push(i);
}

template <typename T> SlotMap<T>::~SlotMap()
{
    std::free(m_data);
    std::free(m_generations);
}

template <typename T> Handle<T> SlotMap<T>::insert(T const &item)
{
    if (needNewHandles())
        resize();

    auto index = m_freelist.front();
    m_freelist.pop();
    assert(index < m_handle_count);

    new (&m_data[index]) T{item};

    // Store index + 1 in handle to honor 0 being invalid
    auto h = Handle<T>(index + 1, m_generations[index]);
    assert(h.isValid());

    return h;
}

template <typename T>
template <typename... Args>
Handle<T> SlotMap<T>::emplace(Args const &...args)
{
    if (needNewHandles())
        resize();

    auto index = m_freelist.front();
    m_freelist.pop();
    assert(index < m_handle_count);

    new (&m_data[index]) T{args...};

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

    m_data =
        reinterpret_cast<T *>(std::realloc(m_data, sizeof(T) * m_handle_count));
    assert(m_data != nullptr);

    m_generations = reinterpret_cast<uint32_t *>(
        std::realloc(m_generations, sizeof(uint32_t) * m_handle_count));
    assert(m_generations != nullptr);

    std::memset(
        m_generations + old_handle_count, 0x0,
        sizeof(uint32_t) * (m_handle_count - old_handle_count));

    for (auto i = old_handle_count; i < m_handle_count; ++i)
        m_freelist.push(i);
}

#endif // SLOTMAP_HPP
