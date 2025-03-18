#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_POW_TWO (((size_t) 1) << 31)
#define DEFAULT_CAPACITY 16
#define DEFAULT_LOAD_FACTOR 0.75f
#define KEY_LENGTH_VARIABLE  -1

enum ht_stat {
    HT_OK                   = 0,

    HT_ERR_ALLOC            = 1,
    HT_ERR_INVALID_CAPACITY = 2,
    HT_ERR_INVALID_RANGE    = 3,
    HT_ERR_MAX_CAPACITY     = 4,
    HT_ERR_KEY_NOT_FOUND    = 6,
    HT_ERR_VALUE_NOT_FOUND  = 7,
    HT_ERR_OUT_OF_RANGE     = 8,
    HT_ITER_END             = 9,
    HT_ERR_NULL_VAL         = 10,
    HT_ERR_SAMEKEY          = 11,
};

typedef struct table_entry_s {
  void *key;
  void *value;
  size_t hash;
  struct table_entry_s *next;
} TableEntry;

typedef struct {
  size_t    capacity;
  size_t    size;
  size_t    threshold;
  u_int32_t hash_seed;
  int       key_len;
  float     load_factor;
  TableEntry **buckets;

  size_t (*hash) (const void *key, int l, uint32_t seed);
  int (*key_cmp) (const void *k1, const void *k2);
  void *(*mem_alloc) (size_t size);
  void *(*mem_calloc) (size_t blocks, size_t size);
  void (*mem_free) (void *block);
} HashTable_s;

typedef struct HashTableConf_s {
  float load_factor;
  size_t initial_capacity;
  int key_len;
  uint32_t hash_seed;
  size_t (*hash) (const void *key, int l, uint32_t seed);
  int (*key_compare) (const void *key1, const void *key2);
  void *(*mem_alloc) (size_t size);
  void *(*mem_calloc) (size_t blocks, size_t size);
  void (*mem_free) (void *block);
} HashTableConf;
