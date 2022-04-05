#ifndef HASH_TABLE_H
#define HASH_TABLE_H

typedef struct hash_table_item {
    char * key;
    char * value;
    int size;
} ht_item;

typedef struct hash_table {
    int base_size;
    int size;
    int count;
    ht_item ** items;
} hash_table;

void ht_del_hash_table(hash_table * ht);
hash_table * ht_new();
hash_table * ht_new_sized(const int base_size);

void ht_delete(hash_table * h, const char * key);
void ht_insert(hash_table * ht, const char * key, const char * value, size_t len);
char * ht_search(hash_table * ht, const char * key);
void ht_delete(hash_table * h, const char * key);

#endif // HASH_TABLE_H