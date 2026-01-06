/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"

#include "opencl/test/unit_test/offline_compiler/ocloc_fatbinary_tests.h"
#include "opencl/test/unit_test/offline_compiler/ocloc_product_config_tests.h"

#include "neo_aot_platforms.h"
namespace NEO {

inline constexpr AOT::PRODUCT_CONFIG productConfigs[] = {
    AOT::DG2_G10_A0,
    AOT::DG2_G10_A1,
    AOT::DG2_G10_B0,
    AOT::DG2_G10_C0,
    AOT::DG2_G11_A0,
    AOT::DG2_G11_B0,
    AOT::DG2_G11_B1,
    AOT::DG2_G12_A0};
INSTANTIATE_TEST_SUITE_P(
    OclocProductConfigDg2TestsValues,
    OclocProductConfigTests,
    ::testing::Combine(
        ::testing::ValuesIn(productConfigs),
        ::testing::Values(IGFX_DG2)));

INSTANTIATE_TEST_SUITE_P(
    OclocFatbinaryDg2Tests,
    OclocFatbinaryPerProductTests,
    ::testing::Combine(
        ::testing::Values("xe-hpg"),
        ::testing::Values(IGFX_DG2)));

using Dg2OfflineCompilerTests = ::testing::Test;
DG2TEST_F(Dg2OfflineCompilerTests, givenDg2G10DeviceAndRevisionIdValueWhenInitHwInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    std::vector<std::pair<HardwareIpVersion, int>> dg2G10Configs = {
        {AOT::DG2_G10_A0, revIdA0},
        {AOT::DG2_G10_A1, revIdA1}};

    std::string deviceStr;
    auto deviceID = dg2G10DeviceIds.front();

    for (const auto &[config, revisionID] : dg2G10Configs) {
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
        EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, config.revision);
        EXPECT_EQ(mockOfflineCompiler.deviceConfig, config.value);
    }
}

DG2TEST_F(Dg2OfflineCompilerTests, givenDg2G11DeviceAndRevisionIdValueWhenInitHwInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    std::vector<std::pair<HardwareIpVersion, int>> dg2G11Configs = {
        {AOT::DG2_G11_A0, revIdA0},
        {AOT::DG2_G11_B0, revIdB0},
        {AOT::DG2_G11_B1, revIdB1}};

    std::string deviceStr;
    auto deviceID = dg2G11DeviceIds.front();

    for (const auto &[config, revisionID] : dg2G11Configs) {
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
        EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, config.revision);
        EXPECT_EQ(mockOfflineCompiler.deviceConfig, config.value);
    }
}

DG2TEST_F(Dg2OfflineCompilerTests, givenDg2G12DeviceAndRevisionIdValueWhenInitHwInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    HardwareIpVersion dg2G12Config = {AOT::DG2_G12_A0};
    std::string deviceStr;
    std::stringstream deviceIDStr, expectedOutput;
    auto deviceID = dg2G12DeviceIds.front();

    mockOfflineCompiler.revisionId = revIdA0;

    deviceIDStr << "0x" << std::hex << deviceID;
    deviceStr = mockOfflineCompiler.argHelper->productConfigHelper->getAcronymForProductConfig(dg2G12Config.value);

    StreamCapture capture;
    capture.captureStdout();
    mockOfflineCompiler.initHardwareInfo(deviceIDStr.str());
    std::string output = capture.getCapturedStdout();
    expectedOutput << "Auto-detected target based on " << deviceIDStr.str() << " device id: " << deviceStr << "\n";

    EXPECT_STREQ(output.c_str(), expectedOutput.str().c_str());
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, deviceID);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, dg2G12Config.revision);
    EXPECT_EQ(mockOfflineCompiler.deviceConfig, dg2G12Config.value);
}

TEST_F(Dg2OfflineCompilerTests, givenDg2G10DeviceAndUnknownRevisionIdValueWhenInitHwInfoThenDefaultConfigIsSet) {
    MockOfflineCompiler mockOfflineCompiler;
    mockOfflineCompiler.argHelper->getPrinterRef().setSuppressMessages(true);
    std::string deviceStr;
    auto deviceID = dg2G10DeviceIds.front();
    auto dg2G10Config = AOT::DG2_G10_A0;

    std::stringstream deviceIDStr, expectedOutput;
    mockOfflineCompiler.revisionId = CommonConstants::invalidRevisionID;
    deviceIDStr << "0x" << std::hex << deviceID;

    mockOfflineCompiler.initHardwareInfo(deviceIDStr.str());

    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, deviceID);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, CommonConstants::invalidRevisionID);
    EXPECT_EQ(mockOfflineCompiler.deviceConfig, dg2G10Config);
}

DG2TEST_F(Dg2OfflineCompilerTests, givenDg2DeprecatedAcronymAndRevisionIdValueWhenInitHwInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    std::vector<std::pair<HardwareIpVersion, int>> dg2G10Configs = {
        {AOT::DG2_G10_A0, revIdA0},
        {AOT::DG2_G10_A1, revIdA1},
        {AOT::DG2_G10_B0, revIdB0},
        {AOT::DG2_G10_C0, revIdC0}};

    std::string deprecatedAcronym = NEO::hardwarePrefix[IGFX_DG2];

    for (const auto &[config, revisionID] : dg2G10Configs) {
        mockOfflineCompiler.revisionId = revisionID;
        mockOfflineCompiler.initHardwareInfo(deprecatedAcronym);

        EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, revisionID);
        EXPECT_EQ(mockOfflineCompiler.hwInfo.ipVersion.value, config.value);
    }
}

DG2TEST_F(Dg2OfflineCompilerTests, givenDg2DeprecatedAcronymAndInvalisRevisionIdValueWhenInitHwInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    std::string deprecatedAcronym = NEO::hardwarePrefix[IGFX_DG2];

    mockOfflineCompiler.revisionId = CommonConstants::invalidRevisionID;
    mockOfflineCompiler.initHardwareInfo(deprecatedAcronym);

    EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usRevId, CommonConstants::invalidRevisionID);
    EXPECT_EQ(mockOfflineCompiler.hwInfo.ipVersion.value, AOT::DG2_G10_C0);
}

} // namespace NEO
