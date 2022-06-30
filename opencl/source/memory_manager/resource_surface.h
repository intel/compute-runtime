/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/surface.h"

#include "opencl/extensions/public/cl_ext_private.h"

namespace NEO {
class ResourceSurface : public GeneralSurface {
  public:
    ResourceSurface(GraphicsAllocation *gfxAlloc, cl_resource_barrier_type type, cl_resource_memory_scope scope) : GeneralSurface(gfxAlloc), resourceType(type), resourceScope(scope) {}
    ~ResourceSurface() override = default;

    GraphicsAllocation *getGraphicsAllocation() {
        return gfxAllocation;
    }

    cl_resource_barrier_type resourceType;
    cl_resource_memory_scope resourceScope;
};
} // namespace NEO