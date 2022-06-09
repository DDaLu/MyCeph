
#ifndef CITYHASH_H
#define CITYHASH_H

#include <stdlib.h>
#include <stdint.h>

struct uint128_t {
  uint64_t a;
  uint64_t b;
};

typedef struct uint128_t uint128_t;

// hash function for a byte array
uint64_t cityhash64(const uint8_t* buf, size_t len);

// hash function for a byte array, for convenience a 64-bit seed is also
// hashed into the result
uint64_t cityhash64_with_seed(const uint8_t* buf, size_t len, uint64_t seed);

// hash function for a byte array, for convenience two seeds are also
// hashed into the result
uint64_t cityhash64_with_seeds(const uint8_t* buf, size_t len, uint64_t seed0, uint64_t seed1);

// hash function for a byte array
uint128_t cityhash128(const uint8_t* s, size_t len);

// hash function for a byte array, for convenience a 128-bit seed is also
// hashed into the result
uint128_t cityhash128_with_seed(const uint8_t* s, size_t len, uint128_t seed);

// hash function for a byte array, most useful in 32-bit binaries
uint32_t cityhash32(const uint8_t* buf, size_t len);

// hash 128 input bits down to 64 bits of output
// this is intended to be a reasonably good hash function
static inline uint64_t hash_128_to_64(const uint128_t x) {

  // murmur-inspired hashing.
  const uint64_t kmul = 0x9ddfea08eb382d69; // 11376068507788127593

  uint64_t a, b;

  a = (x.a ^ x.b) * kmul;
  a ^= (a >> 47);

  b = (x.b ^ a) * kmul;
  b ^= (b >> 47);

  b *= kmul;

  return b;
}

// conditionally include declarations for versions of city that require SSE4.2
// instructions to be available
#if defined(__SSE4_2__) && defined(__x86_64)

struct uint256_t {
  uint64_t a;
  uint64_t b;
  uint64_t c;
  uint64_t d;
};

typedef struct uint256_t uint256_t;

// hash function for a byte array
uint128_t cityhash128_crc(const uint8_t* s, size_t len);

// hash function for a byte array, for convenience a 128-bit seed is also
// hashed into the result
uint128_t cityhash128_crc_with_seed(const uint8_t* s, size_t len,
                                    uint128_t seed);

// hash function for a byte array
uint256_t cityhash256_crc(const uint8_t* s, size_t len);

#endif

#endif // CITY_HASH_H