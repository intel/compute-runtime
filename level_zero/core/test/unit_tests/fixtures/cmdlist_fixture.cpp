/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"

#include "shared/source/os_interface/hw_info_config.h"

namespace L0 {
namespace ult {

void CommandListFixture::setUp() {
    DeviceFixture::setUp();
    ze_result_t returnValue;
    commandList.reset(whiteboxCast(CommandList::create(device->getHwInfo().platform.eProductFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue)));

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
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

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 2;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    eventPool = std::unique_ptr<EventPool>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    event = std::unique_ptr<Event>(Event::create<uint32_t>(eventPool.get(), &eventDesc, device));
}

void ModuleMutableCommandListFixture::setUp(uint32_t revision) {
    ModuleImmutableDataFixture::setUp();

    if (revision != 0) {
        auto revId = NEO::HwInfoConfig::get(device->getHwInfo().platform.eProductFamily)->getHwRevIdFromStepping(revision, device->getHwInfo());
        neoDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId = revId;
    }

    ze_result_t returnValue;

    ze_command_queue_desc_t queueDesc{};
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

    engineGroupType = NEO::HwHelper::get(device->getHwInfo().platform.eRenderCoreFamily).getEngineGroupType(neoDevice->getDefaultEngine().getEngineType(), neoDevice->getDefaultEngine().getEngineUsage(), device->getHwInfo());

    commandList.reset(whiteboxCast(CommandList::create(productFamily, device, engineGroupType, 0u, returnValue)));
    commandListImmediate.reset(whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, engineGroupType, returnValue)));
    commandListImmediate->isFlushTaskSubmissionEnabled = true;

    mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    createKernel(kernel.get());
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

} // namespace ult
} // namespace L0
