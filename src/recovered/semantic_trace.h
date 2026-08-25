#ifndef NBA97_RECOVERED_SEMANTIC_TRACE_H
#define NBA97_RECOVERED_SEMANTIC_TRACE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum { NBA97_SEMANTIC_TRACE_SEQUENCE_CAPACITY = 4096 };

void nba97_semantic_trace_reset(void);
void nba97_semantic_trace_record(uint32_t original_address);
uint32_t nba97_semantic_trace_count(uint32_t original_address);
size_t nba97_semantic_trace_copy(uint32_t* destination, size_t capacity);
size_t nba97_semantic_trace_size(void);
uint32_t nba97_semantic_trace_dropped(void);

#ifdef __cplusplus
}
#endif

#endif
