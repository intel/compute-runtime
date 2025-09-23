/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_helper.h"

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/aub_mem_dump/page_table_entry_bits.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/basic_math.h"
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

uint64_t AubHelper::getTotalMemBankSize(const ReleaseHelper *releaseHelper) {
    if (releaseHelper) {
        return releaseHelper->getTotalMemBankSize();
    }
    return 32ull * MemoryConstants::gigaByte;
}

int AubHelper::getMemTrace(uint64_t pdEntryBits) {
    if (pdEntryBits & BIT(PageTableEntry::localMemoryBit)) {
        return AubMemDump::AddressSpaceValues::TraceLocal;
    }
    return AubMemDump::AddressSpaceValues::TraceNonlocal;
}

uint64_t AubHelper::getPTEntryBits(uint64_t pdEntryBits) {
    pdEntryBits &= ~BIT(PageTableEntry::localMemoryBit);
    return pdEntryBits;
}

uint32_t AubHelper::getMemType(uint32_t addressSpace) {
    if (addressSpace == AubMemDump::AddressSpaceValues::TraceLocal) {
        return MemType::local;
    }
    return MemType::system;
}

uint64_t AubHelper::getPerTileLocalMemorySize(const HardwareInfo *pHwInfo, const ReleaseHelper *releaseHelper) {
    if (debugManager.flags.HBMSizePerTileInGigabytes.get() > 0) {
        return debugManager.flags.HBMSizePerTileInGigabytes.get() * MemoryConstants::gigaByte;
    }
    return getTotalMemBankSize(releaseHelper) / GfxCoreHelper::getSubDevicesCount(pHwInfo);
}

const std::string AubHelper::getDeviceConfigString(const ReleaseHelper *releaseHelper, uint32_t tileCount, uint32_t sliceCount, uint32_t subSliceCount, uint32_t euPerSubSliceCount) {
    if (releaseHelper) {
        return releaseHelper->getDeviceConfigString(tileCount, sliceCount, subSliceCount, euPerSubSliceCount);
    }
    char configString[16] = {0};
    if (tileCount > 1) {
        auto err = snprintf_s(configString, sizeof(configString), sizeof(configString), "%utx%ux%ux%u", tileCount, sliceCount, subSliceCount, euPerSubSliceCount);
        UNRECOVERABLE_IF(err < 0);
    } else {
        auto err = snprintf_s(configString, sizeof(configString), sizeof(configString), "%ux%ux%u", sliceCount, subSliceCount, euPerSubSliceCount);
        UNRECOVERABLE_IF(err < 0);
    }
    return configString;
}

} // namespace NEO
