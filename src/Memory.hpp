#pragma once
#include "config.hpp"

struct Chunk
{
};

class PoolAllocator
{
public:
    PoolAllocator(size_t chunks_per_block) :
        m_chunks_per_block(chunks_per_block)
        {}
    ~PoolAllocator();

    void* allocate(size_t size);
    void deallocate(void* ptr);

private:
    size_t m_chunks_per_block;
    std::list<Chunk*> m_free_chunks;
    std::vector<void*> m_block_ptrs;

    // allocate larger pool
    void allocate_block(size_t chunk_size);
};
