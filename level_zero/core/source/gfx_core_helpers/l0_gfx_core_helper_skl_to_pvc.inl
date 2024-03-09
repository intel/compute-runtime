/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

#include <map>

namespace L0 {

template <typename Family>
bool L0GfxCoreHelperHw<Family>::imageCompressionSupported(const NEO::HardwareInfo &hwInfo) const {
    if (NEO::debugManager.flags.RenderCompressedImagesEnabled.get() != -1) {
        return !!NEO::debugManager.flags.RenderCompressedImagesEnabled.get();
    }

    return false;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::usmCompressionSupported(const NEO::HardwareInfo &hwInfo) const {
    if (NEO::debugManager.flags.RenderCompressedBuffersEnabled.get() != -1) {
        return !!NEO::debugManager.flags.RenderCompressedBuffersEnabled.get();
    }

    return false;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::forceDefaultUsmCompressionSupport() const {
    return false;
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
 * bytes 49 and 50, subSlice
 * bytes 51 and 52, flags
 *
 * total size 64 bytes
 */
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

template <typename Family>
void L0GfxCoreHelperHw<Family>::stallIpDataMapDelete(std::map<uint64_t, void *> &stallSumIpDataMap) {
    for (auto i = stallSumIpDataMap.begin(); i != stallSumIpDataMap.end(); i++) {
        StallSumIpData_t *stallSumData = reinterpret_cast<StallSumIpData_t *>(i->second);
        if (stallSumData) {
            delete stallSumData;
        }
    }
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::stallIpDataMapUpdate(std::map<uint64_t, void *> &stallSumIpDataMap, const uint8_t *pRawIpData) {
    const uint8_t *tempAddr = pRawIpData;
    uint64_t ip = 0ULL;
    memcpy_s(reinterpret_cast<uint8_t *>(&ip), sizeof(ip), tempAddr, sizeof(ip));
    ip &= 0x1fffffff;
    StallSumIpData_t *stallSumData = nullptr;
    if (stallSumIpDataMap.count(ip) == 0) {
        stallSumData = new StallSumIpData_t{};
        stallSumIpDataMap[ip] = stallSumData;
    } else {
        stallSumData = reinterpret_cast<StallSumIpData_t *>(stallSumIpDataMap[ip]);
    }
    tempAddr += 3;

    auto getCount = [&tempAddr]() {
        uint16_t tempCount = 0;
        memcpy_s(reinterpret_cast<uint8_t *>(&tempCount), sizeof(tempCount), tempAddr, sizeof(tempCount));
        tempCount = (tempCount >> 5) & 0xff;
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

    struct StallCntrInfo {
        uint16_t subslice;
        uint16_t flags;
    } stallCntrInfo = {};

    tempAddr = pRawIpData + 48;
    memcpy_s(reinterpret_cast<uint8_t *>(&stallCntrInfo), sizeof(stallCntrInfo), tempAddr, sizeof(stallCntrInfo));

    constexpr int overflowDropFlag = (1 << 8);
    return stallCntrInfo.flags & overflowDropFlag;
}

// The order of push_back calls must match the order of stallSamplingReportList.
template <typename Family>
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

template <typename Family>
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

} // namespace L0
