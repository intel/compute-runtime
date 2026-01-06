/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_arg_helper.h"
#include "shared/source/helpers/product_config_helper.h"
#include "shared/source/xe3_core/hw_info_xe3_core.h"
#include "shared/source/xe3_core/nvls/device_ids_configs_nvls.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_offline_compiler.h"

#include "neo_aot_platforms.h"

using namespace NEO;
using ProductConfigHelperNvlsTests = ::testing::Test;

using NvlsOfflineCompilerTests = ::testing::Test;

NVLSTEST_F(NvlsOfflineCompilerTests, givenNvlUDeviceIdValueWhenInitHwInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    HardwareIpVersion nvlsConfig = AOT::NVL_U_A0;

    std::string deviceStr;
    for (const auto &deviceID : nvlUDeviceIds) {
        std::stringstream deviceIDStr, expectedOutput;

        deviceIDStr << "0x" << std::hex << deviceID;
        deviceStr = mockOfflineCompiler.argHelper->productConfigHelper->getAcronymForProductConfig(nvlsConfig.value);

        StreamCapture capture;
        capture.captureStdout();
        mockOfflineCompiler.initHardwareInfo(deviceIDStr.str());
        std::string output = capture.getCapturedStdout();
        expectedOutput << "Auto-detected target based on " << deviceIDStr.str() << " device id: " << deviceStr << "\n";

        EXPECT_STREQ(output.c_str(), expectedOutput.str().c_str());
        EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, deviceID);
        EXPECT_EQ(mockOfflineCompiler.deviceConfig, nvlsConfig.value);
    }
}

NVLSTEST_F(NvlsOfflineCompilerTests, givenNvlSDeviceIdValueWhenInitHwInfoThenCorrectValuesAreSet) {
    MockOfflineCompiler mockOfflineCompiler;
    HardwareIpVersion nvlsConfig = AOT::NVL_S_A0;

    std::string deviceStr;
    for (const auto &deviceID : nvlSDeviceIds) {
        std::stringstream deviceIDStr, expectedOutput;

        deviceIDStr << "0x" << std::hex << deviceID;
        deviceStr = mockOfflineCompiler.argHelper->productConfigHelper->getAcronymForProductConfig(nvlsConfig.value);

        StreamCapture capture;
        capture.captureStdout();
        mockOfflineCompiler.initHardwareInfo(deviceIDStr.str());
        std::string output = capture.getCapturedStdout();
        expectedOutput << "Auto-detected target based on " << deviceIDStr.str() << " device id: " << deviceStr << "\n";

        EXPECT_STREQ(output.c_str(), expectedOutput.str().c_str());
        EXPECT_EQ(mockOfflineCompiler.hwInfo.platform.usDeviceID, deviceID);
        EXPECT_EQ(mockOfflineCompiler.deviceConfig, nvlsConfig.value);
    }
}
