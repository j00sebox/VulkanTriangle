#pragma once
#include "config.hpp"

struct Chunk
{
};

class PoolAllocator
{
public:
    PoolAllocator() = default;
    ~PoolAllocator();

    void* allocate(u32 num_resources, u32 resource_size);
    void deallocate(void* ptr);

private:
    std::vector<void*> m_block_ptrs;

};

class ResourcePool
{
public:
    ResourcePool(PoolAllocator* allocator, u32 num_resources, u32 resource_size);
    ~ResourcePool();

    u32 acquire();
    void free(u32 handle);

    void* access(u32 handle);

private:
    PoolAllocator* m_allocator = nullptr;
    std::vector<void*> m_resource_chunks;
    std::list<u32> m_free_indices;
    u32 m_resources_per_pool;
    u32 m_resource_size;
    u32 m_free_index;

    void parse_chunks(void* pool_start);
};
