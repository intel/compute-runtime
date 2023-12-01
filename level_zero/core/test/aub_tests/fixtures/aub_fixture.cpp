/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/aub_tests/fixtures/aub_fixture.h"

#include "shared/source/command_stream/aub_command_stream_receiver.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/tests_configuration.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/context/context_imp.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver_handle.h"

#include "gtest/gtest.h"
#include "test_mode.h"

namespace L0 {
AUBFixtureL0::AUBFixtureL0() = default;
AUBFixtureL0::~AUBFixtureL0() = default;

void AUBFixtureL0::setUp() {
    setUp(NEO::defaultHwInfo.get(), false);
}
void AUBFixtureL0::setUp(const NEO::HardwareInfo *hardwareInfo, bool debuggingEnabled) {
    ASSERT_NE(nullptr, hardwareInfo);
    const auto &hwInfo = *hardwareInfo;
    backupUltConfig = std::make_unique<VariableBackup<NEO::UltHwConfig>>(&NEO::ultHwConfig);

    executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1u);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    if (debuggingEnabled) {
        executionEnvironment->setDebuggingMode(NEO::DebuggingMode::Online);
    }

    auto &gfxCoreHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<NEO::GfxCoreHelper>();
    auto engineType = getChosenEngineType(hwInfo);

    const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    std::stringstream strfilename;

    strfilename << NEO::ApiSpecificConfig::getAubPrefixForSpecificApi();
    strfilename << testInfo->test_case_name() << "_" << testInfo->name() << "_" << gfxCoreHelper.getCsTraits(engineType).name;

    aubFileName = strfilename.str();
    NEO::ultHwConfig.aubTestName = aubFileName.c_str();

    neoDevice = NEO::MockDevice::createWithExecutionEnvironment<NEO::MockDevice>(&hwInfo, executionEnvironment, 0u);

    this->csr = neoDevice->getDefaultEngine().commandStreamReceiver;

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<ult::Mock<DriverHandleImp>>();

    driverHandle->enableProgramDebugging = debuggingEnabled ? NEO::DebuggingMode::Online : NEO::DebuggingMode::Disabled;

    driverHandle->initialize(std::move(devices));

    device = driverHandle->devices[0];

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_result_t returnValue;
    commandList.reset(ult::CommandList::whiteboxCast(CommandList::create(hwInfo.platform.eProductFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue)));

    returnValue = ZE_RESULT_ERROR_UNINITIALIZED;
    ze_command_queue_desc_t queueDesc = {};
    pCmdq = CommandQueue::create(hwInfo.platform.eProductFamily, device, csr, &queueDesc, false, false, false, returnValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
}
void AUBFixtureL0::tearDown() {
    context->destroy();
    pCmdq->destroy();
}

} // namespace L0
