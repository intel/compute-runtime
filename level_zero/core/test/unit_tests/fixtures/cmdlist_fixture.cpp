/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/fixtures/cmdlist_fixture.h"

#include "shared/source/command_container/cmdcontainer.h"
#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_device.h"

#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"

#include "gtest/gtest.h"

namespace L0 {
namespace ult {

CommandListFixture::CommandListFixture() = default;
CommandListFixture ::~CommandListFixture() = default;
void CommandListFixture::setUp() {
    DeviceFixture::setUp();
    ze_result_t returnValue;
    commandList.reset(CommandList::whiteboxCast(CommandList::create(device->getHwInfo().platform.eProductFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));

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

DirectSubmissionCommandListFixture::DirectSubmissionCommandListFixture() = default;
DirectSubmissionCommandListFixture ::~DirectSubmissionCommandListFixture() = default;

void DirectSubmissionCommandListFixture::setUp() {
    debugManager.flags.EnableDirectSubmission.set(1);
    CommandListFixture::setUp();
}

void DirectSubmissionCommandListFixture::tearDown() {
    CommandListFixture::tearDown();
}

void MultiTileCommandListFixtureInit::setUp() {
    debugManager.flags.EnableImplicitScaling.set(1);
    osLocalMemoryBackup = std::make_unique<VariableBackup<bool>>(&NEO::OSInterface::osEnableLocalMemory, true);
    apiSupportBackup = std::make_unique<VariableBackup<bool>>(&NEO::ImplicitScaling::apiSupport, true);

    SingleRootMultiSubDeviceFixture::setUp();
}

void MultiTileCommandListFixtureInit::setUpParams(bool createImmediate, bool createInternal, bool createCopy, int32_t primaryBuffer) {
    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(primaryBuffer);

    ze_result_t returnValue;

    NEO::EngineGroupType cmdListEngineType = createCopy ? NEO::EngineGroupType::copy : NEO::EngineGroupType::renderCompute;

    if (!createImmediate) {
        commandList.reset(CommandList::whiteboxCast(CommandList::create(device->getHwInfo().platform.eProductFamily, device, cmdListEngineType, 0u, returnValue, false)));
    } else {
        const ze_command_queue_desc_t desc = {};
        commandList.reset(CommandList::whiteboxCast(CommandList::createImmediate(device->getHwInfo().platform.eProductFamily, device, &desc, createInternal, cmdListEngineType, returnValue)));
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

    commandList.reset(CommandList::whiteboxCast(CommandList::create(productFamily, device, engineGroupType, 0u, returnValue, false)));
    commandListImmediate.reset(CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, engineGroupType, returnValue)));

    mockKernelImmData = std::make_unique<MockImmutableData>(0u);
    createModuleFromMockBinary(0u, false, mockKernelImmData.get());

    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    createKernel(kernel.get());

    this->dshRequired = device->getDeviceInfo().imageSupport;
    this->expectedSbaCmds = commandList->doubleSbaWa ? 2 : 1;
}

void ModuleMutableCommandListFixture::setUp(uint32_t revision) {
    backupHwInfo = std::make_unique<VariableBackup<HardwareInfo>>(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    if (revision != 0) {
        debugManager.flags.OverrideRevision.set(revision);
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

uint32_t ModuleMutableCommandListFixture::getMocs(bool l3On) {
    return device->getMOCS(l3On, false) >> 1;
}

void FrontEndCommandListFixtureInit::setUp(int32_t dispatchCmdBufferPrimary) {
    UnitTestSetter::disableHeapless(this->restorer);

    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(dispatchCmdBufferPrimary);
    debugManager.flags.EnableFrontEndTracking.set(1);

    ModuleMutableCommandListFixture::setUp(REVISION_B);
}

void CmdListPipelineSelectStateFixture::setUp() {
    debugManager.flags.EnablePipelineSelectTracking.set(1);
    debugManager.flags.EnableFrontEndTracking.set(0);
    ModuleMutableCommandListFixture::setUp();

    auto result = ZE_RESULT_SUCCESS;
    commandList2.reset(CommandList::whiteboxCast(CommandList::create(productFamily, this->device, this->engineGroupType, 0u, result, false)));
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
}

void CmdListPipelineSelectStateFixture::tearDown() {
    commandList2.reset(nullptr);
    ModuleMutableCommandListFixture::tearDown();
}

void CmdListStateComputeModeStateFixture::setUp() {
    debugManager.flags.EnableStateComputeModeTracking.set(1);
    debugManager.flags.EnableStateBaseAddressTracking.set(1);

    ModuleMutableCommandListFixture::setUp();
}

void CommandListStateBaseAddressFixture::setUp() {
    debugManager.flags.EnableStateBaseAddressTracking.set(1);
    debugManager.flags.ForceL1Caching.set(0);
    debugManager.flags.ForceDefaultHeapSize.set(64);

    ModuleMutableCommandListFixture::setUp();
}

void CommandListPrivateHeapsFixture::setUp() {

    UnitTestSetter::disableHeapless(this->restore);

    constexpr uint32_t storeAllocations = 4;

    debugManager.flags.SelectCmdListHeapAddressModel.set(static_cast<int32_t>(NEO::HeapAddressModel::privateHeaps));
    debugManager.flags.SetAmountOfReusableAllocations.set(storeAllocations);

    CommandListStateBaseAddressFixture::setUp();

    for (uint32_t i = 0; i < storeAllocations; i++) {
        auto heapAllocation = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), true, 2 * MemoryConstants::megaByte,
                                                                                                   NEO::AllocationType::linearStream, false, false,
                                                                                                   neoDevice->getDeviceBitfield()});
        static_cast<CommandQueueImp *>(commandListImmediate->cmdQImmediate)->getCsr()->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(heapAllocation), REUSABLE_ALLOCATION);
    }

    mockKernelImmData->kernelDescriptor->payloadMappings.samplerTable.numSamplers = 1;
    mockKernelImmData->kernelDescriptor->payloadMappings.samplerTable.tableOffset = 16;
    mockKernelImmData->kernelDescriptor->payloadMappings.samplerTable.borderColor = 0;
    kernel->dynamicStateHeapData.reset(new uint8_t[512]);
    kernel->dynamicStateHeapDataSize = 512;

    mockKernelImmData->mockKernelInfo->heapInfo.surfaceStateHeapSize = 128;
    mockKernelImmData->kernelDescriptor->payloadMappings.bindingTable.numEntries = 1;
    mockKernelImmData->kernelDescriptor->payloadMappings.bindingTable.tableOffset = 64;
    kernel->surfaceStateHeapDataSize = 128;
    kernel->surfaceStateHeapData.reset(new uint8_t[256]);

    bindlessHeapsHelper = device->getNEODevice()->getBindlessHeapsHelper();
}

void CommandListPrivateHeapsFixture::checkAndPrepareBindlessKernel() {
    if (NEO::ApiSpecificConfig::getBindlessMode(*device->getNEODevice())) {
        const_cast<KernelDescriptor &>(kernel->getKernelDescriptor()).kernelAttributes.bufferAddressingMode = KernelDescriptor::Bindless;
        isBindlessKernel = true;
    }
}

void CommandListGlobalHeapsFixtureInit::setUp() {
    CommandListGlobalHeapsFixtureInit::setUpParams(static_cast<int32_t>(NEO::HeapAddressModel::globalStateless));
}

void CommandListGlobalHeapsFixtureInit::setUpParams(int32_t globalHeapMode) {

    UnitTestSetter::disableHeaplessStateInit(this->restorer);

    if (globalHeapMode == static_cast<int32_t>(NEO::HeapAddressModel::globalStateless)) {
        debugManager.flags.UseExternalAllocatorForSshAndDsh.set(0);
    }

    debugManager.flags.SelectCmdListHeapAddressModel.set(globalHeapMode);
    debugManager.flags.UseImmediateFlushTask.set(0);
    CommandListStateBaseAddressFixture::setUp();

    debugManager.flags.SelectCmdListHeapAddressModel.set(static_cast<int32_t>(NEO::HeapAddressModel::privateHeaps));

    ze_result_t returnValue;
    commandListPrivateHeap.reset(CommandList::whiteboxCast(CommandList::create(productFamily, device, engineGroupType, 0u, returnValue, false)));

    debugManager.flags.SelectCmdListHeapAddressModel.set(globalHeapMode);
}

void CommandListGlobalHeapsFixtureInit::tearDown() {
    commandListPrivateHeap.reset(nullptr);
    CommandListStateBaseAddressFixture::tearDown();
}

void ImmediateCmdListSharedHeapsFixture::setUp() {
    constexpr uint32_t storeAllocations = 4;

    debugManager.flags.EnableImmediateCmdListHeapSharing.set(1);
    debugManager.flags.SelectCmdListHeapAddressModel.set(static_cast<int32_t>(NEO::HeapAddressModel::privateHeaps));
    debugManager.flags.SetAmountOfReusableAllocations.set(storeAllocations);

    ModuleMutableCommandListFixture::setUp();

    for (uint32_t i = 0; i < storeAllocations; i++) {
        auto heapAllocation = neoDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), true, 2 * MemoryConstants::megaByte,
                                                                                                   NEO::AllocationType::linearStream, false, false,
                                                                                                   neoDevice->getDeviceBitfield()});
        static_cast<CommandQueueImp *>(commandListImmediate->cmdQImmediate)->getCsr()->getInternalAllocationStorage()->storeAllocation(std::unique_ptr<GraphicsAllocation>(heapAllocation), REUSABLE_ALLOCATION);
    }

    ze_result_t returnValue;

    ze_command_queue_desc_t queueDesc{};
    queueDesc.ordinal = 0u;
    queueDesc.index = 0u;
    queueDesc.priority = ZE_COMMAND_QUEUE_PRIORITY_NORMAL;

    commandListImmediateCoexisting.reset(CommandList::whiteboxCast(CommandList::createImmediate(productFamily, device, &queueDesc, false, engineGroupType, returnValue)));

    if (this->dshRequired) {
        mockKernelImmData->kernelInfo->kernelDescriptor.payloadMappings.samplerTable.numSamplers = 2;
        mockKernelImmData->kernelInfo->kernelDescriptor.payloadMappings.samplerTable.tableOffset = 16;
        mockKernelImmData->kernelInfo->kernelDescriptor.payloadMappings.samplerTable.borderColor = 0;

        auto surfaceStateSize = device->getNEODevice()->getGfxCoreHelper().getSamplerStateSize();

        kernel->dynamicStateHeapDataSize = static_cast<uint32_t>(surfaceStateSize * 2 + mockKernelImmData->kernelInfo->kernelDescriptor.payloadMappings.samplerTable.tableOffset);
        kernel->dynamicStateHeapData.reset(new uint8_t[kernel->dynamicStateHeapDataSize]);

        mockKernelImmData->mockKernelDescriptor->payloadMappings.samplerTable = mockKernelImmData->kernelInfo->kernelDescriptor.payloadMappings.samplerTable;
    }

    mockKernelImmData->kernelInfo->heapInfo.surfaceStateHeapSize = static_cast<uint32_t>(64 + sizeof(uint32_t));
    mockKernelImmData->mockKernelDescriptor->payloadMappings.bindingTable.numEntries = 1;
    mockKernelImmData->mockKernelDescriptor->payloadMappings.bindingTable.tableOffset = 0x40;
    mockKernelImmData->mockKernelDescriptor->kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindfulAndStateless;

    kernel->surfaceStateHeapDataSize = mockKernelImmData->kernelInfo->heapInfo.surfaceStateHeapSize;
    kernel->surfaceStateHeapData.reset(new uint8_t[kernel->surfaceStateHeapDataSize]);

    ze_event_pool_desc_t eventPoolDesc = {ZE_STRUCTURE_TYPE_EVENT_POOL_DESC};
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {ZE_STRUCTURE_TYPE_EVENT_DESC};
    eventDesc.index = 0;
    eventDesc.wait = 0;
    eventDesc.signal = 0;

    eventPool = std::unique_ptr<EventPool>(static_cast<EventPool *>(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue)));
    auto &l0GfxCoreHelper = neoDevice->getRootDeviceEnvironment().getHelper<L0GfxCoreHelper>();
    event = std::unique_ptr<Event>(static_cast<Event *>(l0GfxCoreHelper.createEvent(eventPool.get(), &eventDesc, device)));
}

void ImmediateCmdListSharedHeapsFixture::tearDown() {
    event.reset(nullptr);
    eventPool.reset(nullptr);
    commandListImmediateCoexisting.reset(nullptr);

    ModuleMutableCommandListFixture::tearDown();
}

void ImmediateCmdListSharedHeapsFlushTaskFixtureInit::setUp(int32_t useImmediateFlushTask) {
    this->useImmediateFlushTask = useImmediateFlushTask;
    debugManager.flags.UseImmediateFlushTask.set(useImmediateFlushTask);
    debugManager.flags.ContextGroupSize.set(0);

    ImmediateCmdListSharedHeapsFixture::setUp();
}

void ImmediateCmdListSharedHeapsFlushTaskFixtureInit::appendNonKernelOperation(L0::ult::CommandList *currentCmdList, NonKernelOperation operation) {
    ze_result_t result;

    if (operation == NonKernelOperation::Barrier) {
        result = currentCmdList->appendBarrier(nullptr, 0, nullptr, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    } else if (operation == NonKernelOperation::SignalEvent) {
        result = currentCmdList->appendSignalEvent(event->toHandle(), false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    } else if (operation == NonKernelOperation::ResetEvent) {
        result = currentCmdList->appendEventReset(event->toHandle());
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    } else if (operation == NonKernelOperation::WaitOnEvents) {
        auto eventHandle = event->toHandle();
        result = currentCmdList->appendWaitOnEvents(1, &eventHandle, nullptr, false, false, false, false, false, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    } else if (operation == NonKernelOperation::WriteGlobalTimestamp) {
        uint64_t timestampAddress = 0xfffffffffff0L;
        uint64_t *dstptr = reinterpret_cast<uint64_t *>(timestampAddress);

        result = currentCmdList->appendWriteGlobalTimestamp(dstptr, nullptr, 0, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    } else if (operation == NonKernelOperation::MemoryRangesBarrier) {
        uint8_t dstPtr[64] = {};
        driverHandle->importExternalPointer(dstPtr, MemoryConstants::pageSize);

        size_t rangeSizes = 1;
        const void **ranges = reinterpret_cast<const void **>(&dstPtr[0]);
        result = currentCmdList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, nullptr, 0, nullptr);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);

        driverHandle->releaseImportedPointer(dstPtr);
    }
}

void ImmediateCmdListSharedHeapsFlushTaskFixtureInit::validateDispatchFlags(bool nonKernel, NEO::ImmediateDispatchFlags &recordedImmediateFlushTaskFlags, const NEO::IndirectHeap *recordedSsh) {
    if (this->useImmediateFlushTask == 1) {
        if (nonKernel) {
            EXPECT_EQ(nullptr, recordedImmediateFlushTaskFlags.sshCpuBase);
        } else {
            EXPECT_NE(nullptr, recordedImmediateFlushTaskFlags.sshCpuBase);
        }
    } else {
        if (nonKernel) {
            EXPECT_EQ(nullptr, recordedSsh);
        } else {
            EXPECT_NE(nullptr, recordedSsh);
        }
    }
}

bool AppendFillFixture::MockDriverFillHandle::findAllocationDataForRange(const void *buffer,
                                                                         size_t size,
                                                                         NEO::SvmAllocationData *&allocData) {
    if (forceFalseFromfindAllocationDataForRange) {
        return false;
    }
    mockAllocation.reset(new NEO::MockGraphicsAllocation(const_cast<void *>(buffer), size));
    data.gpuAllocations.addAllocation(mockAllocation.get());
    allocData = &data;
    return true;
}

void AppendFillFixture::setUp() {
    dstPtr = new uint8_t[allocSize];
    immediateDstPtr = new uint8_t[allocSize];

    neoDevice = NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(NEO::defaultHwInfo.get());
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(neoDevice->executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
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
    NEO::debugManager.flags.SignalAllEventPackets.set(0);
    NEO::debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(0);

    CommandListFixture::setUp();
}

void CommandListSecondaryBatchBufferFixture::setUp() {
    NEO::debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(0);

    CommandListFixture::setUp();
}

void TbxImmediateCommandListFixture::setEvent() {
    auto mockEvent = static_cast<Event *>(event.get());

    size_t offset = event->getCompletionFieldOffset();
    void *completionAddress = ptrOffset(mockEvent->hostAddressFromPool, offset);
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
    commandList = CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
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
    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(1);

    ModuleMutableCommandListFixture::setUp();
}

void PrimaryBatchBufferPreamblelessCmdListFixture::setUp() {
    debugManager.flags.SelectCmdListHeapAddressModel.set(static_cast<int32_t>(NEO::HeapAddressModel::globalStateless));
    debugManager.flags.ForceL1Caching.set(0);
    debugManager.flags.EnableStateBaseAddressTracking.set(1);

    PrimaryBatchBufferCmdListFixture::setUp();

    ze_result_t returnValue;
    commandList2.reset(CommandList::whiteboxCast(CommandList::create(productFamily, device, engineGroupType, 0u, returnValue, false)));
    commandList3.reset(CommandList::whiteboxCast(CommandList::create(productFamily, device, engineGroupType, 0u, returnValue, false)));
}

void PrimaryBatchBufferPreamblelessCmdListFixture::tearDown() {
    commandList2.reset(nullptr);
    commandList3.reset(nullptr);

    PrimaryBatchBufferCmdListFixture::tearDown();
}

void ImmediateFlushTaskCmdListFixture::setUp() {
    debugManager.flags.UseImmediateFlushTask.set(1);
    debugManager.flags.ForceL1Caching.set(0);

    ModuleMutableCommandListFixture::setUp();
}

void ImmediateFlushTaskGlobalStatelessCmdListFixture::setUp() {
    debugManager.flags.SelectCmdListHeapAddressModel.set(static_cast<int32_t>(NEO::HeapAddressModel::globalStateless));

    ImmediateFlushTaskCmdListFixture::setUp();
}

void ImmediateFlushTaskCsrSharedHeapCmdListFixture::setUp() {
    debugManager.flags.EnableImmediateCmdListHeapSharing.set(1);
    debugManager.flags.SelectCmdListHeapAddressModel.set(static_cast<int32_t>(NEO::HeapAddressModel::privateHeaps));
    debugManager.flags.ForceDefaultHeapSize.set(64);

    ImmediateFlushTaskCmdListFixture::setUp();

    mockKernelImmData->kernelDescriptor->payloadMappings.samplerTable.numSamplers = 1;
    mockKernelImmData->kernelDescriptor->payloadMappings.samplerTable.tableOffset = 16;
    mockKernelImmData->kernelDescriptor->payloadMappings.samplerTable.borderColor = 0;
    kernel->dynamicStateHeapData.reset(new uint8_t[512]);
    kernel->dynamicStateHeapDataSize = 512;

    mockKernelImmData->mockKernelInfo->heapInfo.surfaceStateHeapSize = 128;
    mockKernelImmData->kernelDescriptor->payloadMappings.bindingTable.numEntries = 1;
    mockKernelImmData->kernelDescriptor->payloadMappings.bindingTable.tableOffset = 64;
    kernel->surfaceStateHeapDataSize = 128;
    kernel->surfaceStateHeapData.reset(new uint8_t[256]);
}

void ImmediateFlushTaskPrivateHeapCmdListFixture::setUp() {
    debugManager.flags.EnableImmediateCmdListHeapSharing.set(0);
    debugManager.flags.SelectCmdListHeapAddressModel.set(static_cast<int32_t>(NEO::HeapAddressModel::privateHeaps));

    ImmediateFlushTaskCmdListFixture::setUp();
}

void CommandQueueThreadArbitrationPolicyFixture::setUp() {
    ze_result_t returnValue = ZE_RESULT_SUCCESS;
    auto executionEnvironment = new NEO::MockExecutionEnvironment();
    auto mockBuiltIns = new MockBuiltins();
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    executionEnvironment->rootDeviceEnvironments[0]->initGmm();

    neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);

    std::vector<std::unique_ptr<NEO::Device>> devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));

    auto driverHandleUlt = whiteboxCast(DriverHandle::create(std::move(devices), L0EnvVariables{}, &returnValue));
    driverHandle.reset(driverHandleUlt);

    ASSERT_NE(nullptr, driverHandle);

    ze_device_handle_t hDevice;
    uint32_t count = 1;
    ze_result_t result = driverHandle->getDevice(&count, &hDevice);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    device = L0::Device::fromHandle(hDevice);
    ASSERT_NE(nullptr, device);

    ze_command_queue_desc_t queueDesc = {};
    commandQueue = whiteboxCast(CommandQueue::create(productFamily, device,
                                                     neoDevice->getDefaultEngine().commandStreamReceiver,
                                                     &queueDesc,
                                                     false,
                                                     false,
                                                     false,
                                                     returnValue));
    ASSERT_NE(nullptr, commandQueue);

    commandList = CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false);
    ASSERT_NE(nullptr, commandList);

    commandList->close();
}

void CommandQueueThreadArbitrationPolicyFixture::tearDown() {
    commandList->destroy();
    commandQueue->destroy();
}

void CommandListScratchPatchFixtureInit::setUpParams(int32_t globalStatelessMode, int32_t heaplessStateInitEnabled, bool scratchAddressPatchingEnabled) {
    fixtureGlobalStatelessMode = globalStatelessMode;
    debugManager.flags.SelectCmdListHeapAddressModel.set(globalStatelessMode);

    ModuleMutableCommandListFixture::setUp();

    commandList->scratchAddressPatchingEnabled = scratchAddressPatchingEnabled;
    commandList->heaplessModeEnabled = true;
    commandList->heaplessStateInitEnabled = !!heaplessStateInitEnabled;

    commandListImmediate->heaplessModeEnabled = true;
    commandListImmediate->heaplessStateInitEnabled = !!heaplessStateInitEnabled;

    commandQueue->heaplessModeEnabled = true;
    commandQueue->heaplessStateInitEnabled = !!heaplessStateInitEnabled;

    mockKernelImmData->kernelDescriptor->kernelAttributes.perThreadScratchSize[0] = 0x40;
    mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress.pointerSize = scratchInlinePointerSize;
    mockKernelImmData->kernelDescriptor->payloadMappings.implicitArgs.scratchPointerAddress.offset = scratchInlineOffset;
}

void CommandListScratchPatchFixtureInit::tearDown() {
    ModuleMutableCommandListFixture::tearDown();
}

uint64_t CommandListScratchPatchFixtureInit::getSurfStateGpuBase(bool useImmediate) {
    if (fixtureGlobalStatelessMode == 1) {
        return device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getGlobalStatelessHeapAllocation()->getGpuAddress();
    } else {

        if (useImmediate) {
            return device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getIndirectHeap(NEO::surfaceState, 0).getGpuBase();
        } else {
            return commandList->commandContainer.getIndirectHeap(NEO::surfaceState)->getGpuBase();
        }
    }
}

uint64_t CommandListScratchPatchFixtureInit::getExpectedScratchPatchAddress(uint64_t controllerScratchAddress) {
    if (fixtureGlobalStatelessMode == 1) {
        controllerScratchAddress += device->getNEODevice()->getDefaultEngine().commandStreamReceiver->getGlobalStatelessHeapAllocation()->getGpuAddress();
    }

    return controllerScratchAddress;
}

} // namespace ult
} // namespace L0
