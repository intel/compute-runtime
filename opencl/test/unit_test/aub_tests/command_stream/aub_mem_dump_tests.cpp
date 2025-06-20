/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aub_mem_dump_tests.h"

#include "shared/source/aub/aub_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_aub_csr.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"

using NEO::ApiSpecificConfig;
using NEO::AUBCommandStreamReceiver;
using NEO::AUBCommandStreamReceiverHw;
using NEO::AUBFamilyMapper;
using NEO::ClDeviceFixture;
using NEO::folderAUB;

std::string getAubFileName(const NEO::Device *pDevice, const std::string baseName) {
    const auto pGtSystemInfo = &pDevice->getHardwareInfo().gtSystemInfo;
    auto releaseHelper = pDevice->getReleaseHelper();
    std::stringstream strfilename;
    uint32_t subSlicesPerSlice = pGtSystemInfo->SubSliceCount / pGtSystemInfo->SliceCount;
    const auto deviceConfig = AubHelper::getDeviceConfigString(releaseHelper, 1, pGtSystemInfo->SliceCount, subSlicesPerSlice, pGtSystemInfo->MaxEuPerSubSlice);
    strfilename << hardwarePrefix[pDevice->getHardwareInfo().platform.eProductFamily] << "_" << deviceConfig << "_" << baseName;

    return strfilename.str();
}

TEST(PageTableTraits, when48BitTraitsAreUsedThenPageTableAddressesAreCorrect) {
    EXPECT_EQ(BIT(32), AubMemDump::PageTableTraits<48>::ptBaseAddress);
    EXPECT_EQ(BIT(31), AubMemDump::PageTableTraits<48>::pdBaseAddress);
    EXPECT_EQ(BIT(30), AubMemDump::PageTableTraits<48>::pdpBaseAddress);
    EXPECT_EQ(BIT(29), AubMemDump::PageTableTraits<48>::pml4BaseAddress);
}

TEST(PageTableTraits, when32BitTraitsAreUsedThenPageTableAddressesAreCorrect) {
    EXPECT_EQ(BIT(38), AubMemDump::PageTableTraits<32>::ptBaseAddress);
    EXPECT_EQ(BIT(37), AubMemDump::PageTableTraits<32>::pdBaseAddress);
    EXPECT_EQ(BIT(36), AubMemDump::PageTableTraits<32>::pdpBaseAddress);
}
