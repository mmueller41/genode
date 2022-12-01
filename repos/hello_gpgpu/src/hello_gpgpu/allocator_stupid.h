#ifndef ALLOCATOR_STUPID
#define ALLOCATOR_STUPID

#include <base/stdint.h>

namespace Genode {
	class Allocator_stupid;
}

class Genode::Allocator_stupid
{
private:
    /*
    * Noncopyable
    */
    Allocator_stupid(Allocator_stupid const &);
    Allocator_stupid &operator = (Allocator_stupid const &);

    /// @brief starting address
    Genode::addr_t m_start;

    /// @brief current allocation addr
    Genode::addr_t m_curr;

    /// @brief last address
    Genode::size_t m_end;

public:
    /**
     * @brief Construct a new Allocator_stupid object
     */
    Allocator_stupid() :
        m_start(0), m_curr(0), m_end(0) { }

    /**
     * @brief Destroy the Allocator_stupid object
     * 
     */
    ~Allocator_stupid() { }

    /**
     * @brief allocate memory
     * 
     * @param size 
     * @return void* 
     */
    void* alloc(size_t size);

    /**
     * @brief allocate aligned memory
     * 
     * @param alignment 
     * @param size 
     * @return void* 
     */
    void* alloc_aligned(Genode::uint32_t alignment, Genode::size_t size);

    /**
     * @brief free memory
     * 
     * @param addr 
     */
    void free(void* addr);

    /**
     * @brief free any unfreed memory
     * 
     */
    void reset();

    /**
     * @brief set address range to allocate from
     * 
     * @param start 
     * @param size 
     */
    void add_range(Genode::addr_t start, Genode::size_t size) { m_start = start; m_curr = start; m_end = m_start + size; };
};

void *operator new    (__SIZE_TYPE__ s, Genode::Allocator_stupid *a);
void *operator new [] (__SIZE_TYPE__ s, Genode::Allocator_stupid *a);
void *operator new    (__SIZE_TYPE__ s, Genode::Allocator_stupid &a);
void *operator new [] (__SIZE_TYPE__ s, Genode::Allocator_stupid &a);

#endif /* ALLOCATOR_STUPID */
