/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "aub_fixture.h"

#include "shared/source/helpers/api_specific_config.h"

namespace L0 {

void AUBFixtureL0::prepareCopyEngines(NEO::MockDevice &device, const std::string &filename) {
    for (auto i = 0u; i < device.engines.size(); i++) {
        if (EngineHelpers::isBcs(device.engines[i].getEngineType())) {
            CommandStreamReceiver *pBcsCommandStreamReceiver = nullptr;
            pBcsCommandStreamReceiver = AUBCommandStreamReceiver::create(filename, true, *device.executionEnvironment, device.getRootDeviceIndex(), device.getDeviceBitfield());
            device.resetCommandStreamReceiver(pBcsCommandStreamReceiver, i);
        }
    }
}

void AUBFixtureL0::SetUp(const HardwareInfo *hardwareInfo) {
    const HardwareInfo &hwInfo = hardwareInfo ? *hardwareInfo : *defaultHwInfo;

    auto &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto engineType = getChosenEngineType(hwInfo);

    const ::testing::TestInfo *const testInfo = ::testing::UnitTest::GetInstance()->current_test_info();
    std::stringstream strfilename;

    strfilename << NEO::ApiSpecificConfig::getAubPrefixForSpecificApi();
    strfilename << testInfo->test_case_name() << "_" << testInfo->name() << "_" << hwHelper.getCsTraits(engineType).name;

    executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1u);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(&hwInfo);

    neoDevice = MockDevice::createWithExecutionEnvironment<MockDevice>(&hwInfo, executionEnvironment, 0u);

    this->csr = AUBCommandStreamReceiver::create(strfilename.str(), true, *executionEnvironment, 0, neoDevice->getDeviceBitfield());
    neoDevice->resetCommandStreamReceiver(this->csr);
    prepareCopyEngines(*neoDevice, strfilename.str());

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<ult::Mock<DriverHandleImp>>();
    driverHandle->initialize(std::move(devices));

    device = driverHandle->devices[0];

    ze_context_handle_t hContext;
    ze_context_desc_t desc;
    ze_result_t res = driverHandle->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    context = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_result_t returnValue;
    commandList.reset(ult::whitebox_cast(CommandList::create(hwInfo.platform.eProductFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    eventPool = std::unique_ptr<EventPool>(EventPool::create(device->getDriverHandle(), context, 0, nullptr, &eventPoolDesc));

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
