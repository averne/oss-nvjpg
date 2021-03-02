// Copyright (C) 2021 averne
//
// This file is part of oss-nvjpg.
//
// oss-nvjpg is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// oss-nvjpg is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with oss-nvjpg.  If not, see <http://www.gnu.org/licenses/>.

#pragma once

#include <cerrno>
#include <concepts>
#include <utility>

#define  _NJ_CAT(x, y) x ## y
#define   NJ_CAT(x, y) _NJ_CAT(x, y)
#define  _NJ_STR(x)    #x
#define   NJ_STR(x)    _NJ_STR(x)

#define NJ_ANONYMOUS_VAR NJ_CAT(var, __COUNTER__)

#define NJ_SCOPEGUARD(f) auto NJ_ANONYMOUS_VAR = ::nj::ScopeGuard(f)

#ifndef __SWITCH__
typedef int Result;
#endif

#define NJ_TRY_RET(expr)            \
    if (auto _rc = (expr); _rc)     \
        return _rc;

#define NJ_TRY_ERRNO(expr)          \
    if ((expr); errno)              \
        return errno;

#define NJ_UNUSED(...) ::nj::variadic_unused(__VA_ARGS__)

namespace nj {

template <typename ...Args>
consteval void variadic_unused(Args &&...args) {
    (static_cast<void>(args), ...);
}

template <typename F>
struct [[nodiscard]] ScopeGuard {
    ScopeGuard(F &&f): f(std::move(f)) { }

    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard(ScopeGuard &&)      = delete;
    ScopeGuard &operator =(const ScopeGuard &) = delete;
    ScopeGuard &operator =(ScopeGuard &&)      = delete;

    ~ScopeGuard() {
        this->f();
    }

    private:
        F f;
};

constexpr auto bit(std::unsigned_integral auto bits) {
    return 1 << bits;
}

constexpr auto mask(std::unsigned_integral auto bits) {
    return (1 << bits) - 1;
}

template <typename T>
constexpr T align_down(T val, T align) {
    return val & ~(align - 1);
}

template <typename T>
constexpr T align_up(T val, T align) {
    return (val + align - 1) & ~(align - 1);
}

template <typename T>
constexpr bool is_aligned(T val, T align) {
    return !(val & (align - 1));
}

} // namespace nj
