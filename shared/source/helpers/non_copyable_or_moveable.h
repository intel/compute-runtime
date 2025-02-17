/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <type_traits>

namespace NEO {
class NonCopyableAndNonMovableClass {
  public:
    NonCopyableAndNonMovableClass() = default;
    NonCopyableAndNonMovableClass(const NonCopyableAndNonMovableClass &) = delete;
    NonCopyableAndNonMovableClass &operator=(const NonCopyableAndNonMovableClass &) = delete;

    NonCopyableAndNonMovableClass(NonCopyableAndNonMovableClass &&) = delete;
    NonCopyableAndNonMovableClass &operator=(NonCopyableAndNonMovableClass &&) = delete;
};

class NonCopyableClass {
  public:
    NonCopyableClass() = default;
    NonCopyableClass(const NonCopyableClass &) = delete;
    NonCopyableClass &operator=(const NonCopyableClass &) = delete;

    NonCopyableClass(NonCopyableClass &&) = default;
    NonCopyableClass &operator=(NonCopyableClass &&) = default;
};

template <typename T>
concept NonCopyable = !
std::is_copy_constructible_v<T> &&
    !std::is_copy_assignable_v<T>;

template <typename T>
concept NonMovable = !
std::is_move_constructible_v<T> &&
    !std::is_move_assignable_v<T>;

template <typename T>
concept NonCopyableAndNonMovable = NonCopyable<T> && NonMovable<T>;

} // namespace NEO
