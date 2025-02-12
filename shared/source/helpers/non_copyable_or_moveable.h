/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <type_traits>

namespace NEO {
class NonCopyableOrMovableClass {
  public:
    NonCopyableOrMovableClass() = default;
    NonCopyableOrMovableClass(const NonCopyableOrMovableClass &) = delete;
    NonCopyableOrMovableClass &operator=(const NonCopyableOrMovableClass &) = delete;

    NonCopyableOrMovableClass(NonCopyableOrMovableClass &&) = delete;
    NonCopyableOrMovableClass &operator=(NonCopyableOrMovableClass &&) = delete;
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
concept NonCopyableOrMovable = !
std::is_copy_constructible_v<T> &&
    !std::is_copy_assignable_v<T> &&
    !std::is_move_constructible_v<T> &&
    !std::is_move_assignable_v<T>;

template <typename T>
concept NonCopyable = !
std::is_copy_constructible_v<T> &&
    !std::is_copy_assignable_v<T>;

} // namespace NEO
