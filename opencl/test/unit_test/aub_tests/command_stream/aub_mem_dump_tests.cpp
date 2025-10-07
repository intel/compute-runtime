/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aub_mem_dump_tests.h"

#include "shared/source/aub/aub_helper.h"
#include "shared/test/common/mocks/mock_aub_csr.h"

std::string getAubFileName(const NEO::Device *pDevice, const std::string baseName) {
    const auto pGtSystemInfo = &pDevice->getHardwareInfo().gtSystemInfo;
    auto releaseHelper = pDevice->getReleaseHelper();
    std::stringstream strfilename;
    uint32_t subSlicesPerSlice = pGtSystemInfo->SubSliceCount / pGtSystemInfo->SliceCount;
    const auto deviceConfig = AubHelper::getDeviceConfigString(releaseHelper, 1, pGtSystemInfo->SliceCount, subSlicesPerSlice, pGtSystemInfo->MaxEuPerSubSlice);
    strfilename << hardwarePrefix[pDevice->getHardwareInfo().platform.eProductFamily] << "_" << deviceConfig << "_" << baseName;

    return strfilename.str();
}
