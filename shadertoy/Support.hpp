/*
    SPDX-License-Identifier: Apache-2.0
    Copyright 2023 Yingwei Zheng
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
        http://www.apache.org/licenses/LICENSE-2.0
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#pragma once

#include "shadertoy/Config.hpp"
#include <chrono>
#pragma warning(push, 0)
#include <gsl/gsl>
#pragma warning(pop)

SHADERTOY_NAMESPACE_BEGIN

template <typename F>
auto scopeExit(F&& f) {
    return gsl::finally(std::forward<F>(f));
}

template <typename F>
auto scopeFail(F&& f) {
    return gsl::finally([func = std::forward<F>(f)]() mutable {
        if(std::uncaught_exceptions())
            func();
    });
}

using Clock = std::chrono::steady_clock;

struct Error final : std::exception {};
#ifdef NDEBUG
#if defined(__cpp_lib_unreachable)
#define SHADERTOY_UNREACHABLE() std::unreachable()
#elif defined(__GNUC__)
#define SHADERTOY_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#define SHADERTOY_UNREACHABLE() __assume(false)
#else
#define SHADERTOY_UNREACHABLE() reportFatalError("unreachable")  // fallback
#endif
#else
#define SHADERTOY_UNREACHABLE() reportFatalError("unreachable")
#endif
[[noreturn]] void reportFatalError(std::string_view error);
[[noreturn]] void reportNotImplemented();
SHADERTOY_NAMESPACE_END
