/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"

#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/source/tbx/tbx_proto.h"

#include "aubstream/aubstream.h"

namespace NEO {

bool AubHelper::isOneTimeAubWritableAllocationType(const AllocationType &type) {
    switch (type) {
    case AllocationType::constantSurface:
    case AllocationType::globalSurface:
    case AllocationType::kernelIsa:
    case AllocationType::kernelIsaInternal:
    case AllocationType::privateSurface:
    case AllocationType::scratchSurface:
    case AllocationType::workPartitionSurface:
    case AllocationType::buffer:
    case AllocationType::image:
    case AllocationType::timestampPacketTagBuffer:
    case AllocationType::externalHostPtr:
    case AllocationType::mapAllocation:
    case AllocationType::svmCpu:
    case AllocationType::svmGpu:
    case AllocationType::gpuTimestampDeviceBuffer:
    case AllocationType::assertBuffer:
    case AllocationType::tagBuffer:
    case AllocationType::syncDispatchToken:
    case AllocationType::hostFunction:
        return true;
    case AllocationType::bufferHostMemory:
        return NEO::debugManager.isTbxPageFaultManagerEnabled() ||
               (NEO::debugManager.flags.SetBufferHostMemoryAlwaysAubWritable.get() == false);
    default:
        return false;
    }
}

uint64_t AubHelper::getPTEntryBits(uint64_t pdEntryBits) {
    pdEntryBits &= ~makeBitMask<PageTableEntry::localMemoryBit>();
    return pdEntryBits;
}

uint64_t AubHelper::getPerTileLocalMemorySize(const HardwareInfo *pHwInfo, const ReleaseHelper &releaseHelper) {
    if (debugManager.flags.HBMSizePerTileInGigabytes.get() > 0) {
        return debugManager.flags.HBMSizePerTileInGigabytes.get() * MemoryConstants::gigaByte;
    }
    return releaseHelper.getTotalMemBankSize() / GfxCoreHelper::getSubDevicesCount(pHwInfo);
}

std::string AubHelper::getDeviceConfigString(bool includeTileCount, bool includeXeCuSegment, uint32_t tileCount, uint32_t sliceCount, uint32_t subSliceCount, uint32_t euPerSubSliceCount) {
    char configString[16] = {0};
    int err = 0;
    if (includeXeCuSegment) {
        uint32_t xecuCount = 1;
        if (sliceCount > 4) {
            UNRECOVERABLE_IF(sliceCount % 4u != 0u);
            xecuCount = sliceCount / 4;
            sliceCount = sliceCount / xecuCount;
        }
        err = snprintf_s(configString, sizeof(configString), sizeof(configString), "%utx%ux%ux%ux%u", tileCount, xecuCount, sliceCount, subSliceCount, euPerSubSliceCount);
    } else if (includeTileCount || tileCount > 1) {
        err = snprintf_s(configString, sizeof(configString), sizeof(configString), "%utx%ux%ux%u", tileCount, sliceCount, subSliceCount, euPerSubSliceCount);
    } else {
        err = snprintf_s(configString, sizeof(configString), sizeof(configString), "%ux%ux%u", sliceCount, subSliceCount, euPerSubSliceCount);
    }
    UNRECOVERABLE_IF(err < 0);
    return configString;
}

} // namespace NEO
