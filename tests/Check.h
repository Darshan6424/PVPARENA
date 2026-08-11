#pragma once
// Bare-bones check macros so the tests don't drag in a test framework.

#include <cstdio>
#include <cmath>

inline int g_failures = 0;
inline int g_checks = 0;

inline void reportCheck(bool ok, const char* expr, const char* file, int line) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::printf("  FAIL %s:%d  %s\n", file, line, expr);
    }
}

inline void reportNear(float got, float want, float eps, const char* expr,
                       const char* file, int line) {
    ++g_checks;
    if (std::fabs(got - want) > eps) {
        ++g_failures;
        std::printf("  FAIL %s:%d  %s (got %.3f, want %.3f)\n", file, line, expr, got, want);
    }
}

inline int testSummary(const char* suite) {
    std::printf("%s: %d checks, %d failed\n", suite, g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}

#define CHECK(cond)            reportCheck((cond), #cond, __FILE__, __LINE__)
#define CHECK_NEAR(got, want)  reportNear((got), (want), 0.01f, #got, __FILE__, __LINE__)

#define TEST(name) std::printf("- %s\n", name)
