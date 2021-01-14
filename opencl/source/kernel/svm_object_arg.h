/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
class GraphicsAllocation;
class MultiGraphicsAllocation;
class SvmObjectArg {
  public:
    SvmObjectArg(GraphicsAllocation *graphicsAllocation);
    SvmObjectArg(MultiGraphicsAllocation *multiGraphicsAllocation);

    GraphicsAllocation *getGraphicsAllocation(uint32_t rootDeviceIndex) const;
    bool isCoherent() const;
    GraphicsAllocation::AllocationType getAllocationType() const;
    MultiGraphicsAllocation *getMultiDeviceSvmAlloc() const { return multiDeviceSvmAlloc; }

  protected:
    enum class SvmObjectArgType {
        SingleDeviceSvm,
        MultiDeviceSvm
    };

    const SvmObjectArgType type;
    GraphicsAllocation *singleDeviceSvmAlloc = nullptr;
    MultiGraphicsAllocation *multiDeviceSvmAlloc = nullptr;
};
} // namespace NEO
