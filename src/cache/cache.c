/**************************************************************************
  * C S 429 system emulator
 * 
 * cache.c - A cache simulator that can replay traces from Valgrind
 *     and output statistics such as number of hits, misses, and
 *     evictions, both dirty and clean.  The replacement policy is LRU. 
 *     The cache is a writeback cache. 
 * 
 * Copyright (c) 2021, 2023. 
 * Authors: M. Hinton, Z. Leeper.
 * All rights reserved.
 * May not be used, modified, or copied without permission.
 **************************************************************************/ 
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include "cache.h"

#define ADDRESS_LENGTH 64

/* Counters used to record cache statistics in printSummary().
   test-cache uses these numbers to verify correctness of the cache. */

//Increment when a miss occurs
int miss_count = 0;

//Increment when a hit occurs
int hit_count = 0;

//Increment when a dirty eviction occurs
int dirty_eviction_count = 0;

//Increment when a clean eviction occurs
int clean_eviction_count = 0;

/* STUDENT TO-DO: add more globals, structs, macros if necessary */
uword_t next_lru;

// log base 2 of a number.
// Useful for getting certain cache parameters
static size_t _log(size_t x) {
  size_t result = 0;
  while(x>>=1)  {
    result++;
  }
  return result;
}

/*
 * Initialize the cache according to specified arguments
 * Called by cache-runner so do not modify the function signature
 *
 * The code provided here shows you how to initialize a cache structure
 * defined above. It's not complete and feel free to modify/add code.
 */
cache_t *create_cache(int A_in, int B_in, int C_in, int d_in) {
    /* see cache-runner for the meaning of each argument */
    cache_t *cache = malloc(sizeof(cache_t));
    cache->A = A_in;
    cache->B = B_in;
    cache->C = C_in;
    cache->d = d_in;
    unsigned int S = cache->C / (cache->A * cache->B);

    cache->sets = (cache_set_t*) calloc(S, sizeof(cache_set_t));
    for (unsigned int i = 0; i < S; i++){
        cache->sets[i].lines = (cache_line_t*) calloc(cache->A, sizeof(cache_line_t));
        for (unsigned int j = 0; j < cache->A; j++){
            cache->sets[i].lines[j].valid = 0;
            cache->sets[i].lines[j].tag   = 0;
            cache->sets[i].lines[j].lru   = 0;
            cache->sets[i].lines[j].dirty = 0;
            cache->sets[i].lines[j].data  = calloc(cache->B, sizeof(byte_t));
        }
    }

    next_lru = 0;
    return cache;
}

cache_t *create_checkpoint(cache_t *cache) {
    unsigned int S = (unsigned int) cache->C / (cache->A * cache->B);
    cache_t *copy_cache = malloc(sizeof(cache_t));
    memcpy(copy_cache, cache, sizeof(cache_t));
    copy_cache->sets = (cache_set_t*) calloc(S, sizeof(cache_set_t));
    for (unsigned int i = 0; i < S; i++) {
        copy_cache->sets[i].lines = (cache_line_t*) calloc(cache->A, sizeof(cache_line_t));
        for (unsigned int j = 0; j < cache->A; j++) {
            memcpy(&copy_cache->sets[i].lines[j], &cache->sets[i].lines[j], sizeof(cache_line_t));
            copy_cache->sets[i].lines[j].data = calloc(cache->B, sizeof(byte_t));
            memcpy(copy_cache->sets[i].lines[j].data, cache->sets[i].lines[j].data, sizeof(byte_t));
        }
    }
    
    return copy_cache;
}

void display_set(cache_t *cache, unsigned int set_index) {
    unsigned int S = (unsigned int) cache->C / (cache->A * cache->B);
    if (set_index < S) {
        cache_set_t *set = &cache->sets[set_index];
        for (unsigned int i = 0; i < cache->A; i++) {
        printf ("Valid: %d Tag: %llx Lru: %lld Dirty: %d\n", set->lines[i].valid, 
                set->lines[i].tag, set->lines[i].lru, set->lines[i].dirty);
        }
    } else {
        printf ("Invalid Set %d. 0 <= Set < %d\n", set_index, S);
    }
}

/*
 * Free allocated memory. Feel free to modify it
 */
void free_cache(cache_t *cache) {
    unsigned int S = (unsigned int) cache->C / (cache->A * cache->B);
    for (unsigned int i = 0; i < S; i++){
        for (unsigned int j = 0; j < cache->A; j++) {
            free(cache->sets[i].lines[j].data);
        }
        free(cache->sets[i].lines);
    }
    free(cache->sets);
    free(cache);
}

unsigned long long big_bit(unsigned long long modder, unsigned shifter, unsigned size) {
    
    unsigned long long res = (unsigned long long) ((modder >> shifter) & ((1 << size) - 1));
    return res;
}
/* STUDENT TO-DO:
 * Get the line for address contained in the cache
 * On hit, return the cache line holding the address
 * On miss, returns NULL
 */
cache_line_t *get_line(cache_t *cache, uword_t addr) {
    /* your implementation */
    int my_A = cache->A;
    int my_B = _log(cache->B);
    int my_C = cache->C;
    next_lru++;

    int loggy = _log(my_C/(my_A*cache->B));

    unsigned long token = big_bit(addr, my_B, loggy);
    uword_t map = addr >> (my_B+loggy);

    display_set(cache, token);
    cache_set_t tower = cache->sets[token];
    cache_line_t *spot = tower.lines;
    
    for (int j = 0; j < my_A; j++) {
        cache_line_t block = spot[j];
        if (block.tag == map && block.valid == 1) {
            return (spot+j);
        }
    }
    
    return NULL;

   
}

/* STUDENT TO-DO:
 * Select the line to fill with the new cache line
 * Return the cache line selected to filled in by addr
 */
cache_line_t *select_line(cache_t *cache, uword_t addr) {
    
    next_lru++;

    int sec = _log(cache->B);
    unsigned long too = big_bit(addr, sec, _log((cache->C)/((cache->A)*cache->B)));

    cache_set_t compl= *(cache->sets + (too));
    display_set(cache, too);

    cache_line_t * give = compl.lines;

    uword_t lowru = give->lru;

    cache_line_t * low = give;

    for (int i = 0 ; i < cache->A; i++) {
        cache_line_t cur = give[i];

        if (!cur.valid) {
            int val = i;
            return (give+val);
        }

        else if (cur.lru < lowru) {
            lowru = cur.lru;
            low = (give+i);
        }
    }
    next_lru++;

    return low;
}

/*  STUDENT TO-DO:
 *  Check if the address is hit in the cache, updating hit and miss data.
 *  Return true if pos hits in the cache.
 */
bool check_hit(cache_t *cache, uword_t addr, operation_t operation) {


    next_lru++;
    

    if (get_line(cache, addr) == NULL) {

        cache_line_t * out = select_line(cache, addr);

        if (out->valid) {
            if (!(out->dirty)) {
                clean_eviction_count++;
            }
            else {
                dirty_eviction_count++; 
            }
        }
        miss_count++;
    }
    else {
        cache_line_t * prod = get_line(cache, addr);

        hit_count++;
        prod->lru = next_lru;
        if (operation != READ) {
            get_line(cache, addr)->dirty = true;
            return true;
        }
        return true;
    }
    
    return false;
}

/*  STUDENT TO-DO:
 *  Handles Misses, evicting from the cache if necessary.
 *  Fill out the evicted_line_t struct with info regarding the evicted line.
 */
evicted_line_t *handle_miss(cache_t *cache, uword_t addr, operation_t operation, byte_t *incoming_data) {

     next_lru++;

    evicted_line_t *evicted_line = malloc(sizeof(evicted_line_t));
    evicted_line->data = (byte_t *) calloc(cache->B, sizeof(byte_t));
    
    // all from other
    unsigned int OffsetLength = _log(cache -> B);
    unsigned int IndexLength = _log(cache -> C/ (cache ->B * cache -> A));
    unsigned int tagBitLength = ADDRESS_LENGTH - (IndexLength + OffsetLength);
    unsigned int index = ((addr << tagBitLength) >> (OffsetLength + tagBitLength));
    uword_t plused = addr >> ((_log(cache->B))+(_log((cache->C)/((cache->A)*cache->B))));


    cache_line_t * ret = select_line(cache, addr);
    evicted_line -> dirty = ret -> dirty;
    evicted_line -> valid = ret -> valid;
    evicted_line -> addr = (ret -> tag << (OffsetLength + IndexLength) | (index << OffsetLength));
    // till here
    int fir = 1;
    int zer = 0;

    if (incoming_data != NULL) {
        memcpy(ret->data, incoming_data, cache->B);
    }
    else if (operation == WRITE) {
        ret->dirty = fir;      
    }
    else {
        ret->dirty = zer;
    }

    ret->valid = true;
    (*ret).tag = plused;
    (*ret).lru = next_lru;

    return evicted_line;
}

/* STUDENT TO-DO:
 * Get 8 bytes from the cache and write it to dest.
 * Preconditon: addr is contained within the cache.
 */
void get_word_cache(cache_t *cache, uword_t addr, word_t *dest) {
    /* Your implementation */
    // his
    unsigned int size_of_block = cache -> B;
    cache_line_t *ret = get_line(cache, addr);
    byte_t *res = (byte_t*) dest;

    for (int i = 0; i < 8; i++) {
        unsigned int break_length = (addr + i) % size_of_block;
        res[i] = ret -> data[break_length];
    }
}

/* STUDENT TO-DO:
 * Set 8 bytes in the cache to val at pos.
 * Preconditon: addr is contained within the cache.
 */
void set_word_cache(cache_t *cache, uword_t addr, word_t val) {
    /* Your implementation */
    //his
    unsigned int size_of_block = cache -> B;
    cache_line_t *ret = get_line(cache, addr);
    byte_t *val_byte = (byte_t*) &val;
    for (int i = 0; i < 8; i++) {
        unsigned int break_length = (addr + i) % size_of_block;
        ret -> data[break_length] = val_byte[i];
    }
}

/*
 * Access data at memory address addr
 * If it is already in cache, increase hit_count
 * If it is not in cache, bring it in cache, increase miss count
 * Also increase eviction_count if a line is evicted
 *
 * Called by cache-runner; no need to modify it if you implement
 * check_hit() and handle_miss()
 */
void access_data(cache_t *cache, uword_t addr, operation_t operation)
{
    if(!check_hit(cache, addr, operation))
        free(handle_miss(cache, addr, operation, NULL));
}
