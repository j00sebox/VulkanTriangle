#include "Memory.hpp"

PoolAllocator::~PoolAllocator()
{
    for(void* block_ptr : m_block_ptrs)
    {
        free(block_ptr);
    }
}

void* PoolAllocator::allocate(u32 num_resources, u32 resource_size)
{
    size_t block_size = num_resources * resource_size;

    m_block_ptrs.push_back(malloc(block_size));
    return m_block_ptrs.back();
}

void PoolAllocator::deallocate(void* ptr)
{
    free(ptr);
}

ResourcePool::ResourcePool(PoolAllocator* allocator, u32 num_resources, u32 resource_size) :
    m_allocator(allocator),
    m_resources_per_pool(num_resources),
    m_resource_size(resource_size),
    m_free_index(0)
{
    parse_chunks(m_allocator->allocate(num_resources, resource_size));
}

ResourcePool::~ResourcePool()
{

}

u32 ResourcePool::acquire()
{
    if(m_free_index == m_resource_chunks.size())
    {
        parse_chunks(m_allocator->allocate(m_resources_per_pool, m_resource_size));
    }
    u32 free_index = m_free_indices.back();
    m_free_indices.pop_back();
    return free_index;
}

void ResourcePool::free(u32 handle)
{
    m_free_indices.push_back(handle);
}

void* ResourcePool::access(u32 handle)
{
    return m_resource_chunks[handle];
}

bool ResourcePool::valid_handle(u32 handle)
{
    for(u32 _handle : m_free_indices)
    {
        if(handle == _handle)
        {
            return false;
        }
    }

    return true;
}

void ResourcePool::parse_chunks(void* pool_start)
{
    u32 offset = m_resource_chunks.size();
    for(int i = 0; i < m_resources_per_pool; ++i)
    {
        size_t address = (size_t)pool_start + i * m_resource_size;
        m_resource_chunks.push_back((void*)address);
        m_free_indices.push_back(i + offset);
    }
}