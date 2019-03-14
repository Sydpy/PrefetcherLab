/*
 * A sample prefetcher which does sequential one-block lookahead.
 * This means that the prefetcher fetches the next block _after_ the one that
 * was just accessed. It also ignores requests to blocks already in the cache.
 */

#include <limits.h>
#include <strings.h>
#include <stdlib.h>
#include "interface.hh"

#ifndef MARKOV_TABLE_SIZE
#define MARKOV_TABLE_SIZE 2048
#endif

#ifndef MARKOV_TABLE_DEGREE
#define MARKOV_TABLE_DEGREE 4
#endif

typedef struct {
    Addr addr; 
    uint8_t confidence; 
} Prediction;

typedef struct {
    Addr addr;
    uint16_t timestamp;
    Prediction predictions[MARKOV_TABLE_DEGREE];
} MarkovEntry;


MarkovEntry markov_table[MARKOV_TABLE_SIZE] = {0};
uint32_t markov_table_size = 0;
uint16_t timestamp = 0;
MarkovEntry* prev_entry = NULL;


int compare_pred(Prediction* p1, Prediction* p2) {
    return p1->confidence - p2->confidence;
}


/* Get the index of the Markov entry for this address */
MarkovEntry* get_entry(Addr addr) {
    uint32_t i;
    for (i = 0; i < markov_table_size; i++) {
        if (markov_table[i].addr == addr)
            return &markov_table[i];
    }

    return NULL;
}


/* Create an entry inside the table (replace one if necessary) */
MarkovEntry* new_entry(Addr addr) {
    
    MarkovEntry* entry;
    uint32_t insert_at = markov_table_size;
    uint16_t lru_timestamp = UINT16_MAX;
    uint32_t i, lru_index;

    /* If the table is full, replace the lru entry */
    if (markov_table_size == MARKOV_TABLE_SIZE) {
        for(i = 0; i < MARKOV_TABLE_SIZE; i++) {
            if (lru_timestamp > markov_table[i].timestamp) {
                lru_timestamp = markov_table[i].timestamp;
                lru_index = i;
            }
        }
        insert_at = lru_index;
    } else {
        markov_table_size++;
    }

    entry = &markov_table[insert_at];
    entry->addr = addr;
    entry->timestamp = timestamp;
    bzero(entry->predictions, MARKOV_TABLE_DEGREE*sizeof(Prediction));

    return entry;
}

/* Update the Markov table */
void update_table(MarkovEntry* entry, Addr addr) {

    uint32_t update_at = 0;
    uint8_t least_confidence = UINT8_MAX;
    uint8_t i, least_confidence_index;

    for (i = 0; i < MARKOV_TABLE_DEGREE; i++) {
            
        /* Look if the current tag is a candidate prediction */
        if (entry->predictions[i].addr == addr) {
            update_at = i;
            break;
        }
        /* Also look for the least confident entry in case we have
         * to replace it */
        if (least_confidence > entry->predictions[i].confidence) {
            least_confidence = entry->predictions[i].confidence;
            least_confidence_index = i;
        }
    }
        
    /* if we didn't find the entry, replace the least confident */
    if (i == MARKOV_TABLE_DEGREE) {
        update_at = least_confidence_index;
        entry->predictions[update_at].confidence = 0;
    }

    entry->predictions[update_at].addr = addr;
    if (entry->predictions[update_at].confidence < UINT8_MAX) {
        entry->predictions[update_at].confidence++;
    }

    qsort(entry->predictions, MARKOV_TABLE_DEGREE, sizeof(Prediction), compare_pred);
}

void prefetch_init(void)
{
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */
}

void prefetch_access(AccessStat stat)
{
    Addr addr = stat.mem_addr;
    MarkovEntry* entry = get_entry(addr);
    Prediction pred;
    int8_t i;

    if (stat.miss) {

        DPRINTF(HWPrefetch, "Missed %#x\n", addr);

        if (!entry) {
            entry = new_entry(addr);
        } else {
        
            for (i = MARKOV_TABLE_DEGREE-1; i > -1; i--) {
                pred = entry->predictions[i];
                if (pred.confidence > 0 && !in_cache(pred.addr)) {
                    DPRINTF(HWPrefetch, "Prefetching %#x\n", pred.addr);
                    issue_prefetch(pred.addr);
                }
            }
        }
    
        if (prev_entry)
            update_table(prev_entry, addr);

        prev_entry = entry;
    }
}

void prefetch_complete(Addr addr) {
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
    set_prefetch_bit(addr);
}
