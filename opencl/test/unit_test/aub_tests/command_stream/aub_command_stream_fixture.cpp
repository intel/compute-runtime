/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/command_stream/aub_command_stream_fixture.h"

#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/tbx_command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"

#include "gtest/gtest.h"

namespace NEO {

CommandStreamReceiver *AUBCommandStreamFixture::prepareComputeEngine(MockDevice &device, const std::string &filename) {
    CommandStreamReceiver *pCommandStreamReceiver = nullptr;
    if (testMode == TestMode::AubTestsWithTbx) {
        pCommandStreamReceiver = TbxCommandStreamReceiver::create(filename, true, *device.executionEnvironment, device.getRootDeviceIndex(), device.getDeviceBitfield());
    } else {
        pCommandStreamReceiver = AUBCommandStreamReceiver::create(filename, true, *device.executionEnvironment, device.getRootDeviceIndex(), device.getDeviceBitfield());
    }
    device.resetCommandStreamReceiver(pCommandStreamReceiver);
    return pCommandStreamReceiver;
}

void AUBCommandStreamFixture::prepareCopyEngines(MockDevice &device, const std::string &filename) {
    for (auto i = 0u; i < device.allEngines.size(); i++) {
        if (EngineHelpers::isBcs(device.allEngines[i].getEngineType())) {
            CommandStreamReceiver *pBcsCommandStreamReceiver = nullptr;
            if (testMode == TestMode::AubTestsWithTbx) {
                pBcsCommandStreamReceiver = TbxCommandStreamReceiver::create(filename, true, *device.executionEnvironment, device.getRootDeviceIndex(), device.getDeviceBitfield());
            } else {
                pBcsCommandStreamReceiver = AUBCommandStreamReceiver::create(filename, true, *device.executionEnvironment, device.getRootDeviceIndex(), device.getDeviceBitfield());
            }
            device.resetCommandStreamReceiver(pBcsCommandStreamReceiver, i);
        }
    }
}

void AUBCommandStreamFixture::setUp(CommandQueue *pCmdQ) {
    ASSERT_NE(pCmdQ, nullptr);
    auto &device = reinterpret_cast<MockDevice &>(pCmdQ->getDevice());
    auto &gfxCoreHelper = device.getGfxCoreHelper();

    const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    std::stringstream strfilename;
    auto engineType = pCmdQ->getGpgpuCommandStreamReceiver().getOsContext().getEngineType();

    strfilename << ApiSpecificConfig::getAubPrefixForSpecificApi();
    strfilename << testInfo->test_case_name() << "_" << testInfo->name() << "_" << gfxCoreHelper.getCsTraits(engineType).name;

    pCommandStreamReceiver = prepareComputeEngine(device, strfilename.str());
    ASSERT_NE(nullptr, pCommandStreamReceiver);

    prepareCopyEngines(device, strfilename.str());

    CommandStreamFixture::setUp(pCmdQ);

    pTagMemory = pCommandStreamReceiver->getTagAddress();
    this->commandQueue = pCmdQ;
}

void AUBCommandStreamFixture::tearDown() {
    CommandStreamFixture::tearDown();
}
} // namespace NEO
