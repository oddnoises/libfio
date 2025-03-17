#include <cstddef>
#include <cstdint>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
  void    *key;
  void    *value;
  size_t  *hash;
} TableEntry;

typedef struct {
  size_t    capacity;
  size_t    size;
  size_t    threshold;
  uint32_t  hash_seed;
  int       key_len;
  float     load_factor;
  TableEntry **buckets;
} HashTable;
/*-------------------------------------------------------------*/
static size_t calc_hash(const void *key, int len, uint32_t seed)
{
  const char *str = key;
  register size_t hash = seed + 5381 + len + 1;
  int c;
  while ((c = *str++))
    hash = ((hash << 5) + hash) ^ c;
  return hash;
}
/*-------------------------------------------------------------*/
static int resize(HashTable *t, size_t new_capacity)
{

}
/*-------------------------------------------------------------*/
int hashtable_add(HashTable *table, void *key, void *val)
{
  int res;
  if (table->size >= table->threshold) {
    
  }
}
/*-------------------------------------------------------------*/
