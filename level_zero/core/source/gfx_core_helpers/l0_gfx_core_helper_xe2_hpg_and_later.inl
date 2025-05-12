/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

namespace L0 {

template <typename Family>
bool L0GfxCoreHelperHw<Family>::imageCompressionSupported(const NEO::HardwareInfo &hwInfo) const {
    if (NEO::debugManager.flags.RenderCompressedImagesEnabled.get() != -1) {
        return !!NEO::debugManager.flags.RenderCompressedImagesEnabled.get();
    }

    return hwInfo.capabilityTable.ftrRenderCompressedImages;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const {
    if (NEO::debugManager.flags.RenderCompressedBuffersEnabled.get() != -1) {
        return !!NEO::debugManager.flags.RenderCompressedBuffersEnabled.get();
    }

    return hwInfo.capabilityTable.ftrRenderCompressedBuffers;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::forceDefaultUsmCompressionSupport() const {
    return true;
}

/*
 * Xe2 stall sample data item format:
 *
 * Bits		Field
 * 0  to 28	IP (addr)
 * 29 to 36	tdr count
 * 37 to 44	other count
 * 45 to 52	control count
 * 53 to 60	pipestall count
 * 61 to 68	send count
 * 69 to 76	dist_acc count
 * 77 to 84	sbid count
 * 85 to 92	sync count
 * 93 to 100 inst_fetch count
 * 101 to 108 active count
 * Exid	[109, 111]
 * "EndFlag" - is always "1" 112
 *
 * bytes at index 48 and 49, subSlice
 * bytes at index 50 and 51, flags
 *
 * total size 64 bytes
 */

#pragma pack(1)
typedef struct StallSumIpDataXeCore {
    uint64_t tdrCount;
    uint64_t otherCount;
    uint64_t controlCount;
    uint64_t pipeStallCount;
    uint64_t sendCount;
    uint64_t distAccCount;
    uint64_t sbidCount;
    uint64_t syncCount;
    uint64_t instFetchCount;
    uint64_t activeCount;
} StallSumIpDataXeCore_t;
#pragma pack()

constexpr uint32_t ipSamplingMetricCountXe2 = 11u;
constexpr uint64_t ipSamplingIpMaskXe2 = 0x1fffffff;

template <typename Family>
uint32_t L0GfxCoreHelperHw<Family>::getIpSamplingMetricCount() {
    return ipSamplingMetricCountXe2;
}

template <typename Family>
void L0GfxCoreHelperHw<Family>::stallIpDataMapDelete(std::map<uint64_t, void *> &stallSumIpDataMap) {
    for (auto i = stallSumIpDataMap.begin(); i != stallSumIpDataMap.end(); i++) {
        StallSumIpDataXeCore_t *stallSumData = reinterpret_cast<StallSumIpDataXeCore_t *>(i->second);
        if (stallSumData) {
            delete stallSumData;
            i->second = nullptr;
        }
    }
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::stallIpDataMapUpdate(std::map<uint64_t, void *> &stallSumIpDataMap, const uint8_t *pRawIpData) {
    constexpr int ipStallSamplingOffset = 3;              // Offset to read the first Stall Sampling report after IP Address.
    constexpr int ipStallSamplingReportShift = 5;         // Shift in bits required to read the stall sampling report data due to the IP address [0-28] bits to access the next report category data.
    constexpr int stallSamplingReportCategoryMask = 0xff; // Mask for Stall Sampling Report Category.

    const uint8_t *tempAddr = pRawIpData;
    uint64_t ip = 0ULL;
    memcpy_s(reinterpret_cast<uint8_t *>(&ip), sizeof(ip), tempAddr, sizeof(ip));
    ip &= ipSamplingIpMaskXe2;
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

// Order of ipDataValues must match stallSamplingReportList
template <typename Family>
void L0GfxCoreHelperHw<Family>::stallSumIpDataToTypedValues(uint64_t ip, void *sumIpData, std::vector<zet_typed_value_t> &ipDataValues) {
    StallSumIpDataXeCore_t *stallSumData = reinterpret_cast<StallSumIpDataXeCore_t *>(sumIpData);
    zet_typed_value_t tmpValueData;
    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = ip;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = stallSumData->activeCount;
    ipDataValues.push_back(tmpValueData);

    tmpValueData.type = ZET_VALUE_TYPE_UINT64;
    tmpValueData.value.ui64 = stallSumData->tdrCount;
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

template <typename Family>
std::vector<std::pair<const char *, const char *>> L0GfxCoreHelperHw<Family>::getStallSamplingReportMetrics() const {
    std::vector<std::pair<const char *, const char *>> stallSamplingReportList = {
        {"Active", "Active cycles"},
        {"Tdr", "Stall on Timeout Detection and Recovery"},
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

template <typename Family>
uint64_t L0GfxCoreHelperHw<Family>::getIpSamplingIpMask() const {
    return ipSamplingIpMaskXe2;
}

template <typename Family>
uint64_t L0GfxCoreHelperHw<Family>::getOaTimestampValidBits() const {
    constexpr uint64_t oaTimestampValidBits = 56u;
    return oaTimestampValidBits;
};

} // namespace L0
