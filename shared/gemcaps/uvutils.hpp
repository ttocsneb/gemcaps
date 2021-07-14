#ifndef __GEMCAPS_SHARED_UVUTILS__
#define __GEMCAPS_SHARED_UVUTILS__

#include <uv.h>

#include "gemcaps/util.hpp"

inline ReusableAllocator<uv_write_t> write_req_allocator;
inline ReusableAllocator<uv_timer_t> timer_allocator;

/**
 * Allocate a buffer
 * 
 * @return the allocated buffer
 */
uv_buf_t buffer_allocate() noexcept;
/**
 * Free an allocated buffer
 * 
 * @param buf buffer to free
 */
void buffer_deallocate(uv_buf_t buf) noexcept;


#endif