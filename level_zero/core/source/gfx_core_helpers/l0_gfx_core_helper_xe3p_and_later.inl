/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

namespace L0 {

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

#pragma pack(1)
typedef struct StallSumIpDataXe3pCore {
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
} StallSumIpDataXe3pCore_t;
#pragma pack()

constexpr uint64_t ipSamplingIpMaskXe3p = 0x1fffffffffffffff;
constexpr uint32_t ipSamplingMetricCountXe3p = 11u;

template <typename Family>
uint32_t L0GfxCoreHelperHw<Family>::getIpSamplingMetricCount() {
    return ipSamplingMetricCountXe3p;
}

template <typename Family>
void L0GfxCoreHelperHw<Family>::stallIpDataMapDeleteSumData(std::map<uint64_t, void *> &stallSumIpDataMap) {
    for (auto i = stallSumIpDataMap.begin(); i != stallSumIpDataMap.end(); i++) {
        StallSumIpDataXe3pCore_t *stallSumData = reinterpret_cast<StallSumIpDataXe3pCore_t *>(i->second);
        if (stallSumData) {
            delete stallSumData;
            i->second = nullptr;
        }
    }
}

template <typename Family>
void L0GfxCoreHelperHw<Family>::stallIpDataMapDeleteSumDataEntry(std::map<uint64_t, void *>::iterator it) {
    StallSumIpDataXe3pCore_t *stallSumData = reinterpret_cast<StallSumIpDataXe3pCore_t *>(it->second);
    if (stallSumData) {
        delete stallSumData;
        it->second = nullptr;
    }
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::stallIpDataMapUpdateFromData(const uint8_t *pRawIpData, std::map<uint64_t, void *> &stallSumIpDataMap) {
    constexpr int ipStallSamplingOffset = 7;              // Offset to read the first Stall Sampling report after IP Address.
    constexpr int ipStallSamplingReportShift = 5;         // Shift in bits required to read the stall sampling report data due to the IP address [0-60] bits to access the next report category data.
    constexpr int stallSamplingReportCategoryMask = 0xff; // Mask for Stall Sampling Report Category.

    const uint8_t *tempAddr = pRawIpData;
    uint64_t ip = 0ULL;
    memcpy_s(reinterpret_cast<uint8_t *>(&ip), sizeof(ip), tempAddr, sizeof(ip));
    ip &= ipSamplingIpMaskXe3p;
    StallSumIpDataXe3pCore_t *stallSumData = nullptr;
    if (stallSumIpDataMap.count(ip) == 0) {
        stallSumData = new StallSumIpDataXe3pCore_t{};
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
        stallSumData = reinterpret_cast<StallSumIpDataXe3pCore_t *>(stallSumIpDataMap[ip]);
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

template <typename Family>
void L0GfxCoreHelperHw<Family>::stallIpDataMapUpdateFromMap(std::map<uint64_t, void *> &sourceMap, std::map<uint64_t, void *> &stallSumIpDataMap) {

    for (auto &entry : sourceMap) {
        uint64_t ip = entry.first;
        StallSumIpDataXe3pCore_t *sourceData = reinterpret_cast<StallSumIpDataXe3pCore_t *>(entry.second);
        if (stallSumIpDataMap.count(ip) == 0) {
            StallSumIpDataXe3pCore_t *newData = new StallSumIpDataXe3pCore_t{};
            memcpy_s(newData, sizeof(StallSumIpDataXe3pCore_t), sourceData, sizeof(StallSumIpDataXe3pCore_t));
            stallSumIpDataMap[ip] = newData;
        } else {
            StallSumIpDataXe3pCore_t *destData = reinterpret_cast<StallSumIpDataXe3pCore_t *>(stallSumIpDataMap[ip]);
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

// Order of ipDataValues must match stallSamplingReportList
template <typename Family>
void L0GfxCoreHelperHw<Family>::stallSumIpDataToTypedValues(uint64_t ip, void *sumIpData, std::vector<zet_typed_value_t> &ipDataValues) {
    StallSumIpDataXe3pCore_t *stallSumData = reinterpret_cast<StallSumIpDataXe3pCore_t *>(sumIpData);
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
        {"PSDepStall", "Stall on Pixel Shader Order Dependency"},
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
    return ipSamplingIpMaskXe3p;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::supportMetricsAggregation() const {
    return false;
}

} // namespace L0
