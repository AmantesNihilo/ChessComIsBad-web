#ifndef THIRD_PARTY_CTEST_H
#define THIRD_PARTY_CTEST_H

#include <stdio.h>
#include <string.h>

typedef void (*ctest_func)(void);

static int ctest_failed = 0;
static int ctest_current_failed = 0;
static int ctest_total = 0;

#define CTEST(suite, name) void suite##_##name(void)
#define CTEST_DECLARE(suite, name) void suite##_##name(void)

#define ASSERT_TRUE(value) do { \
    if (!(value)) { \
        printf("FAIL: %s:%d: ASSERT_TRUE(%s)\n", __FILE__, __LINE__, #value); \
        ctest_current_failed = 1; \
        return; \
    } \
} while (0)

#define ASSERT_FALSE(value) do { \
    if (value) { \
        printf("FAIL: %s:%d: ASSERT_FALSE(%s)\n", __FILE__, __LINE__, #value); \
        ctest_current_failed = 1; \
        return; \
    } \
} while (0)

#define ASSERT_EQUAL(expected, actual) do { \
    long long ctest_expected = (long long)(expected); \
    long long ctest_actual = (long long)(actual); \
    if (ctest_expected != ctest_actual) { \
        printf("FAIL: %s:%d: expected %lld, got %lld\n", \
               __FILE__, __LINE__, ctest_expected, ctest_actual); \
        ctest_current_failed = 1; \
        return; \
    } \
} while (0)

#define ASSERT_STR(expected, actual) do { \
    const char *ctest_expected = (expected); \
    const char *ctest_actual = (actual); \
    if (strcmp(ctest_expected, ctest_actual) != 0) { \
        printf("FAIL: %s:%d: expected \"%s\", got \"%s\"\n", \
               __FILE__, __LINE__, ctest_expected, ctest_actual); \
        ctest_current_failed = 1; \
        return; \
    } \
} while (0)

static int ctest_run(const char *name, ctest_func func) {
    ctest_total++;
    ctest_current_failed = 0;
    func();
    if (ctest_current_failed) {
        ctest_failed++;
        printf("CTEST_FAIL: %s\n", name);
        return 1;
    }
    printf("CTEST_OK: %s\n", name);
    return 0;
}

#define RUN_CTEST(suite, name) ctest_run(#suite "." #name, suite##_##name)

static int ctest_result(void) {
    printf("CTEST_RESULT: %d tests, %d failed\n", ctest_total, ctest_failed);
    return ctest_failed ? 1 : 0;
}

#endif
