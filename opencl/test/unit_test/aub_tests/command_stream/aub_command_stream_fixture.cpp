/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/aub_tests/command_stream/aub_command_stream_fixture.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/tbx_command_stream_receiver.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/memory_management.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/unit_test/tests_configuration.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"

#include "gtest/gtest.h"

namespace NEO {

void AUBCommandStreamFixture::SetUp(CommandQueue *pCmdQ) {
    ASSERT_NE(pCmdQ, nullptr);
    auto &device = reinterpret_cast<MockDevice &>(pCmdQ->getDevice());
    const auto &hwInfo = device.getHardwareInfo();
    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);

    const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    std::stringstream strfilename;
    auto engineType = pCmdQ->getGpgpuCommandStreamReceiver().getOsContext().getEngineType();

    strfilename << ApiSpecificConfig::getAubPrefixForSpecificApi();
    strfilename << testInfo->test_case_name() << "_" << testInfo->name() << "_" << hwHelper.getCsTraits(engineType).name;

    pCommandStreamReceiver = AUBFixture::prepareComputeEngine(device, strfilename.str());
    ASSERT_NE(nullptr, pCommandStreamReceiver);

    AUBFixture::prepareCopyEngines(device, strfilename.str());

    CommandStreamFixture::SetUp(pCmdQ);

    pTagMemory = pCommandStreamReceiver->getTagAddress();
    this->commandQueue = pCmdQ;
}

void AUBCommandStreamFixture::TearDown() {
    CommandStreamFixture::TearDown();
}
} // namespace NEO
