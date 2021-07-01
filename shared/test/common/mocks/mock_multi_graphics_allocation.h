/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/multi_graphics_allocation.h"

namespace NEO {

struct MockMultiGraphicsAllocation : public MultiGraphicsAllocation {
    using MultiGraphicsAllocation::graphicsAllocations;
    using MultiGraphicsAllocation::migrationSyncData;
    using MultiGraphicsAllocation::MultiGraphicsAllocation;
};

} // namespace NEO
