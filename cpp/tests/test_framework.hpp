// SPDX-FileCopyrightText: 2026 CAPCOM CO., LTD.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

namespace redox::test {

inline int g_failures = 0;
inline int g_assertions = 0;

inline void report_failure(const char* file, int line, const std::string& msg) {
    ++g_failures;
    std::cerr << "[FAIL] " << file << ":" << line << " - " << msg << "\n";
}

#define REDOX_CHECK(cond)                                                     \
    do {                                                                      \
        ++::redox::test::g_assertions;                                        \
        if (!(cond)) {                                                        \
            ::redox::test::report_failure(__FILE__, __LINE__, #cond);         \
        }                                                                     \
    } while (false)

#define REDOX_CHECK_EQ(a, b)                                                  \
    do {                                                                      \
        ++::redox::test::g_assertions;                                        \
        auto _a = (a);                                                        \
        auto _b = (b);                                                        \
        if (!(_a == _b)) {                                                    \
            std::ostringstream _oss;                                          \
            _oss << #a << " == " << #b << " (got '" << _a << "' vs '" << _b << "')"; \
            ::redox::test::report_failure(__FILE__, __LINE__, _oss.str());    \
        }                                                                     \
    } while (false)

#define REDOX_CHECK_NEAR(a, b, eps)                                           \
    do {                                                                      \
        ++::redox::test::g_assertions;                                        \
        double _a = static_cast<double>(a);                                   \
        double _b = static_cast<double>(b);                                   \
        if (std::fabs(_a - _b) > (eps)) {                                     \
            std::ostringstream _oss;                                          \
            _oss << #a << " ~= " << #b << " (got " << _a << " vs " << _b << ")"; \
            ::redox::test::report_failure(__FILE__, __LINE__, _oss.str());    \
        }                                                                     \
    } while (false)

#define REDOX_CHECK_THROWS(expr, ExcType)                                     \
    do {                                                                      \
        ++::redox::test::g_assertions;                                        \
        bool _caught = false;                                                 \
        try {                                                                 \
            (void)(expr);                                                     \
        } catch (const ExcType&) {                                            \
            _caught = true;                                                   \
        } catch (...) {                                                       \
        }                                                                     \
        if (!_caught) {                                                       \
            ::redox::test::report_failure(__FILE__, __LINE__,                 \
                                          "Expected exception " #ExcType " in: " #expr); \
        }                                                                     \
    } while (false)

inline int finish(const char* suite_name) {
    if (g_failures == 0) {
        std::cout << "[PASS] " << suite_name << " (" << g_assertions << " assertions)\n";
        return EXIT_SUCCESS;
    }
    std::cerr << "[SUMMARY] " << suite_name << ": " << g_failures << " failure(s) out of "
              << g_assertions << " assertions\n";
    return EXIT_FAILURE;
}

} // namespace redox::test
