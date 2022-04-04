
typedef struct hash_table_item {
    char * key;
    char * value;
} ht_item;

typedef struct hash_table {
    int base_size;
    int size;
    int count;
    ht_item ** items;
} hash_table;

void ht_insert(hash_table * ht, const char * key, const char * value);
char * ht_search(hash_table * ht, const char * key);
void ht_delete(hash_table * h, const char * key);