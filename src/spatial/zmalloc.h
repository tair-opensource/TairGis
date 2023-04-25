/* For now just use stdlib malloc, but may be expaned in the future.  */
 
#ifndef ZMALLOC_H_
#define ZMALLOC_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include "../redismodule.h"

#define zmalloc(size) (RedisModule_Alloc((size)))
#define zrealloc(ptr,size) (RedisModule_Realloc((ptr),(size)))
#define zfree(ptr) (RedisModule_Free((ptr)))

#if defined(__cplusplus)
}
#endif
#endif /* ZMALLOC_H_ */