/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/system_info.h"

#include "shared/source/helpers/debug_helpers.h"

namespace NEO {

SystemInfo::SystemInfo(const std::vector<uint32_t> &inputData) {
    this->parseDeviceBlob(inputData);
}

void SystemInfo::parseDeviceBlob(const std::vector<uint32_t> &inputData) {
    auto data = inputData.data();
    auto dataSize = inputData.size();
    uint32_t i = 0;
    while (i + 2 < dataSize) {
        DEBUG_BREAK_IF(data[i + 1] < 1);

        /* Attribute IDs range */
        DEBUG_BREAK_IF(data[i] < 1);

        if (DeviceBlobConstants::maxSlicesSupported == data[i]) {
            maxSlicesSupported = data[i + 2];
        }
        if (DeviceBlobConstants::maxDualSubSlicesSupported == data[i]) {
            maxDualSubSlicesSupported = std::max(data[i + 2], maxDualSubSlicesSupported);
        }
        if (DeviceBlobConstants::maxEuPerDualSubSlice == data[i]) {
            maxEuPerDualSubSlice = std::max(data[i + 2], maxEuPerDualSubSlice);
        }
        if (DeviceBlobConstants::maxMemoryChannels == data[i]) {
            maxMemoryChannels = data[i + 2];
        }
        if (DeviceBlobConstants::memoryType == data[i]) {
            memoryType = data[i + 2];
        }
        if (DeviceBlobConstants::numThreadsPerEu == data[i]) {
            numThreadsPerEu = data[i + 2];
        }
        if (DeviceBlobConstants::maxRcs == data[i]) {
            maxRCS = data[i + 2];
        }
        if (DeviceBlobConstants::maxCcs == data[i]) {
            maxCCS = data[i + 2];
        }
        if (DeviceBlobConstants::l3BankSizeInKb == data[i]) {
            l3BankSizeInKb = data[i + 2];
        }
        if (DeviceBlobConstants::maxSubSlicesSupported == data[i]) {
            maxDualSubSlicesSupported = std::max(data[i + 2], maxDualSubSlicesSupported);
        }
        if (DeviceBlobConstants::maxEuPerSubSlice == data[i]) {
            maxEuPerDualSubSlice = std::max(data[i + 2], maxEuPerDualSubSlice);
        }
        if (DeviceBlobConstants::csrSizeInMb == data[i]) {
            csrSizeInMb = data[i + 2];
        }
        if (DeviceBlobConstants::slmSizePerDss == data[i]) {
            slmSizePerDss = std::max(data[i + 2], slmSizePerDss);
        }
        if (DeviceBlobConstants::slmSizePerSs == data[i]) {
            slmSizePerDss = std::max(data[i + 2], slmSizePerDss);
        }
        if (DeviceBlobConstants::numHbmStacksPerTile == data[i]) {
            numHbmStacksPerTile = data[i + 2];
        }
        if (DeviceBlobConstants::numChannelsPerHbmStack == data[i]) {
            numChannelsPerHbmStack = data[i + 2];
        }
        if (DeviceBlobConstants::numRegions == data[i]) {
            numRegions = data[i + 2];
        }
        if (DeviceBlobConstants::numL3BanksPerGroup == data[i]) {
            numL3BanksPerGroup = data[i + 2];
        }
        if (DeviceBlobConstants::numL3BankGroups == data[i]) {
            numL3BankGroups = data[i + 2];
        }
        /* Skip to next attribute */
        auto blobLength = 2 + data[i + 1];
        i += blobLength;
    }
}

} // namespace NEO
