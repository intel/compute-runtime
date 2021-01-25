/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/kernel/svm_object_arg.h"

#include "shared/source/memory_manager/multi_graphics_allocation.h"

namespace NEO {
SvmObjectArg::SvmObjectArg(GraphicsAllocation *graphicsAllocation) : type(SvmObjectArgType::SingleDeviceSvm), singleDeviceSvmAlloc(graphicsAllocation) {}
SvmObjectArg::SvmObjectArg(MultiGraphicsAllocation *multiGraphicsAllocation) : type(SvmObjectArgType::MultiDeviceSvm), multiDeviceSvmAlloc(multiGraphicsAllocation) {}

GraphicsAllocation *SvmObjectArg::getGraphicsAllocation(uint32_t rootDeviceIndex) const {
    if (SvmObjectArgType::SingleDeviceSvm == type) {
        DEBUG_BREAK_IF(singleDeviceSvmAlloc && rootDeviceIndex != singleDeviceSvmAlloc->getRootDeviceIndex());
        return singleDeviceSvmAlloc;
    }
    if (multiDeviceSvmAlloc) {
        return multiDeviceSvmAlloc->getGraphicsAllocation(rootDeviceIndex);
    }
    return nullptr;
}

bool SvmObjectArg::isCoherent() const {
    if (SvmObjectArgType::SingleDeviceSvm == type) {
        return singleDeviceSvmAlloc->isCoherent();
    }
    return multiDeviceSvmAlloc->isCoherent();
}
GraphicsAllocation::AllocationType SvmObjectArg::getAllocationType() const {
    if (SvmObjectArgType::SingleDeviceSvm == type) {
        return singleDeviceSvmAlloc->getAllocationType();
    }
    return multiDeviceSvmAlloc->getAllocationType();
}
} // namespace NEO
