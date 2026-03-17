#ifndef _ASSERT_H
#define _ASSERT_H

void __assert_fail(const char* expr, const char* file, int line);

#define assert(expr) \
    ((expr) ? (void)0 : __assert_fail(#expr, __FILE__, __LINE__))

#endif
