/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/compression_selector.h"

namespace NEO {
bool CompressionSelector::preferRenderCompressedBuffer(const AllocationProperties &properties, const HardwareInfo &hwInfo) {
    switch (properties.allocationType) {
    case GraphicsAllocation::AllocationType::BUFFER_COMPRESSED:
        return true;
    case GraphicsAllocation::AllocationType::GLOBAL_SURFACE:
    case GraphicsAllocation::AllocationType::CONSTANT_SURFACE:
    case GraphicsAllocation::AllocationType::SVM_GPU:
    case GraphicsAllocation::AllocationType::PRINTF_SURFACE: {
        auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
        return hwHelper.allowStatelessCompression(hwInfo);
    }
    default:
        return false;
    }
}

} // namespace NEO
