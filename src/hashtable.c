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
enum ht_stat ht_new(HashTable_s **out)
{
  HashTableConf htc;
  ht_conf_init(&htc);
  return ht_new_conf(&htc, out);
}
/*-------------------------------------------------------------*/
enum ht_stat ht_new_conf(HashTableConf const *const conf, HashTable_s **out)
{
  HashTable_s *table = conf->mem_calloc(1, sizeof(HashTable_s));
  if (!table)
    return HT_ERR_ALLOC;
  table->capacity = round_pow_two(conf->initial_capacity);
  table->buckets = conf->mem_calloc(table->capacity, sizeof(TableEntry*));
  if (!table->buckets) {
    conf->mem_free(table);
    return HT_ERR_ALLOC;
  }
  table->hash = conf->hash;
  table->key_cmp = conf->key_compare;
  table->load_factor = conf->load_factor;
  table->hash_seed = conf->hash_seed;
  table->key_len = conf->key_len;
  table->size = 0;
  table->mem_alloc = conf->mem_alloc;
  table->mem_calloc = conf->mem_calloc;
  table->mem_free = conf->mem_free;
  table->threshold = (size_t) (table->capacity * table->load_factor);
  *out = table;
  return HT_OK;
}
/*-------------------------------------------------------------*/
void ht_conf_init(HashTableConf *conf)
{
  conf->hash = string_hash;
  conf->key_compare = ht_common_cmp_str;
  conf->initial_capacity = DEFAULT_CAPACITY;
  conf->key_len = KEY_LENGTH_VARIABLE;
  conf->hash_seed = 0;
  conf->mem_alloc = malloc;
  conf->mem_calloc = calloc;
  conf->mem_free = free;
}
/*-------------------------------------------------------------*/
enum ht_stat ht_add(HashTable_s *table, void *key, void *val)
{
  enum ht_stat res;
  if (table->size >= table->threshold) {
    if ((res = resize(table, table->capacity << 1)) != HT_OK)
      return res;
  }
  if (!key)
    return HT_ERR_NULL_VAL;
  const size_t hash = table->hash(key, table->key_len, table->hash_seed);
  const size_t index = hash & (table->capacity - 1);
  TableEntry *replace = table->buckets[index];
  while (replace) {
    void *rk = replace->key;
    if (rk && table->key_cmp(rk, key) == 0) {
      replace->value = val;
      return HT_OK;
    }
    replace = replace->next;
  }
  TableEntry *new_entry = table->mem_alloc(sizeof(TableEntry));
  if (!new_entry)
    return HT_ERR_ALLOC;
  new_entry->key = key;
  new_entry->value = val;
  new_entry->hash = hash;
  new_entry->next = table->buckets[index];
  table->buckets[index] = new_entry;
  table->size++;
  return HT_OK;
}
/*-------------------------------------------------------------*/
enum ht_stat ht_get(HashTable_s *table, void *key, void **out)
{
  if (!key)
    return HT_ERR_NULL_VAL;
  size_t index = get_table_index(table, key);
  TableEntry *bucket = table->buckets[index];
  while (bucket) {
    if (bucket->key && table->key_cmp(bucket->key, key) == 0) {
      *out = bucket->value;
      return HT_OK;
    }
    bucket = bucket->next;
  }
  return HT_ERR_KEY_NOT_FOUND;
}
/*-------------------------------------------------------------*/
enum ht_stat ht_remove(HashTable_s *table, void *key, void **out)
{
  if (!key)
    return HT_ERR_NULL_VAL;
  const size_t index = get_table_index(table, key);
  TableEntry *entry = table->buckets[index];
  TableEntry *prev = NULL, *next = NULL;
  while (entry) {
    next = entry->next;
    if (entry->key && table->key_cmp(key, entry->key) == 0) {
      void *value = entry->value;
      if (!prev)
        table->buckets[index] = next;
      else
        prev->next = next;
      table->mem_free(entry);
      table->size--;
      if (out)
        *out = value;
      return HT_OK;
    }
    prev = entry;
    entry = next;
  }
  return HT_ERR_KEY_NOT_FOUND;
}
/*-------------------------------------------------------------*/
void ht_remove_all(HashTable_s *table)
{
  for (size_t i = 0; i < table->capacity; i++) {
    TableEntry *entry = table->buckets[i];
    while (entry) {
      TableEntry *next = entry->next;
      table->mem_free(entry);
      table->size--;
      entry = next;
    }
    table->buckets[i] = NULL;
  }
}
/*-------------------------------------------------------------*/
void ht_destroy(HashTable_s *table)
{
  for (size_t i = 0; i < table->capacity; i++) {
    TableEntry *next = table->buckets[i];
    while (next) {
      TableEntry *tmp = next->next;
      table->mem_free(next);
      next = tmp;
    }
  }
  table->mem_free(table->buckets);
  table->mem_free(table);
}
/*-------------------------------------------------------------*/
bool ht_contains_key(HashTable_s *table, void *key)
{
  size_t index = get_table_index(table, key);
  TableEntry *entry = table->buckets[index];
  while (entry) {
    if (table->key_cmp(key, entry->key) == 0)
      return true;
    entry = entry->next;
  }
  return false;
}
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

