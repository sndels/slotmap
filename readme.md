# SlotMap

Inspired by [Sergey Makeev](https://github.com/SergeyMakeev/slot_map) and [Sebastian Aaltonen](https://twitter.com/SebAaltonen/status/1534416275828514817?s=20&t=7OxNvfFwh6u7YhzVbZTd3Q), I decided to build one of these from scratch for practice.

`SlotMap<T>` is a container type that gives out handles to objects of type `T`, `Handle<T>`. These handles can be used to query pointers to the objects from the map or remove them from it. The main selling point of this container is its light weight and simple interface with guaranteed O(1) insertions, removals and queries.

```C++
{
    struct Struct {
        uint32_t data0;
        uint32_t data1;
    };

    SlotMap<Struct> map;

    {
        Handle<Struct> h = map.insert(Struct{
            .data0 = 0,
            .data1 = 1,
        });
        Struct *s = map.get(h);
        map.remove(h);
    }

    {
        Handle<Struct> h = map.emplace(0, 1);
        Struct *s = map.get(h);
        map.remove(h);
    }
}
```

Removing a handle frees up the memory held by its object to be reused for new objects. This reuse is implemented with a freelist of available handles and by assigning a generation for each object. Only the current generation is valid for access to avoid stale handles mapping to the wrong objects. Once all available generations are exhausted for a given handle, it is no longer given out and use of the memory it points to is effectively lost.

Reusing handles using generations could rapidly burn through individual handles given a bad allocation pattern. To avoid the worst case of repeatedly allocating and removing the same handles, a [FIFO freelist](https://twitter.com/dotstdy/status/1536629439961763842?s=20&t=IGuxyH4zxjkunDESRHAXDg) is used with an enforced minimum size of N (256 by default). This way, at least N other handles are (re)used between two reuses of any single one. Once the freelist has less than N handles, the whole allocation is increased to get fresh handles into the mix.

A simple buffer is used internally for object storage and resizing doubles its size when more handles are needed. Using a continuous buffer makes it impossible to free memory for stale handles unless they are at the front of the held memory region. Using page based allocation would make it possible to free memory for entire pages that have gone stale anywhere within the handle range. However, I don't have a real use case to evaluate such an implementation with so it would just needlessly add to the complexity of this implementation.
