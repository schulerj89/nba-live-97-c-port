#include "semantic_trace.h"

typedef struct nba97_semantic_counter {
    uint32_t address;
    uint32_t count;
} nba97_semantic_counter;

enum { NBA97_SEMANTIC_COUNTER_CAPACITY = 128 };

static uint32_t sequence[NBA97_SEMANTIC_TRACE_SEQUENCE_CAPACITY];
static size_t sequence_size;
static uint32_t dropped_events;
static nba97_semantic_counter counters[NBA97_SEMANTIC_COUNTER_CAPACITY];
static size_t counter_count;

void nba97_semantic_trace_reset(void) {
    sequence_size = 0;
    dropped_events = 0;
    counter_count = 0;
}

void nba97_semantic_trace_record(uint32_t original_address) {
    size_t index;
    for (index = 0; index < counter_count; ++index) {
        if (counters[index].address == original_address) {
            ++counters[index].count;
            break;
        }
    }
    if (index == counter_count && counter_count < NBA97_SEMANTIC_COUNTER_CAPACITY) {
        counters[counter_count].address = original_address;
        counters[counter_count].count = 1;
        ++counter_count;
    }
    if (sequence_size < NBA97_SEMANTIC_TRACE_SEQUENCE_CAPACITY)
        sequence[sequence_size++] = original_address;
    else
        ++dropped_events;
}

uint32_t nba97_semantic_trace_count(uint32_t original_address) {
    size_t index;
    for (index = 0; index < counter_count; ++index)
        if (counters[index].address == original_address)
            return counters[index].count;
    return 0;
}

size_t nba97_semantic_trace_copy(uint32_t* destination, size_t capacity) {
    size_t index;
    const size_t count = sequence_size < capacity ? sequence_size : capacity;
    if (destination != NULL)
        for (index = 0; index < count; ++index)
            destination[index] = sequence[index];
    return count;
}

size_t nba97_semantic_trace_size(void) {
    return sequence_size;
}

uint32_t nba97_semantic_trace_dropped(void) {
    return dropped_events;
}
