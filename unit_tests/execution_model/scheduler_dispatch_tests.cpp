/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/built_ins/built_ins.h"
#include "runtime/command_queue/enqueue_kernel.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/memory_manager/surface.h"
#include "runtime/scheduler/scheduler_kernel.h"
#include "unit_tests/fixtures/execution_model_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device_queue.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"

using namespace NEO;

class ExecutionModelSchedulerFixture : public ExecutionModelSchedulerTest,
                                       public testing::Test {
  public:
    void SetUp() override {
        ExecutionModelSchedulerTest::SetUp();
    }

    void TearDown() override {
        ExecutionModelSchedulerTest::TearDown();
    }
};

HWCMDTEST_F(IGFX_GEN8_CORE, ExecutionModelSchedulerFixture, dispatchScheduler) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    if (pDevice->getSupportedClVersion() >= 20) {
        DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);
        SchedulerKernel &scheduler = pDevice->getExecutionEnvironment()->getBuiltIns()->getSchedulerKernel(*context);

        auto *executionModelDshAllocation = pDevQueueHw->getDshBuffer();
        auto *dshHeap = pDevQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE);
        void *executionModelDsh = executionModelDshAllocation->getUnderlyingBuffer();

        EXPECT_NE(nullptr, executionModelDsh);

        size_t minRequiredSizeForSchedulerSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredForExecutionModel(IndirectHeap::SURFACE_STATE, *parentKernel);
        // Setup heaps in pCmdQ
        LinearStream &commandStream = getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*pCmdQ, false, false, &scheduler);
        pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, minRequiredSizeForSchedulerSSH);

        GpgpuWalkerHelper<FamilyType>::dispatchScheduler(
            pCmdQ->getCS(0),
            *pDevQueueHw,
            pDevice->getPreemptionMode(),
            scheduler,
            &pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u),
            pDevQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE));

        EXPECT_EQ(0u, *scheduler.globalWorkOffsetX);
        EXPECT_EQ(0u, *scheduler.globalWorkOffsetY);
        EXPECT_EQ(0u, *scheduler.globalWorkOffsetZ);

        EXPECT_EQ((uint32_t)scheduler.getLws(), *scheduler.localWorkSizeX);
        EXPECT_EQ(1u, *scheduler.localWorkSizeY);
        EXPECT_EQ(1u, *scheduler.localWorkSizeZ);

        EXPECT_EQ((uint32_t)scheduler.getLws(), *scheduler.localWorkSizeX2);
        EXPECT_EQ(1u, *scheduler.localWorkSizeY2);
        EXPECT_EQ(1u, *scheduler.localWorkSizeZ2);

        if (scheduler.enqueuedLocalWorkSizeX != &Kernel::dummyPatchLocation) {
            EXPECT_EQ((uint32_t)scheduler.getLws(), *scheduler.enqueuedLocalWorkSizeX);
        }
        EXPECT_EQ(1u, *scheduler.enqueuedLocalWorkSizeY);
        EXPECT_EQ(1u, *scheduler.enqueuedLocalWorkSizeZ);

        EXPECT_EQ((uint32_t)(scheduler.getGws() / scheduler.getLws()), *scheduler.numWorkGroupsX);
        EXPECT_EQ(0u, *scheduler.numWorkGroupsY);
        EXPECT_EQ(0u, *scheduler.numWorkGroupsZ);

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(commandStream, 0);
        hwParser.findHardwareCommands<FamilyType>();

        ASSERT_NE(hwParser.cmdList.end(), hwParser.itorWalker);

        // Before Walker There must be PC
        PIPE_CONTROL *pc = hwParser.getCommand<PIPE_CONTROL>(hwParser.cmdList.begin(), hwParser.itorWalker);
        ASSERT_NE(nullptr, pc);

        ASSERT_NE(hwParser.cmdList.end(), hwParser.itorMediaInterfaceDescriptorLoad);
        auto *interfaceDescLoad = (MEDIA_INTERFACE_DESCRIPTOR_LOAD *)*hwParser.itorMediaInterfaceDescriptorLoad;

        uint32_t addressOffsetProgrammed = interfaceDescLoad->getInterfaceDescriptorDataStartAddress();
        uint32_t interfaceDescriptorSizeProgrammed = interfaceDescLoad->getInterfaceDescriptorTotalLength();

        uint32_t addressOffsetExpected = pDevQueueHw->colorCalcStateSize;
        uint32_t intDescSizeExpected = DeviceQueue::interfaceDescriptorEntries * sizeof(INTERFACE_DESCRIPTOR_DATA);

        EXPECT_EQ(addressOffsetExpected, addressOffsetProgrammed);
        EXPECT_EQ(intDescSizeExpected, interfaceDescriptorSizeProgrammed);

        auto *walker = (GPGPU_WALKER *)*hwParser.itorWalker;

        size_t workGroups[3] = {(scheduler.getGws() / scheduler.getLws()), 1, 1};

        size_t numWorkgroupsProgrammed[3] = {0, 0, 0};

        uint32_t threadsPerWorkGroup = walker->getThreadWidthCounterMaximum();

        EXPECT_EQ(scheduler.getLws() / scheduler.getKernelInfo().getMaxSimdSize(), threadsPerWorkGroup);

        numWorkgroupsProgrammed[0] = walker->getThreadGroupIdXDimension();
        numWorkgroupsProgrammed[1] = walker->getThreadGroupIdYDimension();
        numWorkgroupsProgrammed[2] = walker->getThreadGroupIdZDimension();

        EXPECT_EQ(workGroups[0], numWorkgroupsProgrammed[0]);
        EXPECT_EQ(workGroups[1], numWorkgroupsProgrammed[1]);
        EXPECT_EQ(workGroups[2], numWorkgroupsProgrammed[2]);

        typename FamilyType::GPGPU_WALKER::SIMD_SIZE simdSize = walker->getSimdSize();
        EXPECT_EQ(FamilyType::GPGPU_WALKER::SIMD_SIZE::SIMD_SIZE_SIMD8, simdSize);

        EXPECT_EQ(0u, walker->getThreadGroupIdStartingX());
        EXPECT_EQ(0u, walker->getThreadGroupIdStartingY());
        EXPECT_EQ(0u, walker->getThreadGroupIdStartingResumeZ());

        uint32_t offsetCrossThreadDataProgrammed = walker->getIndirectDataStartAddress();
        assert(offsetCrossThreadDataProgrammed % 64 == 0);
        size_t curbeSize = scheduler.getCurbeSize();
        size_t offsetCrossThreadDataExpected = dshHeap->getMaxAvailableSpace() - curbeSize - 4096; // take additional page for padding into account
        EXPECT_EQ((uint32_t)offsetCrossThreadDataExpected, offsetCrossThreadDataProgrammed);

        EXPECT_EQ(62u, walker->getInterfaceDescriptorOffset());

        auto numChannels = 3;
        auto sizePerThreadDataTotal = PerThreadDataHelper::getPerThreadDataSizeTotal(scheduler.getKernelInfo().getMaxSimdSize(), numChannels, scheduler.getLws());

        auto sizeCrossThreadData = scheduler.getCrossThreadDataSize();
        auto IndirectDataLength = alignUp((uint32_t)(sizeCrossThreadData + sizePerThreadDataTotal), GPGPU_WALKER::INDIRECTDATASTARTADDRESS_ALIGN_SIZE);
        EXPECT_EQ(IndirectDataLength, walker->getIndirectDataLength());

        ASSERT_NE(hwParser.cmdList.end(), hwParser.itorBBStartAfterWalker);
        auto *bbStart = (MI_BATCH_BUFFER_START *)*hwParser.itorBBStartAfterWalker;

        uint64_t slbAddress = pDevQueueHw->getSlbBuffer()->getGpuAddress();
        EXPECT_EQ(slbAddress, bbStart->getBatchBufferStartAddressGraphicsaddress472());
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, ExecutionModelSchedulerFixture, dispatchSchedulerDoesNotUseStandardCmdQIOH) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    if (pDevice->getSupportedClVersion() >= 20) {
        DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);
        SchedulerKernel &scheduler = pDevice->getExecutionEnvironment()->getBuiltIns()->getSchedulerKernel(*context);

        size_t minRequiredSizeForSchedulerSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredForExecutionModel(IndirectHeap::SURFACE_STATE, *parentKernel);
        // Setup heaps in pCmdQ
        getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*pCmdQ, false, false, &scheduler);
        pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, minRequiredSizeForSchedulerSSH);

        GpgpuWalkerHelper<FamilyType>::dispatchScheduler(
            pCmdQ->getCS(0),
            *pDevQueueHw,
            pDevice->getPreemptionMode(),
            scheduler,
            &pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u),
            pDevQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE));

        auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);

        EXPECT_EQ(0u, ioh.getUsed());
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, ParentKernelCommandQueueFixture, dispatchSchedulerWithEarlyReturnSetToFirstInstanceDoesNotPutBBStartCmd) {

    if (device->getSupportedClVersion() >= 20) {

        cl_queue_properties properties[3] = {0};
        MockDeviceQueueHw<FamilyType> mockDevQueue(context, device, properties[0]);

        auto *igilQueue = mockDevQueue.getIgilQueue();

        ASSERT_NE(nullptr, igilQueue);
        igilQueue->m_controls.m_SchedulerEarlyReturn = 1;

        SchedulerKernel &scheduler = device->getExecutionEnvironment()->getBuiltIns()->getSchedulerKernel(*context);

        size_t minRequiredSizeForSchedulerSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(scheduler);
        // Setup heaps in pCmdQ
        LinearStream &commandStream = getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*pCmdQ, false, false, &scheduler);
        pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, minRequiredSizeForSchedulerSSH);

        GpgpuWalkerHelper<FamilyType>::dispatchScheduler(
            pCmdQ->getCS(0),
            mockDevQueue,
            device->getPreemptionMode(),
            scheduler,
            &pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u),
            mockDevQueue.getIndirectHeap(IndirectHeap::DYNAMIC_STATE));

        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(commandStream, 0);
        hwParser.findHardwareCommands<FamilyType>();

        EXPECT_NE(hwParser.cmdList.end(), hwParser.itorWalker);
        EXPECT_EQ(hwParser.cmdList.end(), hwParser.itorBBStartAfterWalker);
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, ExecutionModelSchedulerFixture, ForceDispatchSchedulerEnqueuesSchedulerKernel) {

    if (pDevice->getSupportedClVersion() >= 20) {
        DebugManagerStateRestore dbgRestorer;

        DebugManager.flags.ForceDispatchScheduler.set(true);

        size_t offset[3] = {0, 0, 0};
        size_t gws[3] = {1, 1, 1};

        MockCommandQueueHw<FamilyType> *mockCmdQ = new MockCommandQueueHw<FamilyType>(context, pDevice, 0);

        mockCmdQ->enqueueKernel(parentKernel, 1, offset, gws, gws, 0, nullptr, nullptr);

        EXPECT_TRUE(mockCmdQ->lastEnqueuedKernels.front()->isSchedulerKernel);

        delete mockCmdQ;
    }
}
