/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/system_info_impl.h"
namespace NEO {

SystemInfoImpl::SystemInfoImpl(const uint32_t *blobData, int32_t blobSize) {
    this->parseDeviceBlob(blobData, blobSize);
}

void SystemInfoImpl::parseDeviceBlob(const uint32_t *data, int32_t size) {

    uint32_t i = 0;
    while (i < (size / sizeof(uint32_t))) {
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
        if (INTEL_HWCONFIG_L3_CACHE_SIZE_IN_KB == data[i]) {
            L3CacheSizeInKb = data[i + 2];
        }
        if (INTEL_HWCONFIG_L3_BANK_COUNT == data[i]) {
            L3BankCount = data[i + 2];
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
        if (INTEL_HWCONFIG_MAX_FILL_RATE == data[i]) {
            maxFillRate = data[i + 2];
        }
        if (INTEL_HWCONFIG_MAX_RCS == data[i]) {
            maxRCS = data[i + 2];
        }
        if (INTEL_HWCONFIG_MAX_CCS == data[i]) {
            maxCCS = data[i + 2];
        }
        /* Skip to next attribute */
        auto blobLength = 2 + data[i + 1];
        i += blobLength;
    }
}

} // namespace NEO