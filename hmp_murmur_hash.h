//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

#ifndef HMP_MURMUR_HASH_H
#define HMP_MURMUR_HASH_H

//-----------------------------------------------------------------------------
// Platform-specific functions and macros
#include <stdint.h>
#include <stddef.h>

//-----------------------------------------------------------------------------

uint32_t murmur_hash(const void *key, size_t length);

//-----------------------------------------------------------------------------

#endif 

