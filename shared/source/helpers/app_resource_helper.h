/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/allocation_type.h"

#include <cstddef>

namespace NEO {
struct AppResourceHelper {
  public:
    static const char *getResourceTagStr(AllocationType type);
    static void copyResourceTagStr(char *dst, AllocationType type, size_t size);
};
} // namespace NEO
