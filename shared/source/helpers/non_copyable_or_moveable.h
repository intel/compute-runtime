/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
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
} // namespace NEO
