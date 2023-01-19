/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/core/source/hw_helpers/l0_hw_helper.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

void CommandListFixture::setUp() {
    DeviceFixture::setUp();
    ze_result_t returnValue;
    commandList.reset(whiteboxCast(CommandList::create(device->getHwInfo().platform.eProductFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    eventPool = std::unique_ptr<EventPool>(static_cast<EventPool *>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue)));
    event = std::unique_ptr<Event>(static_cast<Event *>(getHelper<L0GfxCoreHelper>().createEvent(eventPool.get(), &eventDesc, device)));
}

void CommandListFixture::tearDown() {
    event.reset(nullptr);
    eventPool.reset(nullptr);
    commandList.reset(nullptr);
    DeviceFixture::tearDown();
}

void MultiTileCommandListFixtureInit::setUp() {
    DebugManager.flags.EnableImplicitScaling.set(1);
    osLocalMemoryBackup = std::make_unique<VariableBackup<bool>>(&NEO::OSInterface::osEnableLocalMemory, true);
    apiSupportBackup = std::make_unique<VariableBackup<bool>>(&NEO::ImplicitScaling::apiSupport, true);

    SingleRootMultiSubDeviceFixture::setUp();
}

void MultiTileCommandListFixtureInit::setUpParams(bool createImmediate, bool createInternal, bool createCopy) {
    ze_result_t returnValue;

    NEO::EngineGroupType cmdListEngineType = createCopy ? NEO::EngineGroupType::Copy : NEO::EngineGroupType::RenderCompute;

    if (!createImmediate) {
        commandList.reset(whiteboxCast(CommandList::create(device->getHwInfo().platform.eProductFamily, device, cmdListEngineType, 0u, returnValue)));
    } else {
        const ze_command_queue_desc_t desc = {};
        commandList.reset(whiteboxCast(CommandList::createImmediate(device->getHwInfo().platform.eProductFamily, device, &desc, createInternal, cmdListEngineType, returnValue)));
    }
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    eventPool = std::unique_ptr<EventPool>(static_cast<EventPool *>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue)));
    event = std::unique_ptr<Event>(static_cast<Event *>(device->getL0GfxCoreHelper().createEvent(eventPool.get(), &eventDesc, device)));
}

void ModuleMutableCommandListFixture::setUpImpl(uint32_t revision) {
    if (revision != 0) {
        auto &productHelper = device->getProductHelper();
        auto revId = productHelper.getHwRevIdFromStepping(revision, device->getHwInfo());
        neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId = revId;
    }

    ze_result_t returnValue;

    ze_command_queue_desc_t queueDesc{ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC};
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    commandQueue = whiteboxCast(CommandQueue::create(productFamily,
                                                     device,
                                                     neoDevice->getDefaultEngine().commandStreamReceiver,
                                                     &queueDesc,
                                                     false,
                                                     false,
                                                     returnValue));

    auto &gfxCoreHelper = device->getGfxCoreHelper();
    engineGroupType = gfxCoreHelper.getEngineGroupType(neoDevice->getDefaultEngine().getEngineType(), neoDevice->getDefaultEngine().getEngineUsage(), device->getHwInfo());

    commandList.reset(whiteboxCast(CommandList::create(productFamily, device, engineGroupType, 0u, returnValue)));
    commandListImmediate.reset(whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, engineGroupType, returnValue)));
    commandListImmediate->isFlushTaskSubmissionEnabled = true;

    mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    createKernel(kernel.get());
}

void ModuleMutableCommandListFixture::setUp(uint32_t revision) {
    ModuleImmutableDataFixture::setUp();

    ModuleMutableCommandListFixture::setUpImpl(revision);
}

void ModuleMutableCommandListFixture::tearDown() {
    commandQueue->destroy();
    commandList.reset(nullptr);
    commandListImmediate.reset(nullptr);
    kernel.reset(nullptr);
    mockKernelImmData.reset(nullptr);
    ModuleImmutableDataFixture::tearDown();
}

void MultiReturnCommandListFixture::setUp() {
    DebugManager.flags.EnableFrontEndTracking.set(1);
    ModuleMutableCommandListFixture::setUp(REVISION_B);
}

void CmdListPipelineSelectStateFixture::setUp() {
    DebugManager.flags.EnablePipelineSelectTracking.set(1);
    ModuleMutableCommandListFixture::setUp();
}

void CmdListStateComputeModeStateFixture::setUp() {
    DebugManager.flags.EnableStateComputeModeTracking.set(1);
    ModuleMutableCommandListFixture::setUp();
}

void ImmediateCmdListSharedHeapsFixture::setUp() {
    DebugManager.flags.EnableFlushTaskSubmission.set(1);
    DebugManager.flags.EnableImmediateCmdListHeapSharing.set(1);
    ModuleMutableCommandListFixture::setUp();
}

bool AppendFillFixture::MockDriverFillHandle::findAllocationDataForRange(const void *buffer,
                                                                         size_t size,
                                                                         NEO::SvmAllocationData **allocData) {
    mockAllocation.reset(new NEO::MockGraphicsAllocation(const_cast<void *>(buffer), size));
    data.gpuAllocations.addAllocation(mockAllocation.get());
    if (allocData) {
        *allocData = &data;
    }
    return true;
}

void AppendFillFixture::setUp() {
    dstPtr = new uint8_t[allocSize];
    immediateDstPtr = new uint8_t[allocSize];

    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    driverHandle = std::make_unique<Mock<MockDriverFillHandle>>();
    driverHandle->initialize(std::move(devices));
    device = driverHandle->devices[0];
    neoDevice->deviceInfo.maxWorkGroupSize = 256;

    DeviceFixture::device = device;
}

void AppendFillFixture::tearDown() {
    delete[] immediateDstPtr;
    delete[] dstPtr;
}

void CommandListEventUsedPacketSignalFixture::setUp() {
    NEO::DebugManager.flags.SignalAllEventPackets.set(0);

    CommandListFixture::setUp();
}

void TbxImmediateCommandListFixture::setEvent() {
    auto mockEvent = static_cast<Event *>(event.get());

    size_t offset = event->getCompletionFieldOffset();
    void *completionAddress = ptrOffset(mockEvent->hostAddress, offset);
    size_t packets = event->getPacketsInUse();
    EventFieldType signaledValue = Event::STATE_SIGNALED;
    for (size_t i = 0; i < packets; i++) {
        memcpy(completionAddress, &signaledValue, sizeof(EventFieldType));
        completionAddress = ptrOffset(completionAddress, event->getSinglePacketSize());
    }
}

} // namespace ult
} // namespace L0
