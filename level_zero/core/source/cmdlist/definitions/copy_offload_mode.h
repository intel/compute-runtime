/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/extendable_enum.h"

#include <cstdint>
namespace L0 {

struct CopyOffloadMode : ExtendableEnum {
  public:
    constexpr CopyOffloadMode(uint32_t val) : ExtendableEnum(val) {}
};

namespace CopyOffloadModes {
static constexpr CopyOffloadMode disabled = 0;
static constexpr CopyOffloadMode dualStream = 1;
} // namespace CopyOffloadModes

} // namespace L0
