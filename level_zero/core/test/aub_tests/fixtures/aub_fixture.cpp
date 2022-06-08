/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"

#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/unit_test/tests_configuration.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"
#include "test_mode.h"

namespace L0 {
AUBFixtureL0::AUBFixtureL0() = default;
AUBFixtureL0::~AUBFixtureL0() = default;
void AUBFixtureL0::prepareCopyEngines(NEO::MockDevice &device, const std::string &filename) {
    for (auto i = 0u; i < device.allEngines.size(); i++) {
        if (NEO::EngineHelpers::isBcs(device.allEngines[i].getEngineType())) {
            NEO::CommandStreamReceiver *pBcsCommandStreamReceiver = nullptr;
            pBcsCommandStreamReceiver = NEO::AUBCommandStreamReceiver::create(filename, true, *device.executionEnvironment, device.getRootDeviceIndex(), device.getDeviceBitfield());
            device.resetCommandStreamReceiver(pBcsCommandStreamReceiver, i);
        }
    }
}

void AUBFixtureL0::SetUp() {
    SetUp(NEO::defaultHwInfo.get(), false);
}
void AUBFixtureL0::SetUp(const NEO::HardwareInfo *hardwareInfo, bool debuggingEnabled) {
    ASSERT_NE(nullptr, hardwareInfo);
    const auto &hwInfo = *hardwareInfo;

    auto &hwHelper = NEO::HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto engineType = getChosenEngineType(hwInfo);

    const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    std::stringstream strfilename;

    strfilename << NEO::ApiSpecificConfig::getAubPrefixForSpecificApi();
    strfilename << testInfo->test_case_name() << "_" << testInfo->name() << "_" << hwHelper.getCsTraits(engineType).name;

    executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1u);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    if (debuggingEnabled) {
        executionEnvironment->setDebuggingEnabled();
    }
    neoDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(&hwInfo, executionEnvironment, 0u);

    if (NEO::testMode == NEO::TestMode::AubTestsWithTbx) {
        this->csr = NEO::TbxCommandStreamReceiver::create(strfilename.str(), true, *executionEnvironment, 0, neoDevice->getDeviceBitfield());
    } else {
        this->csr = NEO::AUBCommandStreamReceiver::create(strfilename.str(), true, *executionEnvironment, 0, neoDevice->getDeviceBitfield());
    }

    neoDevice->resetCommandStreamReceiver(this->csr);
    prepareCopyEngines(*neoDevice, strfilename.str());

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<ult::Mock<DriverHandleImp>>();

    driverHandle->enableProgramDebugging = debuggingEnabled;

    driverHandle->initialize(std::move(devices));

    device = driverHandle->devices[0];

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_result_t returnValue;
    commandList.reset(ult::whiteboxCast(CommandList::create(hwInfo.platform.eProductFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));

    returnValue = ZE_RESULT_ERROR_UNINITIALIZED;
    ze_command_queue_desc_t queueDesc = {};
    pCmdq = CommandQueue::create(hwInfo.platform.eProductFamily, device, csr, &queueDesc, false, false, returnValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}
void AUBFixtureL0::TearDown() {
    context->destroy();
    pCmdq->destroy();
}

} // namespace L0
