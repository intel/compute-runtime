/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_base.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_pvc_and_later.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_pvc_to_xe2.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_tgllp_to_pvc.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xe_hpg_and_xe_hpc.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xe_hpg_to_xe2_hpg.inl"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper_xehp_and_later.inl"
#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace L0 {

using Family = NEO::XeHpcCoreFamily;
static auto gfxCore = IGFX_XE_HPC_CORE;

#include "level_zero/core/source/helpers/l0_gfx_core_helper_factory_init.inl"

template <>
bool L0GfxCoreHelperHw<Family>::alwaysAllocateEventInLocalMem() const {
    return true;
}

template <>
bool L0GfxCoreHelperHw<Family>::platformSupportsStateBaseAddressTracking(const NEO::RootDeviceEnvironment &rootDeviceEnvironment) const {
    return true;
}

/*
 * stall sample data item format:
 *
 * Bits		Field
 * 0  to 28	IP (addr)
 * 29 to 36	active count
 * 37 to 44	other count
 * 45 to 52	control count
 * 53 to 60	pipestall count
 * 61 to 68	send count
 * 69 to 76	dist_acc count
 * 77 to 84	sbid count
 * 85 to 92	sync count
 * 93 to 100	inst_fetch count
 *
 * bytes at index 48 and 49, subSlice
 * bytes at index 50 and 51, flags
 *
 * total size 64 bytes
 */

#pragma pack(1)
typedef struct StallSumIpData {
    uint64_t activeCount;
    uint64_t otherCount;
    uint64_t controlCount;
    uint64_t pipeStallCount;
    uint64_t sendCount;
    uint64_t distAccCount;
    uint64_t sbidCount;
    uint64_t syncCount;
    uint64_t instFetchCount;
} StallSumIpData_t;
#pragma pack()

constexpr uint32_t ipSamplingMetricCountXe = 10u;
constexpr uint64_t ipSamplingIpMaskXe = 0x1fffffff;

template <>
uint32_t L0GfxCoreHelperHw<Family>::getIpSamplingMetricCount() {
    return ipSamplingMetricCountXe;
}

template <>
void L0GfxCoreHelperHw<Family>::stallIpDataMapDelete(std::map<uint64_t, void *> &stallSumIpDataMap) {
    for (auto i = stallSumIpDataMap.begin(); i != stallSumIpDataMap.end(); i++) {
        StallSumIpData_t *stallSumData = reinterpret_cast<StallSumIpData_t *>(i->second);
        if (stallSumData) {
            delete stallSumData;
            i->second = nullptr;
        }
    }
}

template <>
bool L0GfxCoreHelperHw<Family>::stallIpDataMapUpdate(std::map<uint64_t, void *> &stallSumIpDataMap, const uint8_t *pRawIpData) {
    constexpr int ipStallSamplingOffset = 3;                      // Offset to read the first Stall Sampling report after IP Address.
    constexpr int ipStallSamplingReportShift = 5;                 // Shift in bits required to read the stall sampling report data due to the IP address [0-28] bits to access the next report category data.
    constexpr int stallSamplingReportCategoryMask = 0xff;         // Mask for Stall Sampling Report Category.
    constexpr int stallSamplingReportSubSliceAndFlagsOffset = 48; // Offset to access Stall Sampling Report Sub Slice and flags.

    const uint8_t *tempAddr = pRawIpData;
    uint64_t ip = 0ULL;
    memcpy_s(reinterpret_cast<uint8_t *>(&ip), sizeof(ip), tempAddr, sizeof(ip));
    ip &= ipSamplingIpMaskXe;
    StallSumIpData_t *stallSumData = nullptr;
    if (stallSumIpDataMap.count(ip) == 0) {
        stallSumData = new StallSumIpData_t{};
        stallSumData->activeCount = 0;
        stallSumData->otherCount = 0;
        stallSumData->controlCount = 0;
        stallSumData->pipeStallCount = 0;
        stallSumData->sendCount = 0;
        stallSumData->distAccCount = 0;
        stallSumData->sbidCount = 0;
        stallSumData->syncCount = 0;
        stallSumData->instFetchCount = 0;
        stallSumIpDataMap[ip] = stallSumData;
    } else {
        stallSumData = reinterpret_cast<StallSumIpData_t *>(stallSumIpDataMap[ip]);
    }
    tempAddr += ipStallSamplingOffset;

    auto getCount = [&tempAddr]() {
        uint16_t tempCount = 0;
        memcpy_s(reinterpret_cast<uint8_t *>(&tempCount), sizeof(tempCount), tempAddr, sizeof(tempCount));
        tempCount = (tempCount >> ipStallSamplingReportShift) & stallSamplingReportCategoryMask;
        tempAddr += 1;
        return static_cast<uint8_t>(tempCount);
    };

    stallSumData->activeCount += getCount();
    stallSumData->otherCount += getCount();
    stallSumData->controlCount += getCount();
    stallSumData->pipeStallCount += getCount();
    stallSumData->sendCount += getCount();
    stallSumData->distAccCount += getCount();
    stallSumData->sbidCount += getCount();
    stallSumData->syncCount += getCount();
    stallSumData->instFetchCount += getCount();

#pragma pack(1)
    struct StallCntrInfo {
        uint16_t subslice;
        uint16_t flags;
    } stallCntrInfo = {};
#pragma pack()

    tempAddr = pRawIpData + stallSamplingReportSubSliceAndFlagsOffset;
    memcpy_s(reinterpret_cast<uint8_t *>(&stallCntrInfo), sizeof(stallCntrInfo), tempAddr, sizeof(stallCntrInfo));

    constexpr int32_t overflowDropFlag = (1 << 8);
    return stallCntrInfo.flags & overflowDropFlag;
}

// Order of ipDataValues must match stallSamplingReportList
template <>
void L0GfxCoreHelperHw<Family>::stallSumIpDataToTypedValues(uint64_t ip, void *sumIpData, std::vector<zet_typed_value_t> &ipDataValues) {
    StallSumIpData_t *stallSumData = reinterpret_cast<StallSumIpData_t *>(sumIpData);
    zet_typed_value_t tmpValueData;
    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = ip;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = stallSumData->activeCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = stallSumData->controlCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = stallSumData->pipeStallCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = stallSumData->sendCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = stallSumData->distAccCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = stallSumData->sbidCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = stallSumData->syncCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = stallSumData->instFetchCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = stallSumData->otherCount;
    ipDataValues.push_back(tmpValueData);
}

template <>
std::vector<std::pair<const char *, const char *>> L0GfxCoreHelperHw<Family>::getStallSamplingReportMetrics() const {
    std::vector<std::pair<const char *, const char *>> stallSamplingReportList = {
        {"Active", "Active cycles"},
        {"ControlStall", "Stall on control"},
        {"PipeStall", "Stall on pipe"},
        {"SendStall", "Stall on send"},
        {"DistStall", "Stall on distance"},
        {"SbidStall", "Stall on scoreboard"},
        {"SyncStall", "Stall on sync"},
        {"InstrFetchStall", "Stall on instruction fetch"},
        {"OtherStall", "Stall on other condition"},
    };
    return stallSamplingReportList;
}

template <>
uint64_t L0GfxCoreHelperHw<Family>::getIpSamplingIpMask() const {
    return ipSamplingIpMaskXe;
}

template <>
bool L0GfxCoreHelperHw<Family>::supportMetricsAggregation() const {
    return false;
}

template class L0GfxCoreHelperHw<Family>;

} // namespace L0
