/*
 * Copyright (C) 2019 Intel Corporation
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
} // namespace NEO
