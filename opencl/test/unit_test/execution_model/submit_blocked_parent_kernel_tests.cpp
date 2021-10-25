/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/utilities/hw_timestamps.h"
#include "shared/source/utilities/tag_allocator.h"

#include "opencl/source/command_queue/gpgpu_walker.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/helpers/task_information.h"
#include "opencl/test/unit_test/fixtures/execution_model_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_device_queue.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

#include <memory>

using namespace NEO;

class SubmitBlockedParentKernelFixture : public ExecutionModelSchedulerTest,
                                         public testing::Test {
    void SetUp() override {
        ExecutionModelSchedulerTest::SetUp();
    }

    void TearDown() override {
        ExecutionModelSchedulerTest::TearDown();
    }
};

template <typename GfxFamily>
class MockDeviceQueueHwWithCriticalSectionRelease : public DeviceQueueHw<GfxFamily> {
    using BaseClass = DeviceQueueHw<GfxFamily>;

  public:
    MockDeviceQueueHwWithCriticalSectionRelease(Context *context,
                                                ClDevice *device,
                                                cl_queue_properties &properties) : BaseClass(context, device, properties) {}

    bool isEMCriticalSectionFree() override {
        auto igilCmdQueue = reinterpret_cast<IGIL_CommandQueue *>(DeviceQueue::queueBuffer->getUnderlyingBuffer());

        criticalSectioncheckCounter++;

        if (criticalSectioncheckCounter == maxCounter) {
            igilCmdQueue->m_controls.m_CriticalSection = DeviceQueueHw<GfxFamily>::ExecutionModelCriticalSection::Free;
            return true;
        }

        return igilCmdQueue->m_controls.m_CriticalSection == DeviceQueueHw<GfxFamily>::ExecutionModelCriticalSection::Free;
    }

    void setupIndirectState(IndirectHeap &surfaceStateHeap, IndirectHeap &dynamicStateHeap, Kernel *parentKernel, uint32_t parentIDCount, bool isCcsUsed) override {
        indirectStateSetup = true;
        return BaseClass::setupIndirectState(surfaceStateHeap, dynamicStateHeap, parentKernel, parentIDCount, isCcsUsed);
    }
    void addExecutionModelCleanUpSection(Kernel *parentKernel, TagNodeBase *hwTimeStamp, uint64_t tagAddress, uint32_t taskCount) override {
        cleanupSectionAdded = true;

        auto hwTimestampT = static_cast<TagNode<HwTimeStamps> *>(hwTimeStamp);

        timestampAddedInCleanupSection = hwTimestampT ? hwTimestampT->tagForCpuAccess : nullptr;
        return BaseClass::addExecutionModelCleanUpSection(parentKernel, hwTimeStamp, tagAddress, taskCount);
    }
    void dispatchScheduler(LinearStream &commandStream, SchedulerKernel &scheduler, PreemptionMode preemptionMode, IndirectHeap *ssh, IndirectHeap *dsh, bool isCcsUsed) override {
        schedulerDispatched = true;
        return BaseClass::dispatchScheduler(commandStream, scheduler, preemptionMode, ssh, dsh, isCcsUsed);
    }

    uint32_t criticalSectioncheckCounter = 0;
    const uint32_t maxCounter = 10;
    bool indirectStateSetup = false;
    bool cleanupSectionAdded = false;
    bool schedulerDispatched = false;
    HwTimeStamps *timestampAddedInCleanupSection = nullptr;
};

HWCMDTEST_F(IGFX_GEN8_CORE, ParentKernelCommandQueueFixture, givenLockedEMcritcalSectionWhenParentKernelCommandIsSubmittedThenItWaitsForcriticalSectionReleasement) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(device);

    cl_queue_properties properties[3] = {0};
    MockParentKernel *parentKernel = MockParentKernel::create(*context);
    auto kernelInfos = MockKernel::toKernelInfoContainer(parentKernel->getKernelInfo(), rootDeviceIndex);
    MultiDeviceKernel multiDeviceKernel(MockMultiDeviceKernel::toKernelVector(parentKernel), kernelInfos);
    MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
    parentKernel->createReflectionSurface();
    context->setDefaultDeviceQueue(&mockDevQueue);

    mockDevQueue.acquireEMCriticalSection();

    size_t heapSize = 20;
    size_t dshSize = mockDevQueue.getDshBuffer()->getUnderlyingBufferSize();

    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
    pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, heapSize, ioh);
    pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, heapSize, ssh);

    dsh->getSpace(mockDevQueue.getDshOffset());

    size_t minSizeSSHForEM = HardwareCommandsHelper<FamilyType>::getSshSizeForExecutionModel(*parentKernel);

    auto cmdStreamAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 4096, GraphicsAllocation::AllocationType::COMMAND_BUFFER, device->getDeviceBitfield()});
    auto blockedCommandData = std::make_unique<KernelOperation>(new LinearStream(cmdStreamAllocation),
                                                                *pCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandData->setHeaps(dsh, ioh, ssh);

    blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
    PreemptionMode preemptionMode = device->getPreemptionMode();
    std::vector<Surface *> surfaces;
    auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, blockedCommandData, surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

    cmdComputeKernel->submit(0, false);

    EXPECT_EQ(mockDevQueue.maxCounter, mockDevQueue.criticalSectioncheckCounter);
    delete cmdComputeKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, ParentKernelCommandQueueFixture, givenParentKernelWhenCommandIsSubmittedThenPassedDshIsUsed) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(device);

    cl_queue_properties properties[3] = {0};
    MockParentKernel *parentKernel = MockParentKernel::create(*context);
    auto kernelInfos = MockKernel::toKernelInfoContainer(parentKernel->getKernelInfo(), rootDeviceIndex);
    MultiDeviceKernel multiDeviceKernel(MockMultiDeviceKernel::toKernelVector(parentKernel), kernelInfos);
    MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
    parentKernel->createReflectionSurface();
    context->setDefaultDeviceQueue(&mockDevQueue);

    auto *dshOfDevQueue = mockDevQueue.getIndirectHeap(IndirectHeap::DYNAMIC_STATE);

    size_t heapSize = 20;
    size_t dshSize = mockDevQueue.getDshBuffer()->getUnderlyingBufferSize();

    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
    pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, heapSize, ioh);
    pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, heapSize, ssh);

    // add initial offset of colorCalState
    dsh->getSpace(DeviceQueue::colorCalcStateSize);

    uint64_t ValueToFillDsh = 5;
    uint64_t *dshVal = (uint64_t *)dsh->getSpace(sizeof(uint64_t));

    // Fill Interface Descriptor Data
    *dshVal = ValueToFillDsh;

    // Move to parent DSH Offset
    size_t alignToOffsetDshSize = mockDevQueue.getDshOffset() - DeviceQueue::colorCalcStateSize - sizeof(uint64_t);
    dsh->getSpace(alignToOffsetDshSize);

    // Fill with pattern
    dshVal = (uint64_t *)dsh->getSpace(sizeof(uint64_t));
    *dshVal = ValueToFillDsh;

    size_t usedDSHBeforeSubmit = dshOfDevQueue->getUsed();

    uint32_t colorCalcSizeDevQueue = DeviceQueue::colorCalcStateSize;
    EXPECT_EQ(colorCalcSizeDevQueue, usedDSHBeforeSubmit);

    auto cmdStreamAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 4096, GraphicsAllocation::AllocationType::COMMAND_BUFFER, device->getDeviceBitfield()});
    auto blockedCommandData = std::make_unique<KernelOperation>(new LinearStream(cmdStreamAllocation),
                                                                *pCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandData->setHeaps(dsh, ioh, ssh);

    size_t minSizeSSHForEM = HardwareCommandsHelper<FamilyType>::getSshSizeForExecutionModel(*parentKernel);

    blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
    PreemptionMode preemptionMode = device->getPreemptionMode();
    std::vector<Surface *> surfaces;
    auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, blockedCommandData, surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

    cmdComputeKernel->submit(0, false);

    //device queue dsh is not changed
    size_t usedDSHAfterSubmit = dshOfDevQueue->getUsed();
    EXPECT_EQ(usedDSHAfterSubmit, usedDSHAfterSubmit);

    delete cmdComputeKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, ParentKernelCommandQueueFixture, givenParentKernelWhenCommandIsSubmittedThenIndirectStateAndEMCleanupSectionIsSetup) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(device);

    cl_queue_properties properties[3] = {0};
    MockParentKernel *parentKernel = MockParentKernel::create(*context);
    auto kernelInfos = MockKernel::toKernelInfoContainer(parentKernel->getKernelInfo(), rootDeviceIndex);
    MultiDeviceKernel multiDeviceKernel(MockMultiDeviceKernel::toKernelVector(parentKernel), kernelInfos);
    MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
    parentKernel->createReflectionSurface();
    context->setDefaultDeviceQueue(&mockDevQueue);

    size_t heapSize = 20;
    size_t dshSize = mockDevQueue.getDshBuffer()->getUnderlyingBufferSize();

    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
    pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, heapSize, ioh);
    pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, heapSize, ssh);

    dsh->getSpace(mockDevQueue.getDshOffset());

    auto cmdStreamAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 4096, GraphicsAllocation::AllocationType::COMMAND_BUFFER, device->getDeviceBitfield()});
    auto blockedCommandData = std::make_unique<KernelOperation>(new LinearStream(cmdStreamAllocation),
                                                                *pCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandData->setHeaps(dsh, ioh, ssh);

    size_t minSizeSSHForEM = HardwareCommandsHelper<FamilyType>::getSshSizeForExecutionModel(*parentKernel);

    blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
    PreemptionMode preemptionMode = device->getPreemptionMode();
    std::vector<Surface *> surfaces;
    auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, blockedCommandData, surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

    cmdComputeKernel->submit(0, false);

    EXPECT_TRUE(mockDevQueue.indirectStateSetup);
    EXPECT_TRUE(mockDevQueue.cleanupSectionAdded);

    delete cmdComputeKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, ParentKernelCommandQueueFixture, givenBlockedParentKernelWithProfilingWhenCommandIsSubmittedThenEMCleanupSectionsSetsCompleteTimestamp) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(device);

    cl_queue_properties properties[3] = {0};
    MockParentKernel *parentKernel = MockParentKernel::create(*context);
    auto kernelInfos = MockKernel::toKernelInfoContainer(parentKernel->getKernelInfo(), rootDeviceIndex);
    MultiDeviceKernel multiDeviceKernel(MockMultiDeviceKernel::toKernelVector(parentKernel), kernelInfos);
    MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
    parentKernel->createReflectionSurface();
    context->setDefaultDeviceQueue(&mockDevQueue);

    size_t heapSize = 20;
    size_t dshSize = mockDevQueue.getDshBuffer()->getUnderlyingBufferSize();
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
    pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, heapSize, ioh);
    pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, heapSize, ssh);
    dsh->getSpace(mockDevQueue.getDshOffset());

    auto cmdStreamAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 4096, GraphicsAllocation::AllocationType::COMMAND_BUFFER, device->getDeviceBitfield()});
    auto blockedCommandData = std::make_unique<KernelOperation>(new LinearStream(cmdStreamAllocation),
                                                                *pCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandData->setHeaps(dsh, ioh, ssh);

    size_t minSizeSSHForEM = HardwareCommandsHelper<FamilyType>::getSshSizeForExecutionModel(*parentKernel);

    blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
    PreemptionMode preemptionMode = device->getPreemptionMode();
    std::vector<Surface *> surfaces;
    auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, blockedCommandData, surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

    auto timestamp = static_cast<TagNode<HwTimeStamps> *>(pCmdQ->getGpgpuCommandStreamReceiver().getEventTsAllocator()->getTag());
    cmdComputeKernel->timestamp = timestamp;
    cmdComputeKernel->submit(0, false);

    EXPECT_TRUE(mockDevQueue.cleanupSectionAdded);
    EXPECT_EQ(mockDevQueue.timestampAddedInCleanupSection, timestamp->tagForCpuAccess);

    delete cmdComputeKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, ParentKernelCommandQueueFixture, givenParentKernelWhenCommandIsSubmittedThenSchedulerIsDispatched) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(device);

    cl_queue_properties properties[3] = {0};
    MockParentKernel *parentKernel = MockParentKernel::create(*context);
    auto kernelInfos = MockKernel::toKernelInfoContainer(parentKernel->getKernelInfo(), rootDeviceIndex);
    MultiDeviceKernel multiDeviceKernel(MockMultiDeviceKernel::toKernelVector(parentKernel), kernelInfos);
    MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
    parentKernel->createReflectionSurface();
    context->setDefaultDeviceQueue(&mockDevQueue);

    size_t heapSize = 20;
    size_t dshSize = mockDevQueue.getDshBuffer()->getUnderlyingBufferSize();

    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
    pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, heapSize, ioh);
    pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, heapSize, ssh);
    dsh->getSpace(mockDevQueue.getDshOffset());

    auto cmdStreamAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 4096, GraphicsAllocation::AllocationType::COMMAND_BUFFER, device->getDeviceBitfield()});
    auto blockedCommandData = std::make_unique<KernelOperation>(new LinearStream(cmdStreamAllocation),
                                                                *pCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandData->setHeaps(dsh, ioh, ssh);

    size_t minSizeSSHForEM = HardwareCommandsHelper<FamilyType>::getSshSizeForExecutionModel(*parentKernel);

    blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
    PreemptionMode preemptionMode = device->getPreemptionMode();
    std::vector<Surface *> surfaces;
    auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, blockedCommandData, surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

    cmdComputeKernel->submit(0, false);

    EXPECT_TRUE(mockDevQueue.schedulerDispatched);

    delete cmdComputeKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, ParentKernelCommandQueueFixture, givenUsedCommandQueueHeapsWhenParentKernelIsSubmittedThenQueueHeapsAreNotUsed) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(device);

    cl_queue_properties properties[3] = {0};
    MockParentKernel *parentKernel = MockParentKernel::create(*context);
    auto kernelInfos = MockKernel::toKernelInfoContainer(parentKernel->getKernelInfo(), rootDeviceIndex);
    MultiDeviceKernel multiDeviceKernel(MockMultiDeviceKernel::toKernelVector(parentKernel), kernelInfos);
    MockDeviceQueueHw<FamilyType> mockDevQueue(context, device, properties[0]);
    parentKernel->createReflectionSurface();
    context->setDefaultDeviceQueue(&mockDevQueue);

    MockCommandQueue cmdQ(context, device, properties, false);

    size_t minSizeSSHForEM = HardwareCommandsHelper<FamilyType>::getSshSizeForExecutionModel(*parentKernel);

    size_t heapSize = 20;

    size_t dshSize = mockDevQueue.getDshBuffer()->getUnderlyingBufferSize();
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
    pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, heapSize, ioh);
    pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, heapSize, ssh);

    dsh->getSpace(mockDevQueue.getDshOffset());

    auto &queueSsh = cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 100);
    auto &queueDsh = cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 100);
    auto &queueIoh = cmdQ.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 100);

    size_t usedSize = 4u;

    queueSsh.getSpace(usedSize);
    queueDsh.getSpace(usedSize);
    queueIoh.getSpace(usedSize);

    auto intialSshUsed = queueSsh.getUsed();

    auto cmdStreamAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 4096, GraphicsAllocation::AllocationType::COMMAND_BUFFER, device->getDeviceBitfield()});
    auto blockedCommandData = std::make_unique<KernelOperation>(new LinearStream(cmdStreamAllocation),
                                                                *pCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandData->setHeaps(dsh, ioh, ssh);

    blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
    PreemptionMode preemptionMode = device->getPreemptionMode();
    std::vector<Surface *> surfaces;
    auto *cmdComputeKernel = new CommandComputeKernel(cmdQ, blockedCommandData, surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

    cmdComputeKernel->submit(0, false);

    EXPECT_FALSE(cmdQ.releaseIndirectHeapCalled);
    EXPECT_EQ(usedSize, queueDsh.getUsed());
    EXPECT_EQ(usedSize, queueIoh.getUsed());
    EXPECT_EQ(intialSshUsed, queueSsh.getUsed());

    delete cmdComputeKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, ParentKernelCommandQueueFixture, givenNotUsedSSHWhenParentKernelIsSubmittedThenExistingSSHIsUsed) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(device);

    cl_queue_properties properties[3] = {0};
    MockParentKernel *parentKernel = MockParentKernel::create(*context);
    auto kernelInfos = MockKernel::toKernelInfoContainer(parentKernel->getKernelInfo(), rootDeviceIndex);
    MultiDeviceKernel multiDeviceKernel(MockMultiDeviceKernel::toKernelVector(parentKernel), kernelInfos);
    MockDeviceQueueHw<FamilyType> mockDevQueue(context, device, properties[0]);
    parentKernel->createReflectionSurface();
    context->setDefaultDeviceQueue(&mockDevQueue);

    size_t minSizeSSHForEM = HardwareCommandsHelper<FamilyType>::getSshSizeForExecutionModel(*parentKernel);

    size_t heapSize = 20;

    size_t dshSize = mockDevQueue.getDshBuffer()->getUnderlyingBufferSize();
    size_t sshSize = 1000;
    IndirectHeap *dsh = nullptr, *ioh = nullptr, *ssh = nullptr;
    pCmdQ->allocateHeapMemory(IndirectHeap::DYNAMIC_STATE, dshSize, dsh);
    pCmdQ->allocateHeapMemory(IndirectHeap::INDIRECT_OBJECT, heapSize, ioh);
    pCmdQ->allocateHeapMemory(IndirectHeap::SURFACE_STATE, sshSize, ssh);
    dsh->getSpace(mockDevQueue.getDshOffset());

    pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, sshSize);

    void *sshBuffer = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u).getCpuBase();

    auto cmdStreamAllocation = device->getMemoryManager()->allocateGraphicsMemoryWithProperties({device->getRootDeviceIndex(), 4096, GraphicsAllocation::AllocationType::COMMAND_BUFFER, device->getDeviceBitfield()});
    auto blockedCommandData = std::make_unique<KernelOperation>(new LinearStream(cmdStreamAllocation),
                                                                *pCmdQ->getGpgpuCommandStreamReceiver().getInternalAllocationStorage());
    blockedCommandData->setHeaps(dsh, ioh, ssh);

    blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
    PreemptionMode preemptionMode = device->getPreemptionMode();
    std::vector<Surface *> surfaces;
    auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, blockedCommandData, surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

    cmdComputeKernel->submit(0, false);

    void *newSshBuffer = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u).getCpuBase();

    EXPECT_EQ(sshBuffer, newSshBuffer);

    delete cmdComputeKernel;
}

HWCMDTEST_F(IGFX_GEN8_CORE, ParentKernelCommandQueueFixture, givenBlockedCommandQueueWhenDispatchWalkerIsCalledThenHeapsHaveProperSizes) {
    REQUIRE_DEVICE_ENQUEUE_OR_SKIP(device);

    cl_queue_properties properties[3] = {0};
    auto parentKernel = MockParentKernel::create(*context);
    auto kernelInfos = MockKernel::toKernelInfoContainer(parentKernel->getKernelInfo(), rootDeviceIndex);
    MultiDeviceKernel multiDeviceKernel(MockMultiDeviceKernel::toKernelVector(parentKernel), kernelInfos);

    MockDeviceQueueHw<FamilyType> mockDevQueue(context, device, properties[0]);
    parentKernel->createReflectionSurface();
    context->setDefaultDeviceQueue(&mockDevQueue);

    auto blockedCommandsData = createBlockedCommandsData(*pCmdQ);
    const size_t globalOffsets[3] = {0, 0, 0};
    const size_t workItems[3] = {1, 1, 1};

    DispatchInfo dispatchInfo(device, parentKernel, 1, workItems, nullptr, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
    MultiDispatchInfo multiDispatchInfo(parentKernel);
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        blockedCommandsData.get(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    EXPECT_NE(nullptr, blockedCommandsData);
    EXPECT_EQ(blockedCommandsData->dsh->getMaxAvailableSpace(), mockDevQueue.getDshBuffer()->getUnderlyingBufferSize());
    EXPECT_EQ(blockedCommandsData->dsh, blockedCommandsData->ioh);

    EXPECT_NE(nullptr, blockedCommandsData->dsh->getGraphicsAllocation());
    EXPECT_NE(nullptr, blockedCommandsData->ioh->getGraphicsAllocation());
    EXPECT_NE(nullptr, blockedCommandsData->ssh->getGraphicsAllocation());
    EXPECT_EQ(blockedCommandsData->dsh->getGraphicsAllocation(), blockedCommandsData->ioh->getGraphicsAllocation());
}
