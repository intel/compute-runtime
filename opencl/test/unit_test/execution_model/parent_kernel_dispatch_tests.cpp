/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"

#include "opencl/source/command_queue/enqueue_kernel.h"
#include "opencl/source/command_queue/hardware_interface.h"
#include "opencl/source/event/perf_counter.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/sampler/sampler.h"
#include "opencl/test/unit_test/fixtures/execution_model_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_mdi.h"
#include "opencl/test/unit_test/mocks/mock_program.h"

using namespace NEO;

static const char *binaryFile = "simple_block_kernel";
static const char *KernelNames[] = {"kernel_reflection", "simple_block_kernel"};

typedef ExecutionModelKernelTest ParentKernelDispatchTest;

HWCMDTEST_P(IGFX_GEN8_CORE, ParentKernelDispatchTest, givenParentKernelWhenQueueIsNotBlockedThenDeviceQueueDSHIsUsed) {
    DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);

    KernelOperation *blockedCommandsData = nullptr;
    const size_t globalOffsets[3] = {0, 0, 0};
    const size_t workItems[3] = {1, 1, 1};

    pKernel->createReflectionSurface();

    size_t dshUsedBefore = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u).getUsed();
    EXPECT_EQ(0u, dshUsedBefore);

    size_t executionModelDSHUsedBefore = pDevQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE)->getUsed();

    DispatchInfo dispatchInfo(pClDevice, pKernel, 1, workItems, nullptr, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
    MultiDispatchInfo multiDispatchInfo(pKernel);
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        blockedCommandsData,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    size_t dshUsedAfter = pCmdQ->getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 0u).getUsed();
    EXPECT_EQ(0u, dshUsedAfter);

    size_t executionModelDSHUsedAfter = pDevQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE)->getUsed();
    EXPECT_NE(executionModelDSHUsedBefore, executionModelDSHUsedAfter);
}

HWCMDTEST_P(IGFX_GEN8_CORE, ParentKernelDispatchTest, givenParentKernelWhenDynamicStateHeapIsRequestedThenDeviceQueueHeapIsReturned) {
    DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, pKernel);
    auto ish = &getIndirectHeap<FamilyType, IndirectHeap::DYNAMIC_STATE>(*pCmdQ, multiDispatchInfo);
    auto ishOfDevQueue = pDevQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE);

    EXPECT_EQ(ishOfDevQueue, ish);
}

HWCMDTEST_P(IGFX_GEN8_CORE, ParentKernelDispatchTest, givenParentKernelWhenIndirectObjectHeapIsRequestedThenDeviceQueueDSHIsReturned) {
    DeviceQueueHw<FamilyType> *pDevQueueHw = castToObject<DeviceQueueHw<FamilyType>>(pDevQueue);

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, pKernel);
    auto ioh = &getIndirectHeap<FamilyType, IndirectHeap::INDIRECT_OBJECT>(*pCmdQ, multiDispatchInfo);
    auto dshOfDevQueue = pDevQueueHw->getIndirectHeap(IndirectHeap::DYNAMIC_STATE);

    EXPECT_EQ(dshOfDevQueue, ioh);
}

HWCMDTEST_P(IGFX_GEN8_CORE, ParentKernelDispatchTest, givenParentKernelWhenQueueIsNotBlockedThenDefaultCmdQIOHIsNotUsed) {
    KernelOperation *blockedCommandsData = nullptr;
    const size_t globalOffsets[3] = {0, 0, 0};
    const size_t workItems[3] = {1, 1, 1};

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, pKernel);

    auto &ioh = pCmdQ->getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 0u);

    DispatchInfo dispatchInfo(pClDevice, pKernel, 1, workItems, nullptr, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        blockedCommandsData,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    auto iohUsed = ioh.getUsed();
    EXPECT_EQ(0u, iohUsed);
}

HWCMDTEST_P(IGFX_GEN8_CORE, ParentKernelDispatchTest, givenParentKernelWhenQueueIsNotBlockedThenSSHSizeAccountForsBlocksSurfaceStates) {
    KernelOperation *blockedCommandsData = nullptr;
    const size_t globalOffsets[3] = {0, 0, 0};
    const size_t workItems[3] = {1, 1, 1};

    MockMultiDispatchInfo multiDispatchInfo(pClDevice, pKernel);
    DispatchInfo dispatchInfo(pClDevice, pKernel, 1, workItems, nullptr, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
    multiDispatchInfo.push(dispatchInfo);
    HardwareInterface<FamilyType>::dispatchWalker(
        *pCmdQ,
        multiDispatchInfo,
        CsrDependencies(),
        blockedCommandsData,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        CL_COMMAND_NDRANGE_KERNEL);

    auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 0u);

    EXPECT_LE(pKernel->getKernelInfo().heapInfo.SurfaceStateHeapSize, ssh.getMaxAvailableSpace());

    size_t minRequiredSize = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo);
    size_t minRequiredSizeForEM = HardwareCommandsHelper<FamilyType>::getSshSizeForExecutionModel(*pKernel);

    EXPECT_LE(minRequiredSize + minRequiredSizeForEM, ssh.getMaxAvailableSpace());
}

HWCMDTEST_P(IGFX_GEN8_CORE, ParentKernelDispatchTest, givenParentKernelWhenQueueIsBlockedThenSSHSizeForParentIsAllocated) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto blockedCommandsData = createBlockedCommandsData(*pCmdQ);
    const size_t globalOffsets[3] = {0, 0, 0};
    const size_t workItems[3] = {1, 1, 1};

    MultiDispatchInfo multiDispatchInfo(pKernel);

    DispatchInfo dispatchInfo(pClDevice, pKernel, 1, workItems, nullptr, globalOffsets);
    dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
    dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
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
    ASSERT_NE(nullptr, blockedCommandsData);

    size_t minRequiredSize = HardwareCommandsHelper<FamilyType>::getTotalSizeRequiredSSH(multiDispatchInfo) + UnitTestHelper<FamilyType>::getDefaultSshUsage();
    size_t minRequiredSizeForEM = HardwareCommandsHelper<FamilyType>::getSshSizeForExecutionModel(*pKernel);

    size_t sshUsed = blockedCommandsData->ssh->getUsed();

    size_t expectedSizeSSH = pKernel->getNumberOfBindingTableStates() * sizeof(RENDER_SURFACE_STATE) +
                             pKernel->getKernelInfo().kernelDescriptor.payloadMappings.bindingTable.numEntries * sizeof(BINDING_TABLE_STATE) +
                             UnitTestHelper<FamilyType>::getDefaultSshUsage();

    if (pKernel->usesBindfulAddressingForBuffers() || pKernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.usesImages) {
        EXPECT_EQ(expectedSizeSSH, sshUsed);
    }

    EXPECT_GE(minRequiredSize, sshUsed);
    // Total SSH size including EM must be greater then ssh allocated
    EXPECT_GT(minRequiredSize + minRequiredSizeForEM, sshUsed);
}

INSTANTIATE_TEST_CASE_P(ParentKernelDispatchTest,
                        ParentKernelDispatchTest,
                        ::testing::Combine(
                            ::testing::Values(binaryFile),
                            ::testing::ValuesIn(KernelNames)));

typedef ParentKernelCommandQueueFixture ParentKernelCommandStreamFixture;

HWCMDTEST_F(IGFX_GEN8_CORE, ParentKernelCommandStreamFixture, GivenDispatchInfoWithParentKernelWhenCommandStreamIsAcquiredThenSizeAccountsForSchedulerDispatch) {
    REQUIRE_OCL_21_OR_SKIP(defaultHwInfo);

    MockParentKernel *mockParentKernel = MockParentKernel::create(*context);

    DispatchInfo dispatchInfo(device, mockParentKernel, 1, Vec3<size_t>{24, 1, 1}, Vec3<size_t>{24, 1, 1}, Vec3<size_t>{0, 0, 0});
    MultiDispatchInfo multiDispatchInfo(mockParentKernel);

    size_t size = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *pCmdQ, mockParentKernel, {});
    size_t numOfKernels = MemoryConstants::pageSize / size;

    size_t rest = MemoryConstants::pageSize - (numOfKernels * size);

    SchedulerKernel &scheduler = pCmdQ->getContext().getSchedulerKernel();
    size_t schedulerSize = EnqueueOperation<FamilyType>::getSizeRequiredCS(CL_COMMAND_NDRANGE_KERNEL, false, false, *pCmdQ, &scheduler, {});

    while (rest >= schedulerSize) {
        numOfKernels++;
        rest = alignUp(numOfKernels * size, MemoryConstants::pageSize) - numOfKernels * size;
    }

    for (size_t i = 0; i < numOfKernels; i++) {
        multiDispatchInfo.push(dispatchInfo);
    }

    size_t totalKernelSize = alignUp(numOfKernels * size, MemoryConstants::pageSize);

    LinearStream &commandStream = getCommandStream<FamilyType, CL_COMMAND_NDRANGE_KERNEL>(*pCmdQ, CsrDependencies(), false, false,
                                                                                          false, multiDispatchInfo, nullptr, 0, false, false);

    EXPECT_LT(totalKernelSize, commandStream.getMaxAvailableSpace());

    delete mockParentKernel;
}

class MockParentKernelDispatch : public ExecutionModelSchedulerTest,
                                 public testing::Test {
  public:
    void SetUp() override {
        DebugManager.flags.EnableTimestampPacket.set(0);
        ExecutionModelSchedulerTest::SetUp();
    }

    void TearDown() override {
        ExecutionModelSchedulerTest::TearDown();
    }

    std::unique_ptr<KernelOperation> createBlockedCommandsData(CommandQueue &commandQueue) {
        auto commandStream = new LinearStream();

        auto &gpgpuCsr = commandQueue.getGpgpuCommandStreamReceiver();
        gpgpuCsr.ensureCommandBufferAllocation(*commandStream, 1, 1);

        return std::make_unique<KernelOperation>(commandStream, *gpgpuCsr.getInternalAllocationStorage());
    }

    DebugManagerStateRestore dbgRestore;
};

HWCMDTEST_F(IGFX_GEN8_CORE, MockParentKernelDispatch, GivenBlockedQueueWhenParentKernelIsDispatchedThenDshHeapForIndirectObjectHeapIsUsed) {

    if (pClDevice->areOcl21FeaturesSupported()) {
        MockParentKernel *mockParentKernel = MockParentKernel::create(*context);

        auto blockedCommandsData = createBlockedCommandsData(*pCmdQ);
        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        DispatchInfo dispatchInfo(pClDevice, mockParentKernel, 1, workItems, nullptr, globalOffsets);
        dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
        dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
        MultiDispatchInfo multiDispatchInfo(mockParentKernel);
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

        ASSERT_NE(nullptr, blockedCommandsData);

        EXPECT_EQ(blockedCommandsData->dsh.get(), blockedCommandsData->ioh.get());
        delete mockParentKernel;
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, MockParentKernelDispatch, GivenParentKernelWhenDispatchedThenMediaInterfaceDescriptorLoadIsCorrectlyProgrammed) {
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    if (pClDevice->areOcl21FeaturesSupported()) {
        MockParentKernel *mockParentKernel = MockParentKernel::create(*context);

        KernelOperation *blockedCommandsData = nullptr;
        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        DispatchInfo dispatchInfo(pClDevice, mockParentKernel, 1, workItems, nullptr, globalOffsets);
        dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
        dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
        MultiDispatchInfo multiDispatchInfo(mockParentKernel);
        multiDispatchInfo.push(dispatchInfo);
        HardwareInterface<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            CsrDependencies(),
            blockedCommandsData,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            CL_COMMAND_NDRANGE_KERNEL);

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

HWCMDTEST_F(IGFX_GEN8_CORE, MockParentKernelDispatch, GivenUsedSSHHeapWhenParentKernelIsDispatchedThenNewSSHIsAllocated) {

    if (pClDevice->areOcl21FeaturesSupported()) {
        MockParentKernel *mockParentKernel = MockParentKernel::create(*context);

        KernelOperation *blockedCommandsData = nullptr;
        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 100);

        uint32_t testSshUse = 20u;
        uint32_t expectedSshUse = testSshUse + UnitTestHelper<FamilyType>::getDefaultSshUsage();
        ssh.getSpace(testSshUse);
        EXPECT_EQ(expectedSshUse, ssh.getUsed());

        // Assuming parent is not using SSH, this is becuase storing allocation on reuse list and allocating
        // new one by obtaining from reuse list returns the same allocation and heap buffer does not differ
        // If parent is not using SSH, then heap obtained has zero usage and the same buffer
        ASSERT_EQ(0u, mockParentKernel->getKernelInfo().heapInfo.SurfaceStateHeapSize);

        DispatchInfo dispatchInfo(pClDevice, mockParentKernel, 1, workItems, nullptr, globalOffsets);
        dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
        dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
        MultiDispatchInfo multiDispatchInfo(mockParentKernel);
        multiDispatchInfo.push(dispatchInfo);
        HardwareInterface<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            CsrDependencies(),
            blockedCommandsData,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            CL_COMMAND_NDRANGE_KERNEL);

        EXPECT_EQ(0u, ssh.getUsed());

        delete mockParentKernel;
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, MockParentKernelDispatch, GivenNotUsedSSHHeapWhenParentKernelIsDispatchedThenExistingSSHIsUsed) {

    if (pClDevice->areOcl21FeaturesSupported()) {
        MockParentKernel *mockParentKernel = MockParentKernel::create(*context);

        KernelOperation *blockedCommandsData = nullptr;
        const size_t globalOffsets[3] = {0, 0, 0};
        const size_t workItems[3] = {1, 1, 1};

        auto &ssh = pCmdQ->getIndirectHeap(IndirectHeap::SURFACE_STATE, 100);
        auto defaultSshUsage = UnitTestHelper<FamilyType>::getDefaultSshUsage();
        EXPECT_EQ(defaultSshUsage, ssh.getUsed());

        auto *bufferMemory = ssh.getCpuBase();

        DispatchInfo dispatchInfo(pClDevice, mockParentKernel, 1, workItems, nullptr, globalOffsets);
        dispatchInfo.setNumberOfWorkgroups({1, 1, 1});
        dispatchInfo.setTotalNumberOfWorkgroups({1, 1, 1});
        MultiDispatchInfo multiDispatchInfo;
        multiDispatchInfo.push(dispatchInfo);
        HardwareInterface<FamilyType>::dispatchWalker(
            *pCmdQ,
            multiDispatchInfo,
            CsrDependencies(),
            blockedCommandsData,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            CL_COMMAND_NDRANGE_KERNEL);

        EXPECT_EQ(bufferMemory, ssh.getCpuBase());

        delete mockParentKernel;
    }
}
