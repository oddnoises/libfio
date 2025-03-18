#include "hashtable.h"

static enum ht_stat resize(HashTable_s *table, size_t new_capacity);
static __inline void
move_entries(TableEntry **src_bucket, TableEntry **dst_bucket,
             size_t       src_size,   size_t       dst_size);
static __inline size_t
get_table_index(HashTable_s *table, void *key);
static int ht_common_cmp_str(const void *str1, const void *str2);
static __inline size_t round_pow_two(size_t n);
/*-------------------------------------------------------------*/
size_t string_hash(const void *key, int len, u_int32_t seed)
{
  const char *str = key;
  int c;
  register size_t hash = seed + 5381 + len + 1;
  while ((c = *str++))
    hash = ((hash << 5) + hash) ^ c;
  return hash;
}
/*-------------------------------------------------------------*/
static enum ht_stat resize(HashTable_s *table, size_t new_capacity)
{
  if (table->capacity >= MAX_POW_TWO)
    return HT_ERR_MAX_CAPACITY;
  TableEntry **new_buckets = table->mem_calloc(new_capacity, sizeof(TableEntry));
  if (!new_buckets)
    return HT_ERR_ALLOC;
  TableEntry **old_buckets = table->buckets;
  move_entries(old_buckets, new_buckets, table->capacity, new_capacity);
  table->buckets = new_buckets;
  table->capacity = new_capacity;
  table->threshold = (size_t) (table->load_factor * new_capacity);
  table->mem_free(old_buckets);
  return HT_OK;
}
/*-------------------------------------------------------------*/
static __inline size_t
get_table_index(HashTable_s *table, void *key)
{
  size_t hash = string_hash(key, table->key_len, table->hash_seed);
  return hash & (table->capacity - 1);
}
/*-------------------------------------------------------------*/
static __inline void
move_entries(TableEntry **src_bucket, TableEntry **dst_bucket,
             size_t       src_size,   size_t       dst_size)
{
  for (size_t i = 0; i < src_size; i++) {
    TableEntry *entry = src_bucket[i];

    while (entry) {
      TableEntry *next = entry->next;
      size_t index = entry->hash & (dst_size - 1);

      entry->next = dst_bucket[index];
      dst_bucket[index] = entry;

      entry = next;
    }
  }
}
/*-------------------------------------------------------------*/
static int ht_common_cmp_str(const void *str1, const void *str2)
{
    return strcmp((const char*) str1, (const char*) str2);
}
/*-------------------------------------------------------------*/
static __inline size_t round_pow_two(size_t n)
{
  if (n >= MAX_POW_TWO)
    return MAX_POW_TWO;
  if (!n)
    return 1;
  n--;
  n |= n >> 1;
  n |= n >> 2;
  n |= n >> 4;
  n |= n >> 8;
  n |= n >> 16;
  n++;
  return n;
}

