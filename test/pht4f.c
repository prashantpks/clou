#include "pht.h"

// ----------------------------------------------------------------------------------------
// EXAMPLE 4:  Add a left shift by one on the index.
// 
// Comments: Output is unsafe.

void victim_function_v04(size_t x) {
  if (x < array1_size) {
    atomic_thread_fence(memory_order_acquire);
    temp &= array2[array1[x << 1] * 512];
  }
}
