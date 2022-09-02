/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/system_info.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/helpers/hw_info.h"

#include "drm/intel_hwconfig_types.h"

namespace NEO {

SystemInfo::SystemInfo(const std::vector<uint8_t> &inputData) {
    this->parseDeviceBlob(inputData);
}

void SystemInfo::parseDeviceBlob(const std::vector<uint8_t> &inputData) {
    auto data = reinterpret_cast<const uint32_t *>(inputData.data());
    auto dataSize = inputData.size() / sizeof(uint32_t);
    uint32_t i = 0;
    while (i + 2 < dataSize) {
        DEBUG_BREAK_IF(data[i + 1] < 1);

        /* Attribute IDs range */
        DEBUG_BREAK_IF(data[i] < 1);

        if (INTEL_HWCONFIG_MAX_SLICES_SUPPORTED == data[i]) {
            maxSlicesSupported = data[i + 2];
        }
        if (INTEL_HWCONFIG_MAX_DUAL_SUBSLICES_SUPPORTED == data[i]) {
            maxDualSubSlicesSupported = data[i + 2];
        }
        if (INTEL_HWCONFIG_MAX_NUM_EU_PER_DSS == data[i]) {
            maxEuPerDualSubSlice = data[i + 2];
        }
        if (INTEL_HWCONFIG_MAX_MEMORY_CHANNELS == data[i]) {
            maxMemoryChannels = data[i + 2];
        }
        if (INTEL_HWCONFIG_MEMORY_TYPE == data[i]) {
            memoryType = data[i + 2];
        }
        if (INTEL_HWCONFIG_NUM_THREADS_PER_EU == data[i]) {
            numThreadsPerEu = data[i + 2];
        }
        if (INTEL_HWCONFIG_TOTAL_VS_THREADS == data[i]) {
            totalVsThreads = data[i + 2];
        }
        if (INTEL_HWCONFIG_TOTAL_HS_THREADS == data[i]) {
            totalHsThreads = data[i + 2];
        }
        if (INTEL_HWCONFIG_TOTAL_DS_THREADS == data[i]) {
            totalDsThreads = data[i + 2];
        }
        if (INTEL_HWCONFIG_TOTAL_GS_THREADS == data[i]) {
            totalGsThreads = data[i + 2];
        }
        if (INTEL_HWCONFIG_TOTAL_PS_THREADS == data[i]) {
            totalPsThreads = data[i + 2];
        }
        if (INTEL_HWCONFIG_MAX_RCS == data[i]) {
            maxRCS = data[i + 2];
        }
        if (INTEL_HWCONFIG_MAX_CCS == data[i]) {
            maxCCS = data[i + 2];
        }
        if (INTEL_HWCONFIG_L3_BANK_SIZE_IN_KB == data[i]) {
            l3BankSizeInKb = data[i + 2];
        }
        /* Skip to next attribute */
        auto blobLength = 2 + data[i + 1];
        i += blobLength;
    }
}

} // namespace NEO
