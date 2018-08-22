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

#include "runtime/command_queue/enqueue_kernel.h"
#include "runtime/event/perf_counter.h"
#include "runtime/kernel/kernel.h"
#include "runtime/sampler/sampler.h"
#include "unit_tests/fixtures/execution_model_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_mdi.h"

using namespace OCLRT;

static const char *binaryFile = "simple_block_kernel";
static const char *KernelNames[] = {"kernel_reflection", "simple_block_kernel"};

typedef ExecutionModelKernelTest ParentKernelDispatchTest;

HWTEST_P(ParentKernelDispatchTest, givenParentKernelWhenQueueIsNotBlockedThenDeviceQueueDSHIsUsed) {
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {
        DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);

        KernelOperation *blockedCommandsData = nullptr;
        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        pKernel->createReflectionSurface();

        size_t dshUsedBefore = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u).getUsed();
        EXPECT_EQ(0u, dshUsedBefore);

        size_t executionModelDSHUsedBefore = pDevQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE)->getUsed();

        DispatchInfo dispatchInfo(pKernel, 1, workItems, nullptr, globalOffsets);
        MultiDispatchInfo multiDispatchInfo(pKernel);
        multiDispatchInfo.push(dispatchInfo);
        GpgpuWalkerHelper<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            0,
            nullptr,
            &blockedCommandsData,
            nullptr,
            nullptr,
            pDevice->getPreemptionMode(),
            false);

        size_t dshUsedAfter = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u).getUsed();
        EXPECT_EQ(0u, dshUsedAfter);

        size_t executionModelDSHUsedAfter = pDevQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE)->getUsed();
        EXPECT_NE(executionModelDSHUsedBefore, executionModelDSHUsedAfter);
    }
}

HWTEST_P(ParentKernelDispatchTest, givenParentKernelWhenDynamicStateHeapIsRequestedThenDeviceQueueHeapIsReturned) {
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {
        DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);

        MockMultiDispatchInfo multiDispatchInfo(pKernel);
        auto ish = &getIndirectHeap<FamilyType, IndirectHeap::DYNAMIC_STATE>(*pCmdQ, multiDispatchInfo);
        auto ishOfDevQueue = pDevQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE);

        EXPECT_EQ(ishOfDevQueue, ish);
    }
}

HWTEST_P(ParentKernelDispatchTest, givenParentKernelWhenIndirectObjectHeapIsRequestedThenDeviceQueueDSHIsReturned) {
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {
        DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);

        MockMultiDispatchInfo multiDispatchInfo(pKernel);
        auto ioh = &getIndirectHeap<FamilyType, IndirectHeap::INDIRECT_OBJECT>(*pCmdQ, multiDispatchInfo);
        auto dshOfDevQueue = pDevQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE);

        EXPECT_EQ(dshOfDevQueue, ioh);
    }
}

HWTEST_P(ParentKernelDispatchTest, givenParentKernelWhenQueueIsNotBlockedThenDefaultCmdQIOHIsNotUsed) {
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {
        KernelOperation *blockedCommandsData = nullptr;
        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        MockMultiDispatchInfo multiDispatchInfo(pKernel);

        auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);

        DispatchInfo dispatchInfo(pKernel, 1, workItems, nullptr, globalOffsets);
        multiDispatchInfo.push(dispatchInfo);
        GpgpuWalkerHelper<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            0,
            nullptr,
            &blockedCommandsData,
            nullptr,
            nullptr,
            pDevice->getPreemptionMode(),
            false);

        auto iohUsed = ioh.getUsed();
        EXPECT_EQ(0u, iohUsed);
    }
}

HWTEST_P(ParentKernelDispatchTest, givenParentKernelWhenQueueIsNotBlockedThenSSHSizeAccountForsBlocksSurfaceStates) {
    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {
        KernelOperation *blockedCommandsData = nullptr;
        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        MockMultiDispatchInfo multiDispatchInfo(pKernel);
        DispatchInfo dispatchInfo(pKernel, 1, workItems, nullptr, globalOffsets);
        multiDispatchInfo.push(dispatchInfo);
        GpgpuWalkerHelper<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            0,
            nullptr,
            &blockedCommandsData,
            nullptr,
            nullptr,
            pDevice->getPreemptionMode(),
            false);

        auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);

        EXPECT_LE(pKernel->getKernelInfo().heapInfo.pKernelHeader->SurfaceStateHeapSize, ssh.getMaxAvailableSpace());

        size_t minRequiredSize = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);
        size_t minRequiredSizeForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*pKernel);

        EXPECT_LE(minRequiredSize + minRequiredSizeForEM, ssh.getMaxAvailableSpace());
    }
}

HWTEST_P(ParentKernelDispatchTest, givenParentKernelWhenQueueIsBlockedThenSSHSizeForParentIsAllocated) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {
        KernelOperation *blockedCommandsData = nullptr;
        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        MultiDispatchInfo multiDispatchInfo(pKernel);

        DispatchInfo dispatchInfo(pKernel, 1, workItems, nullptr, globalOffsets);
        multiDispatchInfo.push(dispatchInfo);
        GpgpuWalkerHelper<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            0,
            nullptr,
            &blockedCommandsData,
            nullptr,
            nullptr,
            pDevice->getPreemptionMode(),
            true);
        ASSERT_NE(nullptr, blockedCommandsData);

        size_t minRequiredSize = KernelCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);
        size_t minRequiredSizeForEM = KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*pKernel);

        size_t sshUsed = blockedCommandsData->ssh->getUsed();

        size_t expectedSizeSSH = pKernel->getNumberOfBindingTableStates() * sizeof(RENDER_SURFACE_STATE) + pKernel->getKernelInfo().patchInfo.bindingTableState->Count * sizeof(BINDING_TABLE_STATE);

        if ((pKernel->requiresSshForBuffers()) || (pKernel->getKernelInfo().patchInfo.imageMemObjKernelArgs.size() > 0)) {
            EXPECT_EQ(expectedSizeSSH, sshUsed);
        }

        EXPECT_GE(minRequiredSize, sshUsed);
        // Total SSH size including EM must be greater then ssh allocated
        EXPECT_GT(minRequiredSize + minRequiredSizeForEM, sshUsed);

        delete blockedCommandsData;
    }
}

INSTANTIATE_TEST_CASE_P(ParentKernelDispatchTest,
                        ParentKernelDispatchTest,
                        ::testing::Combine(
                            ::testing::Values(binaryFile),
                            ::testing::ValuesIn(KernelNames)));

typedef ParentKernelCommandQueueFixture ParentKernelCommandStreamFixture;

HWTEST_F(ParentKernelCommandStreamFixture, GivenDispatchInfoWithParentKernelWhenCommandStreamIsAcquiredThenSizeAccountsForSchedulerDispatch) {

    if (device->getSupportedClVersion() >= 20) {
        MockParentKernel *mockParentKernel = MockParentKernel::create(*context);

        DispatchInfo dispatchInfo(mockParentKernel, 1, Vec3<size_t>{24, 1, 1}, Vec3<size_t>{24, 1, 1}, Vec3<size_t>{0, 0, 0});
        MultiDispatchInfo multiDispatchInfo(mockParentKernel);

        size_t size = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *pCmdQ, mockParentKernel);
        size_t numOfKernels = MemoryConstants::pageSize / size;

        size_t rest = MemoryConstants::pageSize - (numOfKernels * size);

        SchedulerKernel &scheduler = device->getExecutionEnvironment()->getBuiltIns()->getSchedulerKernel(*mockParentKernel->getContext());
        size_t schedulerSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *pCmdQ, &scheduler);

        while (rest >= schedulerSize) {
            numOfKernels++;
            rest = alignUp(numOfKernels * size, MemoryConstants::pageSize) - numOfKernels * size;
        }

        for (size_t i = 0; i < numOfKernels; i++) {
            multiDispatchInfo.push(dispatchInfo);
        }

        size_t totalKernelSize = alignUp(numOfKernels * size, MemoryConstants::pageSize);

        LinearStream &commandStream = getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*pCmdQ, false, false, multiDispatchInfo);

        EXPECT_LT(totalKernelSize, commandStream.getMaxAvailableSpace());

        delete mockParentKernel;
    }
}

class MockParentKernelDispatch : public ExecutionModelSchedulerTest,
                                 public testing::Test {
    void SetUp() override {
        ExecutionModelSchedulerTest::SetUp();
    }

    void TearDown() override {
        ExecutionModelSchedulerTest::TearDown();
    }
};

HWTEST_F(MockParentKernelDispatch, GivenBlockedQueueWhenParentKernelIsDispatchedThenDshHeapForIndirectObjectHeapIsUsed) {

    if (pDevice->getSupportedClVersion() >= 20) {
        MockParentKernel *mockParentKernel = MockParentKernel::create(*context);

        KernelOperation *blockedCommandsData = nullptr;
        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        DispatchInfo dispatchInfo(mockParentKernel, 1, workItems, nullptr, globalOffsets);
        MultiDispatchInfo multiDispatchInfo(mockParentKernel);
        multiDispatchInfo.push(dispatchInfo);
        GpgpuWalkerHelper<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            0,
            nullptr,
            &blockedCommandsData,
            nullptr,
            nullptr,
            pDevice->getPreemptionMode(),
            true);

        ASSERT_NE(nullptr, blockedCommandsData);

        EXPECT_EQ(blockedCommandsData->dsh.get(), blockedCommandsData->ioh.get());
        delete blockedCommandsData;
        delete mockParentKernel;
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, MockParentKernelDispatch, GivenParentKernelWhenDispatchedThenMediaInterfaceDescriptorLoadIsCorrectlyProgrammed) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    if (pDevice->getSupportedClVersion() >= 20) {
        MockParentKernel *mockParentKernel = MockParentKernel::create(*context);

        KernelOperation *blockedCommandsData = nullptr;
        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        DispatchInfo dispatchInfo(mockParentKernel, 1, workItems, nullptr, globalOffsets);
        MultiDispatchInfo multiDispatchInfo(mockParentKernel);
        multiDispatchInfo.push(dispatchInfo);
        GpgpuWalkerHelper<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            0,
            nullptr,
            &blockedCommandsData,
            nullptr,
            nullptr,
            pDevice->getPreemptionMode(),
            false);

        LinearStream *commandStream = &pCmdQ->getCS(0);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(*commandStream, 0);
        hwParser.findHardwareCommands<FamilyType>();

        ASSERT_NE(hwParser.cmdList.end(), hwParser.itorMediaInterfaceDescriptorLoad);

        auto pCmd = (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)hwParser.getCommand<MEDIA_INTERFACE_DESCRIPTOR_LOAD>(hwParser.cmdList.begin(), hwParser.itorWalker);

        ASSERT_NE(nullptr, pCmd);

        uint32_t offsetInterfaceDescriptorData = DeviceQueue::colorCalcStateSize;
        uint32_t sizeInterfaceDescriptorData = sizeof(INTERFACE_DESCRIPTOR_DATA);

        EXPECT_EQ(offsetInterfaceDescriptorData, pCmd->getInterfaceDescriptorDataStartAddress());
        EXPECT_EQ(sizeInterfaceDescriptorData, pCmd->getInterfaceDescriptorTotalLength());

        delete mockParentKernel;
    }
}

HWTEST_F(MockParentKernelDispatch, GivenUsedSSHHeapWhenParentKernelIsDispatchedThenNewSSHIsAllocated) {

    if (pDevice->getSupportedClVersion() >= 20) {
        MockParentKernel *mockParentKernel = MockParentKernel::create(*context);

        KernelOperation *blockedCommandsData = nullptr;
        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 100);

        ssh.getSpace(20);

        EXPECT_EQ(20u, ssh.getUsed());

        // Assuming parent is not using SSH, this is becuase storing allocation on reuse list and allocating
        // new one by obtaining from reuse list returns the same allocation and heap buffer does not differ
        // If parent is not using SSH, then heap obtained has zero usage and the same buffer
        ASSERT_EQ(0u, mockParentKernel->getKernelInfo().heapInfo.pKernelHeader->SurfaceStateHeapSize);

        DispatchInfo dispatchInfo(mockParentKernel, 1, workItems, nullptr, globalOffsets);
        MultiDispatchInfo multiDispatchInfo(mockParentKernel);
        multiDispatchInfo.push(dispatchInfo);
        GpgpuWalkerHelper<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            0,
            nullptr,
            &blockedCommandsData,
            nullptr,
            nullptr,
            pDevice->getPreemptionMode(),
            false);

        EXPECT_EQ(0u, ssh.getUsed());

        delete mockParentKernel;
    }
}

HWTEST_F(MockParentKernelDispatch, GivenNotUsedSSHHeapWhenParentKernelIsDispatchedThenExistingSSHIsUsed) {

    if (pDevice->getSupportedClVersion() >= 20) {
        MockParentKernel *mockParentKernel = MockParentKernel::create(*context);

        KernelOperation *blockedCommandsData = nullptr;
        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 100);

        EXPECT_EQ(0u, ssh.getUsed());

        auto *bufferMemory = ssh.getCpuBase();

        DispatchInfo dispatchInfo(mockParentKernel, 1, workItems, nullptr, globalOffsets);
        MultiDispatchInfo multiDispatchInfo;
        multiDispatchInfo.push(dispatchInfo);
        GpgpuWalkerHelper<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            0,
            nullptr,
            &blockedCommandsData,
            nullptr,
            nullptr,
            pDevice->getPreemptionMode(),
            false);

        EXPECT_EQ(bufferMemory, ssh.getCpuBase());

        delete mockParentKernel;
    }
}
