#include <stdint.h>
#include <stdbool.h>

#ifndef _TOKEN_BUCKET_H_
#define _TOKEN_BUCKET_H_

#ifdef __GNUC__
#define atomic_cas(p, old, new) __sync_bool_compare_and_swap (p, old, new)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#error "atomic operations are supported only by GCC"
#endif

struct token_bucket {
	uint64_t max_tokens;
	volatile uint64_t cur_tokens;
	uint64_t rate_bps;
	uint64_t interval; //ms
	uint64_t add_tokens;
};

enum token_bucket_action {
	TB_INVAL, // invalid result
	TB_SUCCESS,
	TB_FAILED,
};

/*
 * thread unsafety*/
struct token_bucket *token_bucket_init(uint64_t bps,  uint64_t interval, float scale);
void token_bucket_destroy(struct token_bucket *tb);

/* add token to bucket
 * thread safety
 * return the actual number added*/
uint64_t token_bucket_add_tokens(struct token_bucket *tb, uint64_t tokens);

/* get tokens from token bucket, may block
 * thread safety
 * return 
 *     TB_INVAL : tokens is more than max tokens
 *     TB_SUCCESS  : get tokens success*/
enum token_bucket_action 
token_bucket_get_tokens(struct token_bucket *tb, uint64_t tokens);

/* get tokens from token bucket, return immediately
 * thread safety
 * return 
 *     TB_INVAL : tokens is more than max tokens
 *     TB_SUCCESS  : get tokens success
 *     TB_FAILED  : get tokens failed*/
enum token_bucket_action 
token_bucket_try_get_tokens(struct token_bucket *tb, uint64_t tokens);

#endif //_TOKEN_BUCKET_H_