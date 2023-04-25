#ifndef TEST_H_
#define TEST_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <assert.h>

#define tassert(x) ((void)((x) || (__tassert_fail(#x, __FILE__, __LINE__, __func__),0)))
#ifdef assert
#undef assert
#define assert tassert
#endif

void __tassert_fail(const char *what, const char *file, int line, const char *func);

void stopClock();
void restartClock();

#if defined(__cplusplus)
}
#endif
#endif /* TEST_H_ */