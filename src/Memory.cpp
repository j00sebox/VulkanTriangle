#include "Memory.hpp"

PoolAllocator::~PoolAllocator()
{
    if(m_free_chunks.size() != m_chunks_per_block * m_block_ptrs.size())
    {
        std::cout << "WARNING: Pool Allocator is being deleted and not all chunks allocated from it have been deallocated.\n";
    }

    for(void* block_ptr : m_block_ptrs)
    {
        free(block_ptr);
    }
}

void* PoolAllocator::allocate(size_t size)
{
    // no chunks left allocate new block
    if(m_free_chunks.empty())
    {
        allocate_block(size);
    }

    Chunk* free_chunk = m_free_chunks.back();
    m_free_chunks.pop_back();

    return free_chunk;
}

void PoolAllocator::deallocate(void* ptr)
{
    Chunk* chunk = static_cast<Chunk*>(ptr);
    m_free_chunks.push_back(chunk);
}

void PoolAllocator::allocate_block(size_t chunk_size)
{
    size_t block_size = m_chunks_per_block * chunk_size;

    m_block_ptrs.push_back(malloc(block_size));
    Chunk* block_begin = static_cast<Chunk*>(m_block_ptrs.back());

    // need to chain all the chunks together
    Chunk* chunk = block_begin;
    for(int i = 0; i < m_chunks_per_block; ++i)
    {
        size_t address = (size_t)block_begin + i * chunk_size;
        m_free_chunks.push_back((Chunk*)address);
    }
}