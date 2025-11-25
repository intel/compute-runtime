/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "neo_aot_platforms.h"

#include <string>

using namespace NEO;

using AubFileStreamTestsCri = ::testing::Test;

CRITEST_F(AubFileStreamTestsCri, givenDeviceWithXecuWhenCreateFullFilePathIsCalledThenFileNameIsCorrect) {
    DebugManagerStateRestore stateRestore;
    auto hwInfo = *defaultHwInfo;
    const uint32_t rootDeviceIndex = 0u;

    auto setupConfig = [&hwInfo](uint32_t tileCount, uint32_t sliceCount, uint32_t subSliceCount, uint32_t euPerSubSliceCount) {
        debugManager.flags.CreateMultipleSubDevices.set(tileCount);
        hwInfo.gtSystemInfo.SliceCount = sliceCount;
        hwInfo.gtSystemInfo.SubSliceCount = sliceCount * subSliceCount;
        hwInfo.gtSystemInfo.MaxEuPerSubSlice = euPerSubSliceCount;
    };

    setupConfig(1, 1, 1, 1);
    auto fullName = AUBCommandStreamReceiver::createFullFilePath(hwInfo, "aubfile", rootDeviceIndex);
    EXPECT_NE(std::string::npos, fullName.find("1tx1x1x1x1"));

    setupConfig(1, 3, 2, 4);
    fullName = AUBCommandStreamReceiver::createFullFilePath(hwInfo, "aubfile", rootDeviceIndex);
    EXPECT_NE(std::string::npos, fullName.find("1tx1x3x2x4"));

    setupConfig(12, 8, 6, 3);
    fullName = AUBCommandStreamReceiver::createFullFilePath(hwInfo, "aubfile", rootDeviceIndex);
    EXPECT_NE(std::string::npos, fullName.find("12tx2x4x6x3"));

    setupConfig(8, 8, 8, 8);
    fullName = AUBCommandStreamReceiver::createFullFilePath(hwInfo, "aubfile", rootDeviceIndex);
    EXPECT_NE(std::string::npos, fullName.find("8tx2x4x8x8"));

    setupConfig(16, 16, 16, 16);
    fullName = AUBCommandStreamReceiver::createFullFilePath(hwInfo, "aubfile", rootDeviceIndex);
    EXPECT_NE(std::string::npos, fullName.find("16tx4x4x16x16"));

    setupConfig(16, 10, 16, 16);
    EXPECT_ANY_THROW(AUBCommandStreamReceiver::createFullFilePath(hwInfo, "aubfile", rootDeviceIndex));
}
