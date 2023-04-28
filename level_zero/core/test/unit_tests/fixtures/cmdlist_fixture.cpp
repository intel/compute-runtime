/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"

#include "shared/source/built_ins/sip.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"

#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
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

void MultiTileCommandListFixtureInit::setUpParams(bool createImmediate, bool createInternal, bool createCopy, int32_t primaryBuffer) {
    DebugManager.flags.DispatchCmdlistCmdBufferPrimary.set(primaryBuffer);

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

void ModuleMutableCommandListFixture::setUpImpl() {
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
    backupHwInfo = std::make_unique<VariableBackup<HardwareInfo>>(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    if (revision != 0) {
        DebugManager.flags.OverrideRevision.set(revision);
    }
    ModuleImmutableDataFixture::setUp();
    ModuleMutableCommandListFixture::setUpImpl();
}

void ModuleMutableCommandListFixture::tearDown() {
    commandQueue->destroy();
    commandList.reset(nullptr);
    commandListImmediate.reset(nullptr);
    kernel.reset(nullptr);
    mockKernelImmData.reset(nullptr);
    ModuleImmutableDataFixture::tearDown();
}

void FrontEndCommandListFixtureInit::setUp(int32_t dispatchCmdBufferPrimary) {
    DebugManager.flags.DispatchCmdlistCmdBufferPrimary.set(dispatchCmdBufferPrimary);
    DebugManager.flags.EnableFrontEndTracking.set(1);
    DebugManager.flags.EnableFlushTaskSubmission.set(1);
    ModuleMutableCommandListFixture::setUp(REVISION_B);
}

void CmdListPipelineSelectStateFixture::setUp() {
    DebugManager.flags.EnablePipelineSelectTracking.set(1);
    ModuleMutableCommandListFixture::setUp();

    auto result = ZE_RESULT_SUCCESS;
    commandList2.reset(whiteboxCast(CommandList::create(productFamily, this->device, this->engineGroupType, 0u, result)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

void CmdListPipelineSelectStateFixture::tearDown() {
    commandList2.reset(nullptr);
    ModuleMutableCommandListFixture::tearDown();
}

void CmdListStateComputeModeStateFixture::setUp() {
    DebugManager.flags.EnableStateComputeModeTracking.set(1);
    ModuleMutableCommandListFixture::setUp();
}

void CommandListStateBaseAddressFixture::setUp() {
    DebugManager.flags.EnableStateBaseAddressTracking.set(1);
    DebugManager.flags.ForceL1Caching.set(0);
    DebugManager.flags.ForceDefaultHeapSize.set(64);

    ModuleMutableCommandListFixture::setUp();

    this->dshRequired = device->getDeviceInfo().imageSupport;
    this->expectedSbaCmds = commandList->doubleSbaWa ? 2 : 1;
}

uint32_t CommandListStateBaseAddressFixture::getMocs(bool l3On) {
    return device->getMOCS(l3On, false) >> 1;
}

void CommandListPrivateHeapsFixture::setUp() {
    constexpr uint32_t storeAllocations = 4;

    DebugManager.flags.SelectCmdListHeapAddressModel.set(static_cast<int32_t>(NEO::HeapAddressModel::PrivateHeaps));
    DebugManager.flags.SetAmountOfReusableAllocations.set(storeAllocations);

    CommandListStateBaseAddressFixture::setUp();

    for (uint32_t i = 0; i < storeAllocations; i++) {
        auto heapAllocation = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), true, 2 * MB,
                                                                                                   NEO::AllocationType::LINEAR_STREAM, false, false,
                                                                                                   neoDevice->getDeviceBitfield()});
        commandListImmediate->csr->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(heapAllocation), REUSABLE_ALLOCATION);
    }

    mockKernelImmData->kernelDescriptor->payloadMappings.samplerTable.numSamplers = 1;
    mockKernelImmData->kernelDescriptor->payloadMappings.samplerTable.tableOffset = 16;
    mockKernelImmData->kernelDescriptor->payloadMappings.samplerTable.borderColor = 0;
    kernel->dynamicStateHeapData.reset(new uint8_t[512]);

    mockKernelImmData->mockKernelInfo->heapInfo.surfaceStateHeapSize = 128;
    mockKernelImmData->kernelDescriptor->payloadMappings.bindingTable.numEntries = 1;
    mockKernelImmData->kernelDescriptor->payloadMappings.bindingTable.tableOffset = 64;
    kernel->surfaceStateHeapDataSize = 128;
    kernel->surfaceStateHeapData.reset(new uint8_t[256]);
}

void CommandListGlobalHeapsFixtureInit::setUp() {
    CommandListGlobalHeapsFixtureInit::setUpParams(static_cast<int32_t>(NEO::HeapAddressModel::GlobalStateless));
}

void CommandListGlobalHeapsFixtureInit::setUpParams(int32_t globalHeapMode) {
    DebugManager.flags.SelectCmdListHeapAddressModel.set(globalHeapMode);
    CommandListStateBaseAddressFixture::setUp();

    DebugManager.flags.SelectCmdListHeapAddressModel.set(static_cast<int32_t>(NEO::HeapAddressModel::PrivateHeaps));

    ze_result_t returnValue;
    commandListPrivateHeap.reset(whiteboxCast(CommandList::create(productFamily, device, engineGroupType, 0u, returnValue)));

    DebugManager.flags.SelectCmdListHeapAddressModel.set(globalHeapMode);
}

void CommandListGlobalHeapsFixtureInit::tearDown() {
    commandListPrivateHeap.reset(nullptr);
    CommandListStateBaseAddressFixture::tearDown();
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

void RayTracingCmdListFixture::setUp() {
    ModuleMutableCommandListFixture::setUp();

    mockKernelImmData->kernelDescriptor->kernelAttributes.flags.hasRTCalls = true;

    const uint32_t bvhLevels = NEO::RayTracingHelper::maxBvhLevels;
    device->getNEODevice()->initializeRayTracing(bvhLevels);

    rtAllocation = device->getNEODevice()->getRTMemoryBackedBuffer();
    rtAllocationAddress = rtAllocation->getGpuAddress();
}

void CommandListAppendLaunchRayTracingKernelFixture::setUp() {
    ModuleFixture::setUp();

    ze_context_handle_t hContext;
    ze_context_desc_t desc = {ZE_STRUCTURE_TYPE_CONTEXT_DESC, nullptr, 0};
    ze_result_t res = device->getDriverHandle()->createContext(&desc, 0u, nullptr, &hContext);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    contextImp = static_cast<ContextImp *>(Context::fromHandle(hContext));

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto result = contextImp->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &allocSrc);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    buffer1 = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(allocSrc)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, buffer1);
    result = contextImp->allocDeviceMem(device->toHandle(), &deviceDesc, 16384u, 4096u, &allocDst);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    buffer2 = device->getDriverHandle()->getSvmAllocsManager()->getSVMAllocs()->get(allocDst)->gpuAllocations.getGraphicsAllocation(device->getRootDeviceIndex());
    ASSERT_NE(nullptr, buffer2);

    ze_result_t returnValue;
    commandList = whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::RenderCompute, 0u, returnValue));
    ASSERT_NE(commandList->getCmdContainer().getCommandStream(), nullptr);

    dispatchKernelArguments.groupCountX = 1u;
    dispatchKernelArguments.groupCountY = 2u;
    dispatchKernelArguments.groupCountZ = 3u;

    createKernel();
}

void CommandListAppendLaunchRayTracingKernelFixture::tearDown() {
    contextImp->freeMem(allocSrc);
    contextImp->freeMem(allocDst);
    commandList->destroy();
    contextImp->destroy();

    ModuleFixture::tearDown();
}

void PrimaryBatchBufferCmdListFixture::setUp() {
    DebugManager.flags.DispatchCmdlistCmdBufferPrimary.set(1);

    ModuleMutableCommandListFixture::setUp();
}

void PrimaryBatchBufferPreamblelessCmdListFixture::setUp() {
    DebugManager.flags.SelectCmdListHeapAddressModel.set(static_cast<int32_t>(NEO::HeapAddressModel::GlobalStateless));
    DebugManager.flags.ForceL1Caching.set(0);

    PrimaryBatchBufferCmdListFixture::setUp();

    ze_result_t returnValue;
    commandList2.reset(whiteboxCast(CommandList::create(productFamily, device, engineGroupType, 0u, returnValue)));
    commandList3.reset(whiteboxCast(CommandList::create(productFamily, device, engineGroupType, 0u, returnValue)));
}

void PrimaryBatchBufferPreamblelessCmdListFixture::tearDown() {
    commandList2.reset(nullptr);
    commandList3.reset(nullptr);

    PrimaryBatchBufferCmdListFixture::tearDown();
}

} // namespace ult
} // namespace L0
