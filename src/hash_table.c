#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "hash_table.h"
#include "log.h"
#include "prime.h"

#define HT_INITIAL_BASE_SIZE 512

#define HT_PRIME_1 3329
#define HT_PRIME_2 2789

ht_item HT_DELETED_ITEM = {NULL, NULL};

static int ht_hash(const char* s, const int a, const int m)
{
    long hash = 0;
    const int len_s = strlen(s);

    for (int i = 0; i < len_s; i++) {
        hash += (long)pow(a, len_s - (i+1)) * s[i];
        hash = hash % m;
    }

    return (int)hash;
}

static int ht_get_hash(const char* s, const int num_buckets, const int attempt)
{
    const int hash_a = ht_hash(s, HT_PRIME_1, num_buckets);
    const int hash_b = ht_hash(s, HT_PRIME_2, num_buckets);

    return (hash_a + (attempt * (hash_b + 1))) % num_buckets;
}

hash_table * ht_new_sized(const int base_size) {
    hash_table * ht = calloc(1, sizeof(hash_table));
    ht->base_size = base_size;

    ht->size = next_prime(ht->base_size);

    ht->count = 0;
    ht->items = calloc((size_t)ht->size, sizeof(ht_item*));
    return ht;
}

hash_table * ht_new() {
    return ht_new_sized(HT_INITIAL_BASE_SIZE);
}

static ht_item * ht_new_item(const char * k, const char * v, size_t len)
{
    ht_item * i = calloc(1, sizeof(ht_item));
    i->key = strdup(k);
    i->value = malloc(len);
    i->size = len;
    memcpy(i->value, v, len);

    return i;
}

static void ht_del_item(ht_item * i)
{
    free(i->key);
    free(i->value);
    free(i);
}

static void ht_resize(hash_table * ht, const int base_size) {
    if (base_size < HT_INITIAL_BASE_SIZE) {
        return;
    }
    hash_table * new_ht = ht_new_sized(base_size);

    for (int i = 0; i < ht->size; i++) {
        ht_item* item = ht->items[i];
        if (item != NULL && item != &HT_DELETED_ITEM) {
            ht_insert(new_ht, item->key, item->value, item->size);
        }
    }

    ht->base_size = new_ht->base_size;
    ht->count = new_ht->count;

    // To delete new_ht, we give it ht's size and items 
    const int tmp_size = ht->size;
    ht->size = new_ht->size;
    new_ht->size = tmp_size;

    ht_item** tmp_items = ht->items;
    ht->items = new_ht->items;
    new_ht->items = tmp_items;

    ht_del_hash_table(new_ht);
}

static void ht_resize_up(hash_table * ht) {
    const int new_size = ht->base_size * 2;
    ht_resize(ht, new_size);
}


static void ht_resize_down(hash_table * ht) {
    const int new_size = ht->base_size / 2;
    ht_resize(ht, new_size);
}

void ht_del_hash_table(hash_table * ht)
{
    for (int i = 0; i < ht->size; i++) {
        ht_item* item = ht->items[i];
        if (item != NULL && item != &HT_DELETED_ITEM) {
            ht_del_item(item);
        }
    }

    free(ht->items);
    free(ht);
}

void ht_delete(hash_table * ht, const char * key) {
    const int load = ht->count * 100 / ht->size;
    
    if (load < 10) {
        ht_resize_down(ht);
    }

    int index = ht_get_hash(key, ht->size, 0);
    ht_item * item = ht->items[index];
    int i = 1;

    while (item != NULL) {
        if (item != &HT_DELETED_ITEM) {
            if (strcmp(item->key, key) == 0) {
                ht_del_item(item);
                ht->items[index] = &HT_DELETED_ITEM;
                ht->count--;
                break;
            }
        }

        index = ht_get_hash(key, ht->size, i);

        if (index > -1 && index < ht->size)
            item = ht->items[index];
        i++;
    }
}

void ht_insert(hash_table * ht, const char * key, const char * value, size_t len) {
    const int load = ht->count * 100 / ht->size;
    
    if (load > 70) {
        ht_resize_up(ht);
    }

    ht_item * item = ht_new_item(key, value, len);
    int index = ht_get_hash(item->key, ht->size, 0);
    ht_item * cur_item = ht->items[index];
    int i = 1;

    while (cur_item != NULL) {
        if (cur_item != &HT_DELETED_ITEM)
        {
            if (strcmp(cur_item->key, key) == 0) {
                ht_del_item(cur_item);
                ht->items[index] = item;
                return;
            }
        }

        index = ht_get_hash(item->key, ht->size, i);

        if (index > -1 && index < ht->size)
            cur_item = ht->items[index];
        i++;
    } 

    ht->items[index] = item;
    ht->count++;
}

char* ht_search(hash_table * ht, const char * key) {
    int index = ht_get_hash(key, ht->size, 0);
    ht_item * item = ht->items[index];
    int i = 1;

    while (item != NULL) {
        if (item != &HT_DELETED_ITEM) { 
            if (strcmp(item->key, key) == 0) {
                return item->value;
            }
        }

        index = ht_get_hash(key, ht->size, i);
        if (index > -1 && index < ht->size)
            item = ht->items[index];
        i++;
    }

    return NULL;
}

void ht_sort(hash_table * ht, int (*cmpfcn)(const void *, const void *)) {
    qsort(ht->items, ht->size, sizeof(ht_item *), cmpfcn);

    hash_table * new_ht = ht_new_sized(ht->base_size);

    for (int i = 0; i < ht->size; i++) {
        ht_item* item = ht->items[i];
        if (item != NULL && item != &HT_DELETED_ITEM) {
            log_debug("insert after sort: %s, %p, %d",  item->key, item->value, item->size);
            ht_insert(new_ht, item->key, item->value, item->size);
        }
    }

    ht->base_size = new_ht->base_size;
    ht->count = new_ht->count;

    // To delete new_ht, we give it ht's size and items 
    const int tmp_size = ht->size;
    ht->size = new_ht->size;
    new_ht->size = tmp_size;

    ht_item** tmp_items = ht->items;
    ht->items = new_ht->items;
    new_ht->items = tmp_items;

    ht_del_hash_table(new_ht);
}