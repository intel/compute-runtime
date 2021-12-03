/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/compression_selector.h"
#include "shared/source/os_interface/hw_info_config.h"

namespace NEO {
bool CompressionSelector::preferCompressedAllocation(const AllocationProperties &properties, const HardwareInfo &hwInfo) {
    switch (properties.allocationType) {
    case GraphicsAllocation::AllocationType::GLOBAL_SURFACE:
    case GraphicsAllocation::AllocationType::CONSTANT_SURFACE:
    case GraphicsAllocation::AllocationType::SVM_GPU:
    case GraphicsAllocation::AllocationType::PRINTF_SURFACE: {
        const auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
        return hwInfoConfig.allowStatelessCompression(hwInfo);
    }
    default:
        return false;
    }
}

} // namespace NEO
