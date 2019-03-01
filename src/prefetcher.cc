/*
 * A sample prefetcher which does sequential one-block lookahead.
 * This means that the prefetcher fetches the next block _after_ the one that
 * was just accessed. It also ignores requests to blocks already in the cache.
 */

#include <limits.h>
#include <strings.h>
#include "interface.hh"

#ifndef MARKOV_TABLE_SIZE
#define MARKOV_TABLE_SIZE 32
#endif

#ifndef MARKOV_TABLE_DEGREE
#define MARKOV_TABLE_DEGREE 4
#endif

typedef Addr Tag; 
inline Tag tag_of(Addr addr) {
    return addr >> 6;
}

typedef struct {
    Tag    tag;
    unsigned short confidence;
} PredictionEntry;

typedef struct {
    Tag    miss_tag;
    unsigned short timestamp;
    PredictionEntry predictions[MARKOV_TABLE_DEGREE];
} MarkovEntry;


MarkovEntry markov_table[MARKOV_TABLE_SIZE] = {0};
int markov_table_size = 0;
unsigned short timestamp = 0;
    

Tag prev_tag;

/* Get the index of the Markov entry for this address */
MarkovEntry* get_entry(Tag tag) {
    for (int i = 0; i < MARKOV_TABLE_SIZE; i++) {
        if (markov_table[i].miss_tag == tag)
            return &markov_table[i];
    }

    return NULL;
}

/* Get the predicted addr, the one with the greater confidence */
Tag make_prediction(Tag tag) {
    
    MarkovEntry* entry = get_entry(tag);
    unsigned short max_confidence = 0;
    Tag prediction;

    if (!entry) return 0;

    int i;
    for (i = 0; i < MARKOV_TABLE_DEGREE; i++) {
        if (entry->predictions[i].confidence >= max_confidence) {
            prediction = entry->predictions[i].tag;
            max_confidence = entry->predictions[i].confidence;
        }
    }
    
    return prediction;
}

/* Create an entry inside the table (replace one if necessary) */
MarkovEntry* new_entry(Tag tag) {
    
    int to_replace_index = markov_table_size;
    unsigned short lru_timestamp = USHRT_MAX;
    int i, lru_index;

    /* If the table it's full find the LRU to replace it */
    if (markov_table_size == MARKOV_TABLE_SIZE) {
        
        for (i = 0; i < MARKOV_TABLE_SIZE; i++) {
            if (lru_timestamp > markov_table[i].timestamp) {
                lru_timestamp = markov_table[i].timestamp;
                lru_index = i;
            }
        }

        to_replace_index = lru_index;

    } else {
        markov_table_size++;
    }

    markov_table[to_replace_index].miss_tag = tag;
    markov_table[to_replace_index].timestamp = timestamp;
    bzero(markov_table[to_replace_index].predictions, MARKOV_TABLE_DEGREE*sizeof(PredictionEntry));

    return &markov_table[to_replace_index];
}

/* Update the Markov table */
void update_table(Tag miss_tag) {
    
    MarkovEntry* prev_entry = get_entry(prev_tag);
    int i, least_confidence_index = 0;
    unsigned short least_confidence;


    if (timestamp == USHRT_MAX) {
        bzero(markov_table, sizeof(markov_table));
        markov_table_size = 0;
    }

    timestamp++;

    if (!prev_entry) {
        prev_entry = new_entry(prev_tag);
    } else {
        for (i = 0; i < MARKOV_TABLE_DEGREE; i++) {
            if (least_confidence > prev_entry->predictions[i].confidence) {
                least_confidence = prev_entry->predictions[i].confidence;
                least_confidence_index = i;
            }
        }
    }
    
    for (i = 0; i < MARKOV_TABLE_DEGREE; i++) {
        
        if (prev_entry->predictions[i].tag == miss_tag) {
            
            if (prev_entry->predictions[i].confidence < USHRT_MAX)
                prev_entry->predictions[i].confidence++;

            break;
        }
    }

    if (i == MARKOV_TABLE_DEGREE) {
        prev_entry->predictions[least_confidence_index] = {miss_tag, 0};
    }
    
    prev_entry->timestamp = timestamp;
    prev_tag = miss_tag;
}

void prefetch_init(void)
{
    /* Called before any calls to prefetch_access. */
    /* This is the place to initialize data structures. */

    DPRINTF(HWPrefetch, "Initialized Markov prefetcher\n");
}

void prefetch_access(AccessStat stat)
{
    Tag miss, prediction;

    if (stat.miss) {
        
        miss = tag_of(stat.mem_addr);

        /* Make a predicition and prefetch it if necessary */
        prediction = make_prediction(miss);
        if (prediction && !in_cache(prediction)) {
            issue_prefetch(prediction);
        }

        update_table(miss);
    }
}

void prefetch_complete(Addr addr) {
    /*
     * Called when a block requested by the prefetcher has been loaded.
     */
}
