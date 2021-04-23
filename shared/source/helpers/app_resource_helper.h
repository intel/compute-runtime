/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
struct AppResourceHelper {
  public:
    static const char *getResourceTagStr(GraphicsAllocation::AllocationType type);
    static void copyResourceTagStr(char *dst, GraphicsAllocation::AllocationType type, size_t size);
};

} // namespace NEO
