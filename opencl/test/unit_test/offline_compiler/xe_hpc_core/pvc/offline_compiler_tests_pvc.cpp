/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/xe_hpc_core/pvc/product_configs_pvc.h"

#include "opencl/test/unit_test/offline_compiler/ocloc_fatbinary_tests.h"
#include "opencl/test/unit_test/offline_compiler/ocloc_product_config_tests.h"

namespace NEO {
INSTANTIATE_TEST_SUITE_P(
    OclocProductConfigPvcTestsValues,
    OclocProductConfigTests,
    ::testing::Combine(
        ::testing::ValuesIn(AOT_PVC::productConfigs),
        ::testing::Values(IGFX_PVC)));

INSTANTIATE_TEST_SUITE_P(
    OclocFatbinaryPvcTests,
    OclocFatbinaryPerProductTests,
    ::testing::Combine(
        ::testing::Values("xe-hpc"),
        ::testing::Values(IGFX_PVC)));

using PvcOfflineCompilerTests = ::testing::Test;
PVCTEST_F(PvcOfflineCompilerTests, givenPvcDeviceAndRevisionIdValueWhenInitHwInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    std::vector<std::pair<HardwareIpVersion, int>> pvcConfigs = {
        {AOT::PVC_XL_A0, 0x0},
        {AOT::PVC_XL_A0P, 0x1},
        {AOT::PVC_XT_A0, 0x3},
        {AOT::PVC_XT_B1, 0x26},
        {AOT::PVC_XT_C0, 0x2f}};

    std::string deviceStr;
    auto deviceID = pvcXtDeviceIds.front();

    for (const auto &[config, revisionID] : pvcConfigs) {
        std::stringstream deviceIDStr, expectedOutput;
        mockOfflineCompiler.revisionId = revisionID;

        deviceIDStr << "0x" << std::hex << deviceID;
        deviceStr = mockOfflineCompiler.argHelper->productConfigHelper->getAcronymForProductConfig(config.value);

        StreamCapture capture;
        capture.captureStdout();
        mockOfflineCompiler.initHardwareInfo(deviceIDStr.str());
        std::string output = capture.getCapturedStdout();
        expectedOutput << "Auto-detected target based on " << deviceIDStr.str() << " device id: " << deviceStr << "\n";

        EXPECT_STREQ(output.c_str(), expectedOutput.str().c_str());
        EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, deviceID);
        EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, revisionID);
        EXPECT_EQ(mockOfflineCompiler.deviceConfig, config.value);
    }
}

PVCTEST_F(PvcOfflineCompilerTests, givenPvcXtVgDeviceAndRevisionIdValueWhenInitHwInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;

    std::string deviceStr;
    auto deviceID = pvcXtVgDeviceIds.front();

    HardwareIpVersion config = AOT::PVC_XT_C0_VG;
    int revisionID = 0x2f;

    std::stringstream deviceIDStr, expectedOutput;
    mockOfflineCompiler.revisionId = revisionID;

    deviceIDStr << "0x" << std::hex << deviceID;
    deviceStr = mockOfflineCompiler.argHelper->productConfigHelper->getAcronymForProductConfig(config.value);

    StreamCapture capture;
    capture.captureStdout();
    mockOfflineCompiler.initHardwareInfo(deviceIDStr.str());
    std::string output = capture.getCapturedStdout();
    expectedOutput << "Auto-detected target based on " << deviceIDStr.str() << " device id: " << deviceStr << "\n";

    EXPECT_STREQ(output.c_str(), expectedOutput.str().c_str());
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, deviceID);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, revisionID);
    EXPECT_EQ(mockOfflineCompiler.deviceConfig, config.value);
}

PVCTEST_F(PvcOfflineCompilerTests, givenPvcXtDeviceAndUnknownRevisionIdValueWhenInitHwInfoThenDefaultConfigIsSet) {
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.argHelper->getPrinterRef().setSuppressMessages(true);
    std::string deviceStr;
    auto deviceID = pvcXtDeviceIds.front();
    auto pvcXtConfig = AOT::PVC_XT_C0;

    std::stringstream deviceIDStr, expectedOutput;
    mockOfflineCompiler.revisionId = CommonConstants::invalidRevisionID;
    deviceIDStr << "0x" << std::hex << deviceID;

    mockOfflineCompiler.initHardwareInfo(deviceIDStr.str());

    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, deviceID);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, CommonConstants::invalidRevisionID);
    EXPECT_EQ(mockOfflineCompiler.deviceConfig, pvcXtConfig);
}

} // namespace NEO