/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/event/hw_timestamps.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/task_information.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_device_queue.h"

#include "unit_tests/fixtures/execution_model_fixture.h"

#include <memory>

using namespace OCLRT;

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
                                                Device *device,
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

    void setupIndirectState(IndirectHeap &surfaceStateHeap, Kernel *parentKernel, uint32_t parentIDCount) override {
        indirectStateSetup = true;
        return BaseClass::setupIndirectState(surfaceStateHeap, parentKernel, parentIDCount);
    }
    void addExecutionModelCleanUpSection(Kernel *parentKernel, HwTimeStamps *hwTimeStamp, uint32_t taskCount) override {
        cleanupSectionAdded = true;
        timestampAddedInCleanupSection = hwTimeStamp;
        return BaseClass::addExecutionModelCleanUpSection(parentKernel, hwTimeStamp, taskCount);
    }
    void dispatchScheduler(CommandQueue &cmdQ, SchedulerKernel &scheduler, PreemptionMode preemptionMode) override {
        schedulerDispatched = true;
        return BaseClass::dispatchScheduler(cmdQ, scheduler, preemptionMode);
    }

    uint32_t criticalSectioncheckCounter = 0;
    const uint32_t maxCounter = 10;
    bool indirectStateSetup = false;
    bool cleanupSectionAdded = false;
    bool schedulerDispatched = false;
    HwTimeStamps *timestampAddedInCleanupSection = nullptr;
};

HWTEST_F(ParentKernelCommandQueueFixture, givenLockedEMcritcalSectionWhenParentKernelCommandIsSubmittedThenItWaitsForcriticalSectionReleasement) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        MockParentKernel *parentKernel = MockParentKernel::create(*device);
        MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        mockDevQueue.acquireEMCriticalSection();

        size_t heapSize = 20;
        size_t alignement = 64;
        size_t dshSize = heapSize + mockDevQueue.getDshOffset();
        IndirectHeap *dsh = new IndirectHeap(alignedMalloc(dshSize, alignement), dshSize);
        dsh->getSpace(mockDevQueue.getDshOffset());

        size_t minSizeSSHForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        KernelOperation *blockedCommandData = new KernelOperation(std::unique_ptr<LinearStream>(new LinearStream()),
                                                                  std::unique_ptr<IndirectHeap>(dsh),
                                                                  std::unique_ptr<IndirectHeap>(new IndirectHeap(alignedMalloc(heapSize, alignement), heapSize)),
                                                                  std::unique_ptr<IndirectHeap>(new IndirectHeap(alignedMalloc(heapSize, alignement), heapSize)));

        blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        PreemptionMode preemptionMode = device->getPreemptionMode();
        std::vector<Surface *> surfaces;
        auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, device->getCommandStreamReceiver(),
                                                          std::unique_ptr<KernelOperation>(blockedCommandData), surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

        cmdComputeKernel->submit(0, false);

        EXPECT_EQ(mockDevQueue.maxCounter, mockDevQueue.criticalSectioncheckCounter);
        delete cmdComputeKernel;
        delete parentKernel;
    }
}

HWTEST_F(ParentKernelCommandQueueFixture, givenParentKernelWhenCommandIsSubmittedThenDeviceQueueDshIsUsed) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        MockParentKernel *parentKernel = MockParentKernel::create(*device);
        MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        auto *dshOfDevQueue = mockDevQueue.getIndirectHeap(IndirectHeap::DYNAMIC_STATE);

        size_t heapSize = 20;
        size_t alignement = 64;
        size_t dshSize = heapSize + mockDevQueue.getDshOffset();
        IndirectHeap *dsh = new IndirectHeap(alignedMalloc(dshSize, alignement), dshSize);
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
        uint64_t *devQueueDshValue = (uint64_t *)dshOfDevQueue->getSpace(0);

        uint32_t colorCalcSizeDevQueue = DeviceQueue::colorCalcStateSize;
        EXPECT_EQ(colorCalcSizeDevQueue, usedDSHBeforeSubmit);

        KernelOperation *blockedCommandData = new KernelOperation(std::unique_ptr<LinearStream>(new LinearStream()),
                                                                  std::unique_ptr<IndirectHeap>(dsh),
                                                                  std::unique_ptr<IndirectHeap>(new IndirectHeap(alignedMalloc(heapSize, alignement), heapSize)),
                                                                  std::unique_ptr<IndirectHeap>(new IndirectHeap(alignedMalloc(heapSize, alignement), heapSize)));

        size_t minSizeSSHForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        PreemptionMode preemptionMode = device->getPreemptionMode();
        std::vector<Surface *> surfaces;
        auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, device->getCommandStreamReceiver(),
                                                          std::unique_ptr<KernelOperation>(blockedCommandData), surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

        cmdComputeKernel->submit(0, false);

        size_t usedDSHAfterSubmit = dshOfDevQueue->getUsed();

        EXPECT_EQ(mockDevQueue.getDshOffset() + sizeof(uint64_t), usedDSHAfterSubmit);
        EXPECT_EQ(ValueToFillDsh, *devQueueDshValue);

        uint64_t *devQueueDshParent = (uint64_t *)ptrOffset((char *)dshOfDevQueue->getCpuBase(), mockDevQueue.getDshOffset());
        EXPECT_EQ(ValueToFillDsh, *devQueueDshParent);

        delete cmdComputeKernel;
        delete parentKernel;
    }
}

HWTEST_F(ParentKernelCommandQueueFixture, givenParentKernelWhenCommandIsSubmittedThenIndirectStateAndEMCleanupSectionIsSetup) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        MockParentKernel *parentKernel = MockParentKernel::create(*device);
        MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        size_t heapSize = 20;
        size_t alignement = 64;
        size_t dshSize = heapSize + mockDevQueue.getDshOffset();
        IndirectHeap *dsh = new IndirectHeap(alignedMalloc(dshSize, alignement), dshSize);
        dsh->getSpace(mockDevQueue.getDshOffset());

        KernelOperation *blockedCommandData = new KernelOperation(std::unique_ptr<LinearStream>(new LinearStream()),
                                                                  std::unique_ptr<IndirectHeap>(dsh),
                                                                  std::unique_ptr<IndirectHeap>(new IndirectHeap(alignedMalloc(heapSize, alignement), heapSize)),
                                                                  std::unique_ptr<IndirectHeap>(new IndirectHeap(alignedMalloc(heapSize, alignement), heapSize)));

        size_t minSizeSSHForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        PreemptionMode preemptionMode = device->getPreemptionMode();
        std::vector<Surface *> surfaces;
        auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, device->getCommandStreamReceiver(),
                                                          std::unique_ptr<KernelOperation>(blockedCommandData), surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

        cmdComputeKernel->submit(0, false);

        EXPECT_TRUE(mockDevQueue.indirectStateSetup);
        EXPECT_TRUE(mockDevQueue.cleanupSectionAdded);

        delete cmdComputeKernel;
        delete parentKernel;
    }
}

HWTEST_F(ParentKernelCommandQueueFixture, givenBlockedParentKernelWithProfilingWhenCommandIsSubmittedThenEMCleanupSectionsSetsCompleteTimestamp) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        MockParentKernel *parentKernel = MockParentKernel::create(*device);
        MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        size_t heapSize = 20;
        size_t alignement = 64;
        size_t dshSize = heapSize + mockDevQueue.getDshOffset();
        IndirectHeap *dsh = new IndirectHeap(alignedMalloc(dshSize, alignement), dshSize);
        dsh->getSpace(mockDevQueue.getDshOffset());

        KernelOperation *blockedCommandData = new KernelOperation(std::unique_ptr<LinearStream>(new LinearStream()),
                                                                  std::unique_ptr<IndirectHeap>(dsh),
                                                                  std::unique_ptr<IndirectHeap>(new IndirectHeap(alignedMalloc(heapSize, alignement), heapSize)),
                                                                  std::unique_ptr<IndirectHeap>(new IndirectHeap(alignedMalloc(heapSize, alignement), heapSize)));

        size_t minSizeSSHForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        PreemptionMode preemptionMode = device->getPreemptionMode();
        std::vector<Surface *> surfaces;
        auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, device->getCommandStreamReceiver(),
                                                          std::unique_ptr<KernelOperation>(blockedCommandData), surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

        HwTimeStamps timestamp;

        cmdComputeKernel->timestamp = &timestamp;
        cmdComputeKernel->submit(0, false);

        EXPECT_TRUE(mockDevQueue.cleanupSectionAdded);
        EXPECT_EQ(mockDevQueue.timestampAddedInCleanupSection, &timestamp);

        delete cmdComputeKernel;
        delete parentKernel;
    }
}

HWTEST_F(ParentKernelCommandQueueFixture, givenParentKernelWhenCommandIsSubmittedThenSchedulerIsDispatched) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        MockParentKernel *parentKernel = MockParentKernel::create(*device);
        MockDeviceQueueHwWithCriticalSectionRelease<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        size_t heapSize = 20;
        size_t alignement = 64;
        size_t dshSize = heapSize + mockDevQueue.getDshOffset();
        IndirectHeap *dsh = new IndirectHeap(alignedMalloc(dshSize, alignement), dshSize);
        dsh->getSpace(mockDevQueue.getDshOffset());

        KernelOperation *blockedCommandData = new KernelOperation(std::unique_ptr<LinearStream>(new LinearStream()),
                                                                  std::unique_ptr<IndirectHeap>(dsh),
                                                                  std::unique_ptr<IndirectHeap>(new IndirectHeap(alignedMalloc(heapSize, alignement), heapSize)),
                                                                  std::unique_ptr<IndirectHeap>(new IndirectHeap(alignedMalloc(heapSize, alignement), heapSize)));

        size_t minSizeSSHForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        PreemptionMode preemptionMode = device->getPreemptionMode();
        std::vector<Surface *> surfaces;
        auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, device->getCommandStreamReceiver(),
                                                          std::unique_ptr<KernelOperation>(blockedCommandData), surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

        cmdComputeKernel->submit(0, false);

        EXPECT_TRUE(mockDevQueue.schedulerDispatched);

        delete cmdComputeKernel;
        delete parentKernel;
    }
}

HWTEST_F(ParentKernelCommandQueueFixture, givenUsedSSHWhenParentKernelIsSubmittedThenNewSSHIsAllocated) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        MockParentKernel *parentKernel = MockParentKernel::create(*device);
        MockDeviceQueueHw<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        MockCommandQueue cmdQ(context, device, properties);

        size_t minSizeSSHForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        size_t heapSize = 20;
        size_t alignement = 64;

        size_t dshSize = heapSize + mockDevQueue.getDshOffset();
        IndirectHeap *dsh = new IndirectHeap(alignedMalloc(dshSize, alignement), dshSize);
        dsh->getSpace(mockDevQueue.getDshOffset());

        cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 100);
        // use some SSH
        cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE).getSpace(4);

        KernelOperation *blockedCommandData = new KernelOperation(std::unique_ptr<LinearStream>(new LinearStream()),
                                                                  std::unique_ptr<IndirectHeap>(dsh),
                                                                  std::unique_ptr<IndirectHeap>(new IndirectHeap(alignedMalloc(heapSize, alignement), heapSize)),
                                                                  std::unique_ptr<IndirectHeap>(new IndirectHeap(alignedMalloc(heapSize, alignement), heapSize)));

        blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        PreemptionMode preemptionMode = device->getPreemptionMode();
        std::vector<Surface *> surfaces;
        auto *cmdComputeKernel = new CommandComputeKernel(cmdQ, device->getCommandStreamReceiver(),
                                                          std::unique_ptr<KernelOperation>(blockedCommandData), surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

        cmdComputeKernel->submit(0, false);

        EXPECT_TRUE(cmdQ.releaseIndirectHeapCalled);

        delete cmdComputeKernel;
        delete parentKernel;
    }
}

HWTEST_F(ParentKernelCommandQueueFixture, givenNotUsedSSHWhenParentKernelIsSubmittedThenExistingSSHIsUsed) {
    if (device->getSupportedClVersion() >= 20) {
        cl_queue_properties properties[3] = {0};
        MockParentKernel *parentKernel = MockParentKernel::create(*device);
        MockDeviceQueueHw<FamilyType> mockDevQueue(context, device, properties[0]);
        parentKernel->createReflectionSurface();
        context->setDefaultDeviceQueue(&mockDevQueue);

        size_t minSizeSSHForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*parentKernel);

        size_t heapSize = 20;
        size_t alignement = 64;

        size_t dshSize = heapSize + mockDevQueue.getDshOffset();
        IndirectHeap *dsh = new IndirectHeap(alignedMalloc(dshSize, alignement), dshSize);
        dsh->getSpace(mockDevQueue.getDshOffset());

        size_t sshSize = 1000;
        IndirectHeap *ssh = new IndirectHeap(alignedMalloc(sshSize, 4096), sshSize);

        EXPECT_EQ(0u, ssh->getUsed());

        pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, sshSize);

        void *sshBuffer = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE).getCpuBase();

        KernelOperation *blockedCommandData = new KernelOperation(std::unique_ptr<LinearStream>(new LinearStream()),
                                                                  std::unique_ptr<IndirectHeap>(dsh),
                                                                  std::unique_ptr<IndirectHeap>(new IndirectHeap(alignedMalloc(heapSize, alignement), heapSize)),
                                                                  std::unique_ptr<IndirectHeap>(ssh));

        blockedCommandData->surfaceStateHeapSizeEM = minSizeSSHForEM;
        PreemptionMode preemptionMode = device->getPreemptionMode();
        std::vector<Surface *> surfaces;
        auto *cmdComputeKernel = new CommandComputeKernel(*pCmdQ, device->getCommandStreamReceiver(),
                                                          std::unique_ptr<KernelOperation>(blockedCommandData), surfaces, false, false, false, nullptr, preemptionMode, parentKernel, 1);

        cmdComputeKernel->submit(0, false);

        void *newSshBuffer = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE).getCpuBase();

        EXPECT_EQ(sshBuffer, newSshBuffer);

        delete cmdComputeKernel;
        delete parentKernel;
    }
}
