/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_base.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_pvc_and_later.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xe2_hpg_and_later.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xe3_and_later.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xehp_and_later.inl"
#include "level_zero/core/source/helpers/l0_populate_factory.h"

#include "hw_cmds_xe3p_core.h"

namespace L0 {
using Family = NEO::Xe3pCoreFamily;
static auto gfxCore = IGFX_XE3P_CORE;

#include "level_zero/core/source/helpers/l0_gfx_core_helper_factory_init.inl"

template <>
uint32_t L0GfxCoreHelperHw<Family>::getGrfRegisterCount(uint32_t *regPtr) const {
    return (regPtr[4] & 0x3FF);
}

template <>
bool L0GfxCoreHelperHw<Family>::platformSupportsStateBaseAddressTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const {
    if (rootDeviceEnvironment.getHardwareInfo()->capabilityTable.supportsImages) {
        return false;
    } else {
        return true;
    }
}

template <>
NEO::HeapAddressModel L0GfxCoreHelperHw<Family>::getPlatformHeapAddressModel(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    if (hwInfo.capabilityTable.supportsImages) {
        return NEO::HeapAddressModel::privateHeaps;
    } else {
        if (rootDeviceEnvironment.getHelper<NEO::CompilerProductHelper>().isHeaplessModeEnabled(hwInfo)) {
            return NEO::HeapAddressModel::globalStateless;
        } else {
            return NEO::HeapAddressModel::privateHeaps;
        }
    }
}

template <>
bool L0GfxCoreHelperHw<Family>::implicitSynchronizedDispatchForCooperativeKernelsAllowed() const {
    return true;
}

template <>
bool L0GfxCoreHelperHw<Family>::alwaysAllocateEventInLocalMem() const {
    return true;
}

template <>
bool L0GfxCoreHelperHw<Family>::threadResumeRequiresUnlock() const {
    return true;
}

template <>
bool L0GfxCoreHelperHw<Family>::isThreadControlStoppedSupported() const {
    return false;
}

constexpr uint64_t ipSamplingIpMaskXe3p = 0x1fffffffffffffff;

/*
 * Xe3p stall sample data item format:
 *
 * Bits        Field
 * 0  to 60    IP (addr)
 * 61 to 68    tdr count
 * 69 to 76    other count
 * 77 to 84    control count
 * 85 to 92    pipestall count
 * 93 to 100   send count
 * 101 to 108  dist_acc count
 * 109 to 116  sbid count
 * 117 to 124  sync count
 * 125 to 132  inst_fetch count
 * 133 to 140  active count
 *
 * total size 64 bytes
 */

template <>
bool L0GfxCoreHelperHw<Family>::stallIpDataMapUpdateFromData(const uint8_t *pRawIpData, std::map<uint64_t, void *> &stallSumIpDataMap) {
    constexpr int ipStallSamplingOffset = 7;              // Offset to read the first Stall Sampling report after IP Address.
    constexpr int ipStallSamplingReportShift = 5;         // Shift in bits required to read the stall sampling report data due to the IP address [0-60] bits to access the next report category data.
    constexpr int stallSamplingReportCategoryMask = 0xff; // Mask for Stall Sampling Report Category.

    const uint8_t *tempAddr = pRawIpData;
    uint64_t ip = 0ULL;
    memcpy_s(reinterpret_cast<uint8_t *>(&ip), sizeof(ip), tempAddr, sizeof(ip));
    ip &= ipSamplingIpMaskXe3p;
    StallSumIpDataXeCore_t *stallSumData = nullptr;
    if (stallSumIpDataMap.count(ip) == 0) {
        stallSumData = new StallSumIpDataXeCore_t{};
        stallSumData->tdrCount = 0;
        stallSumData->otherCount = 0;
        stallSumData->controlCount = 0;
        stallSumData->pipeStallCount = 0;
        stallSumData->sendCount = 0;
        stallSumData->distAccCount = 0;
        stallSumData->sbidCount = 0;
        stallSumData->syncCount = 0;
        stallSumData->instFetchCount = 0;
        stallSumData->activeCount = 0;
        stallSumIpDataMap[ip] = stallSumData;
    } else {
        stallSumData = reinterpret_cast<StallSumIpDataXeCore_t *>(stallSumIpDataMap[ip]);
    }
    tempAddr += ipStallSamplingOffset;

    auto getCount = [&tempAddr]() {
        uint16_t tempCount = 0;
        memcpy_s(reinterpret_cast<uint8_t *>(&tempCount), sizeof(tempCount), tempAddr, sizeof(tempCount));
        tempCount = (tempCount >> ipStallSamplingReportShift) & stallSamplingReportCategoryMask;
        tempAddr += 1;
        return static_cast<uint8_t>(tempCount);
    };

    stallSumData->tdrCount += getCount();
    stallSumData->otherCount += getCount();
    stallSumData->controlCount += getCount();
    stallSumData->pipeStallCount += getCount();
    stallSumData->sendCount += getCount();
    stallSumData->distAccCount += getCount();
    stallSumData->sbidCount += getCount();
    stallSumData->syncCount += getCount();
    stallSumData->instFetchCount += getCount();
    stallSumData->activeCount += getCount();

    return false;
}

template <>
void L0GfxCoreHelperHw<Family>::stallIpDataMapUpdateFromMap(std::map<uint64_t, void *> &sourceMap, std::map<uint64_t, void *> &stallSumIpDataMap) {

    for (auto &entry : sourceMap) {
        uint64_t ip = entry.first;
        StallSumIpDataXeCore_t *sourceData = reinterpret_cast<StallSumIpDataXeCore_t *>(entry.second);
        if (stallSumIpDataMap.count(ip) == 0) {
            StallSumIpDataXeCore_t *newData = new StallSumIpDataXeCore_t{};
            memcpy_s(newData, sizeof(StallSumIpDataXeCore_t), sourceData, sizeof(StallSumIpDataXeCore_t));
            stallSumIpDataMap[ip] = newData;
        } else {
            StallSumIpDataXeCore_t *destData = reinterpret_cast<StallSumIpDataXeCore_t *>(stallSumIpDataMap[ip]);
            destData->tdrCount += sourceData->tdrCount;
            destData->otherCount += sourceData->otherCount;
            destData->controlCount += sourceData->controlCount;
            destData->pipeStallCount += sourceData->pipeStallCount;
            destData->sendCount += sourceData->sendCount;
            destData->distAccCount += sourceData->distAccCount;
            destData->sbidCount += sourceData->sbidCount;
            destData->syncCount += sourceData->syncCount;
            destData->instFetchCount += sourceData->instFetchCount;
            destData->activeCount += sourceData->activeCount;
        }
    }
}

template <>
uint64_t L0GfxCoreHelperHw<Family>::getIpSamplingIpMask() const {
    return ipSamplingIpMaskXe3p;
}

template class L0GfxCoreHelperHw<Family>;
} // namespace L0
