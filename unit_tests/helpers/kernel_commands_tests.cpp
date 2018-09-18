/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/built_ins/builtins_dispatch_builder.h"
#include "hw_cmds.h"
#include "runtime/command_queue/command_queue_hw.h"
#include "runtime/helpers/basic_math.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/kernel/kernel.h"
#include "unit_tests/fixtures/context_fixture.h"
#include "unit_tests/fixtures/device_fixture.h"
#include "unit_tests/fixtures/image_fixture.h"
#include "unit_tests/fixtures/execution_model_kernel_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/indirect_heap/indirect_heap_fixture.h"
#include "unit_tests/fixtures/built_in_fixture.h"
#include "unit_tests/mocks/mock_kernel.h"
#include "unit_tests/mocks/mock_program.h"
#include "unit_tests/mocks/mock_context.h"
#include "test.h"

#include <memory>

using namespace OCLRT;

struct KernelCommandsTest : DeviceFixture,
                            ContextFixture,
                            BuiltInFixture,
                            ::testing::Test {

    using BuiltInFixture::SetUp;
    using ContextFixture::SetUp;

    void SetUp() override {
        DeviceFixture::SetUp();
        ASSERT_NE(nullptr, pDevice);
        cl_device_id device = pDevice;
        ContextFixture::SetUp(1, &device);
        ASSERT_NE(nullptr, pContext);
        BuiltInFixture::SetUp(pDevice);
        ASSERT_NE(nullptr, pBuiltIns);
    }

    void TearDown() override {
        BuiltInFixture::TearDown();
        ContextFixture::TearDown();
        DeviceFixture::TearDown();
    }

    size_t sizeRequiredCS;
    size_t sizeRequiredISH;
};

HWCMDTEST_F(IGFX_GEN8_CORE, KernelCommandsTest, programInterfaceDescriptorDataResourceUsage) {
    CommandQueueHw<FamilyType> cmdQ(pContext, pDevice, 0);

    std::unique_ptr<Image> srcImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, srcImage.get());
    std::unique_ptr<Image> dstImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, dstImage.get());

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pDevice->getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImageToImage3d,
                                                                                                     cmdQ.getContext(), cmdQ.getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcMemObj = srcImage.get();
    dc.dstMemObj = dstImage.get();
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);
    auto usedIndirectHeapBefore = indirectHeap.getUsed();
    indirectHeap.getSpace(sizeof(INTERFACE_DESCRIPTOR_DATA));

    size_t crossThreadDataSize = kernel->getCrossThreadDataSize();
    KernelCommandsHelper<FamilyType>::sendInterfaceDescriptorData(
        indirectHeap, 0, 0, crossThreadDataSize, 64, 0, 0, 0, 1, 0 * KB, 0, false, pDevice->getPreemptionMode(), nullptr);

    auto usedIndirectHeapAfter = indirectHeap.getUsed();
    EXPECT_EQ(sizeof(INTERFACE_DESCRIPTOR_DATA), usedIndirectHeapAfter - usedIndirectHeapBefore);
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelCommandsTest, programMediaInterfaceDescriptorLoadResourceUsage) {
    CommandQueueHw<FamilyType> cmdQ(nullptr, pDevice, 0);

    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::MEDIA_STATE_FLUSH MEDIA_STATE_FLUSH;

    auto &commandStream = cmdQ.getCS(1024);
    auto usedBefore = commandStream.getUsed();

    KernelCommandsHelper<FamilyType>::sendMediaInterfaceDescriptorLoad(commandStream,
                                                                       0,
                                                                       sizeof(INTERFACE_DESCRIPTOR_DATA));

    auto usedAfter = commandStream.getUsed();
    EXPECT_EQ(sizeof(MEDIA_INTERFACE_DESCRIPTOR_LOAD) + sizeof(MEDIA_STATE_FLUSH), usedAfter - usedBefore);
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelCommandsTest, programMediaStateFlushResourceUsage) {
    CommandQueueHw<FamilyType> cmdQ(nullptr, pDevice, 0);

    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
    typedef typename FamilyType::MEDIA_STATE_FLUSH MEDIA_STATE_FLUSH;

    auto &commandStream = cmdQ.getCS(1024);
    auto usedBefore = commandStream.getUsed();

    KernelCommandsHelper<FamilyType>::sendMediaStateFlush(commandStream,
                                                          sizeof(INTERFACE_DESCRIPTOR_DATA));

    auto usedAfter = commandStream.getUsed();
    EXPECT_EQ(sizeof(MEDIA_STATE_FLUSH), usedAfter - usedBefore);
}

HWTEST_F(KernelCommandsTest, sendCrossThreadDataResourceUsage) {
    CommandQueueHw<FamilyType> cmdQ(pContext, pDevice, 0);

    std::unique_ptr<Image> srcImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, srcImage.get());
    std::unique_ptr<Image> dstImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, dstImage.get());

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pDevice->getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImageToImage3d,
                                                                                                     cmdQ.getContext(), cmdQ.getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcMemObj = srcImage.get();
    dc.dstMemObj = dstImage.get();
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);
    auto usedBefore = indirectHeap.getUsed();

    KernelCommandsHelper<FamilyType>::sendCrossThreadData(
        indirectHeap,
        *kernel);

    auto usedAfter = indirectHeap.getUsed();
    EXPECT_EQ(kernel->getCrossThreadDataSize(), usedAfter - usedBefore);
}

HWTEST_F(KernelCommandsTest, givenSendCrossThreadDataWhenWhenAddPatchInfoCommentsForAUBDumpIsNotSetThenAddPatchInfoDataOffsetsAreNotMoved) {
    CommandQueueHw<FamilyType> cmdQ(pContext, pDevice, 0);

    MockContext context;

    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false);
    auto kernelInfo = std::make_unique<KernelInfo>();

    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *kernelInfo, *pDevice));

    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 8192);

    PatchInfoData patchInfoData = {0xaaaaaaaa, 0, PatchInfoAllocationType::KernelArg, 0xbbbbbbbb, 0, PatchInfoAllocationType::IndirectObjectHeap};
    kernel->getPatchInfoDataList().push_back(patchInfoData);

    KernelCommandsHelper<FamilyType>::sendCrossThreadData(
        indirectHeap,
        *kernel);

    ASSERT_EQ(1u, kernel->getPatchInfoDataList().size());
    EXPECT_EQ(0xaaaaaaaa, kernel->getPatchInfoDataList()[0].sourceAllocation);
    EXPECT_EQ(0u, kernel->getPatchInfoDataList()[0].sourceAllocationOffset);
    EXPECT_EQ(PatchInfoAllocationType::KernelArg, kernel->getPatchInfoDataList()[0].sourceType);
    EXPECT_EQ(0xbbbbbbbb, kernel->getPatchInfoDataList()[0].targetAllocation);
    EXPECT_EQ(0u, kernel->getPatchInfoDataList()[0].targetAllocationOffset);
    EXPECT_EQ(PatchInfoAllocationType::IndirectObjectHeap, kernel->getPatchInfoDataList()[0].targetType);
}

HWTEST_F(KernelCommandsTest, givenIndirectHeapNotAllocatedFromInternalPoolWhenSendCrossThreadDataIsCalledThenOffsetZeroIsReturned) {
    auto nonInternalAllocation = pDevice->getMemoryManager()->allocateGraphicsMemory(4096);
    IndirectHeap indirectHeap(nonInternalAllocation, false);

    MockKernelWithInternals mockKernelWithInternal(*pDevice);
    auto offset = KernelCommandsHelper<FamilyType>::sendCrossThreadData(
        indirectHeap,
        *mockKernelWithInternal.mockKernel);
    EXPECT_EQ(0u, offset);
    pDevice->getMemoryManager()->freeGraphicsMemory(nonInternalAllocation);
}

HWTEST_F(KernelCommandsTest, givenIndirectHeapAllocatedFromInternalPoolWhenSendCrossThreadDataIsCalledThenHeapBaseOffsetIsReturned) {
    auto internalAllocation = pDevice->getMemoryManager()->allocate32BitGraphicsMemory(MemoryConstants::pageSize, nullptr, AllocationOrigin::INTERNAL_ALLOCATION);
    IndirectHeap indirectHeap(internalAllocation, true);
    auto expectedOffset = internalAllocation->getGpuAddressToPatch();

    MockKernelWithInternals mockKernelWithInternal(*pDevice);
    auto offset = KernelCommandsHelper<FamilyType>::sendCrossThreadData(
        indirectHeap,
        *mockKernelWithInternal.mockKernel);
    EXPECT_EQ(expectedOffset, offset);

    pDevice->getMemoryManager()->freeGraphicsMemory(internalAllocation);
}

HWTEST_F(KernelCommandsTest, givenSendCrossThreadDataWhenWhenAddPatchInfoCommentsForAUBDumpIsSetThenAddPatchInfoDataOffsetsAreMoved) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    CommandQueueHw<FamilyType> cmdQ(pContext, pDevice, 0);

    MockContext context;

    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false);
    auto kernelInfo = std::make_unique<KernelInfo>();

    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *kernelInfo, *pDevice));

    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 8192);
    indirectHeap.getSpace(128u);

    PatchInfoData patchInfoData1 = {0xaaaaaaaa, 0, PatchInfoAllocationType::KernelArg, 0xbbbbbbbb, 0, PatchInfoAllocationType::IndirectObjectHeap};
    PatchInfoData patchInfoData2 = {0xcccccccc, 0, PatchInfoAllocationType::IndirectObjectHeap, 0xdddddddd, 0, PatchInfoAllocationType::Default};

    kernel->getPatchInfoDataList().push_back(patchInfoData1);
    kernel->getPatchInfoDataList().push_back(patchInfoData2);

    auto offsetCrossThreadData = KernelCommandsHelper<FamilyType>::sendCrossThreadData(
        indirectHeap,
        *kernel);

    ASSERT_NE(0u, offsetCrossThreadData);
    EXPECT_EQ(128u, offsetCrossThreadData);

    ASSERT_EQ(2u, kernel->getPatchInfoDataList().size());
    EXPECT_EQ(0xaaaaaaaa, kernel->getPatchInfoDataList()[0].sourceAllocation);
    EXPECT_EQ(0u, kernel->getPatchInfoDataList()[0].sourceAllocationOffset);
    EXPECT_EQ(PatchInfoAllocationType::KernelArg, kernel->getPatchInfoDataList()[0].sourceType);
    EXPECT_NE(0xbbbbbbbb, kernel->getPatchInfoDataList()[0].targetAllocation);
    EXPECT_EQ(indirectHeap.getGraphicsAllocation()->getGpuAddress(), kernel->getPatchInfoDataList()[0].targetAllocation);
    EXPECT_NE(0u, kernel->getPatchInfoDataList()[0].targetAllocationOffset);
    EXPECT_EQ(offsetCrossThreadData, kernel->getPatchInfoDataList()[0].targetAllocationOffset);
    EXPECT_EQ(PatchInfoAllocationType::IndirectObjectHeap, kernel->getPatchInfoDataList()[0].targetType);
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelCommandsTest, sendIndirectStateResourceUsage) {
    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;

    CommandQueueHw<FamilyType> cmdQ(pContext, pDevice, 0);

    std::unique_ptr<Image> srcImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, srcImage.get());
    std::unique_ptr<Image> dstImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, dstImage.get());

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pDevice->getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImageToImage3d,
                                                                                                     cmdQ.getContext(), cmdQ.getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcMemObj = srcImage.get();
    dc.dstMemObj = dstImage.get();
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    const size_t localWorkSize = 256;
    const size_t localWorkSizes[3]{localWorkSize, 1, 1};

    auto &commandStream = cmdQ.getCS(1024);
    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 8192);
    auto usedBeforeCS = commandStream.getUsed();
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    dsh.align(KernelCommandsHelper<FamilyType>::alignInterfaceDescriptorData);
    size_t IDToffset = dsh.getUsed();
    dsh.getSpace(sizeof(INTERFACE_DESCRIPTOR_DATA));

    KernelCommandsHelper<FamilyType>::sendMediaInterfaceDescriptorLoad(
        commandStream,
        IDToffset,
        sizeof(INTERFACE_DESCRIPTOR_DATA));

    KernelCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        *kernel,
        kernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        IDToffset,
        0,
        pDevice->getPreemptionMode(),
        nullptr);

    // It's okay these are EXPECT_GE as they're only going to be used for
    // estimation purposes to avoid OOM.
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();
    auto sizeRequiredDSH = KernelCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel);
    auto sizeRequiredIOH = KernelCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel, localWorkSize);
    auto sizeRequiredSSH = KernelCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel);

    EXPECT_GE(sizeRequiredDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(sizeRequiredIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(sizeRequiredSSH, usedAfterSSH - usedBeforeSSH);

    auto usedAfterCS = commandStream.getUsed();
    EXPECT_GE(KernelCommandsHelper<FamilyType>::getSizeRequiredCS(), usedAfterCS - usedBeforeCS);
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelCommandsTest, givenKernelWithFourBindingTableEntriesWhenIndirectStateIsEmittedThenInterfaceDescriptorContainsCorrectBindingTableEntryCount) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    CommandQueueHw<FamilyType> cmdQ(pContext, pDevice, 0);

    auto &commandStream = cmdQ.getCS(1024);

    MockKernelWithInternals mockKernel(*pDevice, pContext);

    auto expectedBindingTableCount = 3u;
    mockKernel.mockKernel->numberOfBindingTableStates = expectedBindingTableCount;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 8192);
    const size_t localWorkSize = 256;
    const size_t localWorkSizes[3]{localWorkSize, 1, 1};

    KernelCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        *mockKernel.mockKernel,
        mockKernel.mockKernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        0,
        0,
        pDevice->getPreemptionMode(),
        nullptr);

    auto interfaceDescriptor = reinterpret_cast<INTERFACE_DESCRIPTOR_DATA *>(dsh.getCpuBase());
    if (KernelCommandsHelper<FamilyType>::doBindingTablePrefetch()) {
        EXPECT_EQ(expectedBindingTableCount, interfaceDescriptor->getBindingTableEntryCount());
    } else {
        EXPECT_EQ(0u, interfaceDescriptor->getBindingTableEntryCount());
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelCommandsTest, givenKernelThatIsSchedulerWhenIndirectStateIsEmittedThenInterfaceDescriptorContainsZeroBindingTableEntryCount) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    CommandQueueHw<FamilyType> cmdQ(pContext, pDevice, 0);

    auto &commandStream = cmdQ.getCS(1024);

    MockKernelWithInternals mockKernel(*pDevice, pContext);

    auto expectedBindingTableCount = 3u;
    mockKernel.mockKernel->numberOfBindingTableStates = expectedBindingTableCount;
    auto isScheduler = const_cast<bool *>(&mockKernel.mockKernel->isSchedulerKernel);
    *isScheduler = true;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 8192);
    const size_t localWorkSize = 256;
    const size_t localWorkSizes[3]{localWorkSize, 1, 1};

    KernelCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        *mockKernel.mockKernel,
        mockKernel.mockKernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        0,
        0,
        pDevice->getPreemptionMode(),
        nullptr);

    auto interfaceDescriptor = reinterpret_cast<INTERFACE_DESCRIPTOR_DATA *>(dsh.getCpuBase());
    EXPECT_EQ(0u, interfaceDescriptor->getBindingTableEntryCount());
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelCommandsTest, givenKernelWith100BindingTableEntriesWhenIndirectStateIsEmittedThenInterfaceDescriptorHas31BindingTableEntriesSet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    CommandQueueHw<FamilyType> cmdQ(pContext, pDevice, 0);

    auto &commandStream = cmdQ.getCS(1024);

    MockKernelWithInternals mockKernel(*pDevice, pContext);

    auto expectedBindingTableCount = 100u;
    mockKernel.mockKernel->numberOfBindingTableStates = expectedBindingTableCount;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 8192);
    const size_t localWorkSize = 256;
    const size_t localWorkSizes[3]{localWorkSize, 1, 1};

    KernelCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        *mockKernel.mockKernel,
        mockKernel.mockKernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        0,
        0,
        pDevice->getPreemptionMode(),
        nullptr);

    auto interfaceDescriptor = reinterpret_cast<INTERFACE_DESCRIPTOR_DATA *>(dsh.getCpuBase());
    if (KernelCommandsHelper<FamilyType>::doBindingTablePrefetch()) {
        EXPECT_EQ(31u, interfaceDescriptor->getBindingTableEntryCount());
    } else {
        EXPECT_EQ(0u, interfaceDescriptor->getBindingTableEntryCount());
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelCommandsTest, whenSendingIndirectStateThenKernelsWalkOrderIsTakenIntoAccount) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    CommandQueueHw<FamilyType> cmdQ(pContext, pDevice, 0);

    std::unique_ptr<Image> img(Image2dHelper<>::create(pContext));

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = cmdQ.getDevice().getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImageToImage3d,
                                                                                                             cmdQ.getContext(), cmdQ.getDevice());

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcMemObj = img.get();
    dc.dstMemObj = img.get();
    dc.size = {1, 1, 1};
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    ASSERT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    const size_t localWorkSizeX = 2;
    const size_t localWorkSizeY = 3;
    const size_t localWorkSizeZ = 4;
    const size_t localWorkSizes[3]{localWorkSizeX, localWorkSizeY, localWorkSizeZ};

    auto &commandStream = cmdQ.getCS(1024);
    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 8192);

    dsh.align(KernelCommandsHelper<FamilyType>::alignInterfaceDescriptorData);
    size_t IDToffset = dsh.getUsed();
    dsh.getSpace(sizeof(INTERFACE_DESCRIPTOR_DATA));

    KernelInfo modifiedKernelInfo = {};
    modifiedKernelInfo.patchInfo = kernel->getKernelInfo().patchInfo;
    modifiedKernelInfo.workgroupWalkOrder[0] = 2;
    modifiedKernelInfo.workgroupWalkOrder[1] = 1;
    modifiedKernelInfo.workgroupWalkOrder[2] = 0;
    modifiedKernelInfo.workgroupDimensionsOrder[0] = 2;
    modifiedKernelInfo.workgroupDimensionsOrder[1] = 1;
    modifiedKernelInfo.workgroupDimensionsOrder[2] = 0;
    MockKernel mockKernel{kernel->getProgram(), modifiedKernelInfo, kernel->getDevice(), false};
    KernelCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        mockKernel,
        modifiedKernelInfo.getMaxSimdSize(),
        localWorkSizes,
        IDToffset,
        0,
        pDevice->getPreemptionMode(),
        nullptr);
    size_t numThreads = localWorkSizeX * localWorkSizeY * localWorkSizeZ;
    numThreads = (numThreads + modifiedKernelInfo.getMaxSimdSize() - 1) / modifiedKernelInfo.getMaxSimdSize();
    size_t expectedIohSize = ((modifiedKernelInfo.getMaxSimdSize() == 32) ? 32 : 16) * 3 * numThreads * sizeof(uint16_t);
    ASSERT_LE(expectedIohSize, ioh.getUsed());
    auto expectedLocalIds = alignedMalloc(expectedIohSize, 64);
    generateLocalIDs(expectedLocalIds, modifiedKernelInfo.getMaxSimdSize(),
                     std::array<uint16_t, 3>{{localWorkSizeX, localWorkSizeY, localWorkSizeZ}},
                     std::array<uint8_t, 3>{{modifiedKernelInfo.workgroupDimensionsOrder[0], modifiedKernelInfo.workgroupDimensionsOrder[1], modifiedKernelInfo.workgroupDimensionsOrder[2]}}, false);
    EXPECT_EQ(0, memcmp(expectedLocalIds, ioh.getCpuBase(), expectedIohSize));
    alignedFree(expectedLocalIds);
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelCommandsTest, usedBindingTableStatePointer) {
    typedef typename FamilyType::BINDING_TABLE_STATE BINDING_TABLE_STATE;
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;

    CommandQueueHw<FamilyType> cmdQ(pContext, pDevice, 0);
    std::unique_ptr<Image> dstImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, dstImage.get());

    MultiDispatchInfo multiDispatchInfo;
    auto &builder = pDevice->getExecutionEnvironment()->getBuiltIns()->getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToImage3d,
                                                                                                     cmdQ.getContext(), cmdQ.getDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinDispatchInfoBuilder::BuiltinOpParams dc;
    dc.srcPtr = nullptr;
    dc.dstMemObj = dstImage.get();
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.dstRowPitch = 0;
    dc.dstSlicePitch = 0;
    builder.buildDispatchInfos(multiDispatchInfo, dc);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    const size_t localWorkSizes[3]{256, 1, 1};

    auto &commandStream = cmdQ.getCS(1024);
    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 8192);

    // Obtain where the pointers will be stored
    const auto &kernelInfo = kernel->getKernelInfo();
    auto numSurfaceStates = kernelInfo.patchInfo.statelessGlobalMemObjKernelArgs.size() +
                            kernelInfo.patchInfo.imageMemObjKernelArgs.size();
    EXPECT_EQ(2u, numSurfaceStates);
    size_t bindingTableStateSize = numSurfaceStates * sizeof(RENDER_SURFACE_STATE);
    uint32_t *bindingTableStatesPointers = reinterpret_cast<uint32_t *>(
        reinterpret_cast<uint8_t *>(ssh.getCpuBase()) + ssh.getUsed() + bindingTableStateSize);
    for (auto i = 0u; i < numSurfaceStates; i++) {
        *(&bindingTableStatesPointers[i]) = 0xDEADBEEF;
    }

    // force statefull path for buffers
    const_cast<KernelInfo &>(kernelInfo).requiresSshForBuffers = true;

    KernelCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        *kernel,
        kernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        0,
        0,
        pDevice->getPreemptionMode(),
        nullptr);

    EXPECT_EQ(0x00000000u, *(&bindingTableStatesPointers[0]));
    EXPECT_EQ(0x00000040u, *(&bindingTableStatesPointers[1]));
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelCommandsTest, usedBindingTableStatePointersForGlobalAndConstantAndPrivateAndEventPoolAndDefaultCommandQueueSurfaces) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    SPatchExecutionEnvironment tokenEE;
    tokenEE.CompiledSIMD8 = false;
    tokenEE.CompiledSIMD16 = false;
    tokenEE.CompiledSIMD32 = true;
    pKernelInfo->patchInfo.executionEnvironment = &tokenEE;

    // define patch offsets for global, constant, private, event pool and default device queue surfaces
    SPatchAllocateStatelessGlobalMemorySurfaceWithInitialization AllocateStatelessGlobalMemorySurfaceWithInitialization;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.GlobalBufferIndex = 0;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.SurfaceStateHeapOffset = 0;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.DataParamOffset = 0;
    AllocateStatelessGlobalMemorySurfaceWithInitialization.DataParamSize = 8;
    pKernelInfo->patchInfo.pAllocateStatelessGlobalMemorySurfaceWithInitialization = &AllocateStatelessGlobalMemorySurfaceWithInitialization;

    SPatchAllocateStatelessConstantMemorySurfaceWithInitialization AllocateStatelessConstantMemorySurfaceWithInitialization;
    AllocateStatelessConstantMemorySurfaceWithInitialization.ConstantBufferIndex = 0;
    AllocateStatelessConstantMemorySurfaceWithInitialization.SurfaceStateHeapOffset = 64;
    AllocateStatelessConstantMemorySurfaceWithInitialization.DataParamOffset = 8;
    AllocateStatelessConstantMemorySurfaceWithInitialization.DataParamSize = 8;
    pKernelInfo->patchInfo.pAllocateStatelessConstantMemorySurfaceWithInitialization = &AllocateStatelessConstantMemorySurfaceWithInitialization;

    SPatchAllocateStatelessPrivateSurface AllocateStatelessPrivateMemorySurface;
    AllocateStatelessPrivateMemorySurface.PerThreadPrivateMemorySize = 32;
    AllocateStatelessPrivateMemorySurface.SurfaceStateHeapOffset = 128;
    AllocateStatelessPrivateMemorySurface.DataParamOffset = 16;
    AllocateStatelessPrivateMemorySurface.DataParamSize = 8;
    pKernelInfo->patchInfo.pAllocateStatelessPrivateSurface = &AllocateStatelessPrivateMemorySurface;

    SPatchAllocateStatelessEventPoolSurface AllocateStatelessEventPoolSurface;
    AllocateStatelessEventPoolSurface.SurfaceStateHeapOffset = 192;
    AllocateStatelessEventPoolSurface.DataParamOffset = 24;
    AllocateStatelessEventPoolSurface.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessEventPoolSurface = &AllocateStatelessEventPoolSurface;

    SPatchAllocateStatelessDefaultDeviceQueueSurface AllocateStatelessDefaultDeviceQueueSurface;
    AllocateStatelessDefaultDeviceQueueSurface.SurfaceStateHeapOffset = 256;
    AllocateStatelessDefaultDeviceQueueSurface.DataParamOffset = 32;
    AllocateStatelessDefaultDeviceQueueSurface.DataParamSize = 8;

    pKernelInfo->patchInfo.pAllocateStatelessDefaultDeviceQueueSurface = &AllocateStatelessDefaultDeviceQueueSurface;

    // create program with valid context
    MockContext context;
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false);

    // setup global memory
    char globalBuffer[16];
    GraphicsAllocation gfxGlobalAlloc(globalBuffer, sizeof(globalBuffer));
    program.setGlobalSurface(&gfxGlobalAlloc);

    // setup constant memory
    char constBuffer[16];
    GraphicsAllocation gfxConstAlloc(constBuffer, sizeof(constBuffer));
    program.setConstantSurface(&gfxConstAlloc);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    SKernelBinaryHeaderCommon kernelHeader;

    // setup surface state heap
    constexpr uint32_t numSurfaces = 5;
    constexpr uint32_t sshSize = numSurfaces * sizeof(typename FamilyType::RENDER_SURFACE_STATE) + numSurfaces * sizeof(typename FamilyType::BINDING_TABLE_STATE);
    unsigned char *surfaceStateHeap = reinterpret_cast<unsigned char *>(alignedMalloc(sshSize, sizeof(typename FamilyType::RENDER_SURFACE_STATE)));

    uint32_t btiOffset = static_cast<uint32_t>(numSurfaces * sizeof(typename FamilyType::RENDER_SURFACE_STATE));
    auto bti = reinterpret_cast<typename FamilyType::BINDING_TABLE_STATE *>(surfaceStateHeap + btiOffset);
    for (uint32_t i = 0; i < numSurfaces; ++i) {
        bti[i].setSurfaceStatePointer(i * sizeof(typename FamilyType::RENDER_SURFACE_STATE));
    }

    kernelHeader.SurfaceStateHeapSize = sshSize;

    // setup kernel heap
    uint32_t kernelIsa[32];
    kernelHeader.KernelHeapSize = sizeof(kernelIsa);

    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.pKernelHeap = kernelIsa;
    pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;

    // setup binding table state
    SPatchBindingTableState bindingTableState;
    bindingTableState.Token = iOpenCL::PATCH_TOKEN_BINDING_TABLE_STATE;
    bindingTableState.Size = sizeof(SPatchBindingTableState);
    bindingTableState.Count = 5;
    bindingTableState.Offset = btiOffset;
    bindingTableState.SurfaceStateOffset = 0;
    pKernelInfo->patchInfo.bindingTableState = &bindingTableState;

    // setup thread payload
    SPatchThreadPayload threadPayload;
    threadPayload.LocalIDXPresent = 1;
    threadPayload.LocalIDYPresent = 1;
    threadPayload.LocalIDZPresent = 1;
    pKernelInfo->patchInfo.threadPayload = &threadPayload;

    // define stateful path
    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    // initialize kernel
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    // setup cross thread data
    char pCrossThreadData[64];
    pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));

    // try with different offsets to surface state base address
    for (uint32_t ssbaOffset : {0U, (uint32_t)sizeof(typename FamilyType::RENDER_SURFACE_STATE)}) {
        CommandQueueHw<FamilyType> cmdQ(nullptr, pDevice, 0);

        auto &commandStream = cmdQ.getCS(1024);
        auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);
        auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 8192);
        auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 8192);

        // Initialize binding table state pointers with pattern
        EXPECT_EQ(numSurfaces, pKernel->getNumberOfBindingTableStates());

        const size_t localWorkSizes[3]{256, 1, 1};

        dsh.getSpace(sizeof(INTERFACE_DESCRIPTOR_DATA));

        ssh.getSpace(ssbaOffset); // offset local ssh from surface state base address

        uint32_t localSshOffset = static_cast<uint32_t>(ssh.getUsed());

        // push surfaces states and binding table to given ssh heap
        KernelCommandsHelper<FamilyType>::sendIndirectState(
            commandStream,
            dsh,
            ioh,
            ssh,
            *pKernel,
            pKernel->getKernelInfo().getMaxSimdSize(),
            localWorkSizes,
            0,
            0,
            pDevice->getPreemptionMode(),
            nullptr);

        bti = reinterpret_cast<typename FamilyType::BINDING_TABLE_STATE *>(reinterpret_cast<unsigned char *>(ssh.getCpuBase()) + localSshOffset + btiOffset);
        for (uint32_t i = 0; i < numSurfaces; ++i) {
            uint32_t expected = localSshOffset + i * sizeof(typename FamilyType::RENDER_SURFACE_STATE);
            EXPECT_EQ(expected, bti[i].getSurfaceStatePointer());
        }

        program.setGlobalSurface(nullptr);
        program.setConstantSurface(nullptr);

        //exhaust space to trigger reload
        ssh.getSpace(ssh.getAvailableSpace());
        dsh.getSpace(dsh.getAvailableSpace());
    }
    alignedFree(surfaceStateHeap);
    delete pKernel;
}

HWTEST_F(KernelCommandsTest, setBindingTableStatesForKernelWithBuffersNotRequiringSSHDoesNotTouchSSH) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    // create program with valid context
    MockContext context;
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // setup surface state heap
    char surfaceStateHeap[256];
    SKernelBinaryHeaderCommon kernelHeader;
    kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;

    // define stateful path
    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = false;

    SPatchStatelessGlobalMemoryObjectKernelArgument statelessGlobalMemory;
    statelessGlobalMemory.ArgumentNumber = 0;
    statelessGlobalMemory.DataParamOffset = 0;
    statelessGlobalMemory.DataParamSize = 0;
    statelessGlobalMemory.Size = 0;
    statelessGlobalMemory.SurfaceStateHeapOffset = 0;

    pKernelInfo->patchInfo.statelessGlobalMemObjKernelArgs.push_back(&statelessGlobalMemory);

    // initialize kernel
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    CommandQueueHw<FamilyType> cmdQ(nullptr, pDevice, 0);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 8192);

    ssh.align(8);
    auto usedBefore = ssh.getUsed();

    // Initialize binding table state pointers with pattern
    auto numSurfaceStates = pKernel->getNumberOfBindingTableStates();
    EXPECT_EQ(0u, numSurfaceStates);

    // set binding table states
    auto dstBindingTablePointer = KernelCommandsHelper<FamilyType>::pushBindingTableAndSurfaceStates(ssh, *pKernel);
    EXPECT_EQ(0u, dstBindingTablePointer);

    auto usedAfter = ssh.getUsed();

    EXPECT_EQ(usedBefore, usedAfter);
    ssh.align(8);
    EXPECT_EQ(usedAfter, ssh.getUsed());

    delete pKernel;
}

HWTEST_F(KernelCommandsTest, setBindingTableStatesForNoSurfaces) {

    // define kernel info
    auto pKernelInfo = std::make_unique<KernelInfo>();

    // create program with valid context
    MockContext context;
    MockProgram program(*pDevice->getExecutionEnvironment(), &context, false);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pDevice);

    // setup surface state heap
    char surfaceStateHeap[256];
    SKernelBinaryHeaderCommon kernelHeader;
    kernelHeader.SurfaceStateHeapSize = sizeof(surfaceStateHeap);
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.pKernelHeader = &kernelHeader;

    // define stateful path
    pKernelInfo->usesSsh = true;
    pKernelInfo->requiresSshForBuffers = true;

    // initialize kernel
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    CommandQueueHw<FamilyType> cmdQ(nullptr, pDevice, 0);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 8192);

    // Initialize binding table state pointers with pattern
    auto numSurfaceStates = pKernel->getNumberOfBindingTableStates();
    EXPECT_EQ(0u, numSurfaceStates);

    auto dstBindingTablePointer = KernelCommandsHelper<FamilyType>::pushBindingTableAndSurfaceStates(ssh, *pKernelInfo);
    EXPECT_EQ(0u, dstBindingTablePointer);

    dstBindingTablePointer = KernelCommandsHelper<FamilyType>::pushBindingTableAndSurfaceStates(ssh, *pKernel);
    EXPECT_EQ(0u, dstBindingTablePointer);

    SPatchBindingTableState bindingTableState;
    bindingTableState.Token = iOpenCL::PATCH_TOKEN_BINDING_TABLE_STATE;
    bindingTableState.Size = sizeof(SPatchBindingTableState);
    bindingTableState.Count = 0;
    bindingTableState.Offset = 64;
    bindingTableState.SurfaceStateOffset = 0;
    pKernelInfo->patchInfo.bindingTableState = &bindingTableState;

    dstBindingTablePointer = KernelCommandsHelper<FamilyType>::pushBindingTableAndSurfaceStates(ssh, *pKernel);
    EXPECT_EQ(0u, dstBindingTablePointer);

    pKernelInfo->patchInfo.bindingTableState = nullptr;

    delete pKernel;
}

HWTEST_F(KernelCommandsTest, slmValueScenarios) {
    if (::renderCoreFamily == IGFX_GEN8_CORE) {
        EXPECT_EQ(0u, KernelCommandsHelper<FamilyType>::computeSlmValues(0));
        EXPECT_EQ(1u, KernelCommandsHelper<FamilyType>::computeSlmValues(1));
        EXPECT_EQ(1u, KernelCommandsHelper<FamilyType>::computeSlmValues(1024));
        EXPECT_EQ(1u, KernelCommandsHelper<FamilyType>::computeSlmValues(1025));
        EXPECT_EQ(1u, KernelCommandsHelper<FamilyType>::computeSlmValues(2048));
        EXPECT_EQ(1u, KernelCommandsHelper<FamilyType>::computeSlmValues(2049));
        EXPECT_EQ(1u, KernelCommandsHelper<FamilyType>::computeSlmValues(4096));
        EXPECT_EQ(2u, KernelCommandsHelper<FamilyType>::computeSlmValues(4097));
        EXPECT_EQ(2u, KernelCommandsHelper<FamilyType>::computeSlmValues(8192));
        EXPECT_EQ(4u, KernelCommandsHelper<FamilyType>::computeSlmValues(8193));
        EXPECT_EQ(4u, KernelCommandsHelper<FamilyType>::computeSlmValues(12288));
        EXPECT_EQ(4u, KernelCommandsHelper<FamilyType>::computeSlmValues(16384));
        EXPECT_EQ(8u, KernelCommandsHelper<FamilyType>::computeSlmValues(16385));
        EXPECT_EQ(8u, KernelCommandsHelper<FamilyType>::computeSlmValues(24576));
        EXPECT_EQ(8u, KernelCommandsHelper<FamilyType>::computeSlmValues(32768));
        EXPECT_EQ(16u, KernelCommandsHelper<FamilyType>::computeSlmValues(32769));
        EXPECT_EQ(16u, KernelCommandsHelper<FamilyType>::computeSlmValues(49152));
        EXPECT_EQ(16u, KernelCommandsHelper<FamilyType>::computeSlmValues(65535));
        EXPECT_EQ(16u, KernelCommandsHelper<FamilyType>::computeSlmValues(65536));
    } else {
        EXPECT_EQ(0u, KernelCommandsHelper<FamilyType>::computeSlmValues(0));
        EXPECT_EQ(1u, KernelCommandsHelper<FamilyType>::computeSlmValues(1));
        EXPECT_EQ(1u, KernelCommandsHelper<FamilyType>::computeSlmValues(1024));
        EXPECT_EQ(2u, KernelCommandsHelper<FamilyType>::computeSlmValues(1025));
        EXPECT_EQ(2u, KernelCommandsHelper<FamilyType>::computeSlmValues(2048));
        EXPECT_EQ(3u, KernelCommandsHelper<FamilyType>::computeSlmValues(2049));
        EXPECT_EQ(3u, KernelCommandsHelper<FamilyType>::computeSlmValues(4096));
        EXPECT_EQ(4u, KernelCommandsHelper<FamilyType>::computeSlmValues(4097));
        EXPECT_EQ(4u, KernelCommandsHelper<FamilyType>::computeSlmValues(8192));
        EXPECT_EQ(5u, KernelCommandsHelper<FamilyType>::computeSlmValues(8193));
        EXPECT_EQ(5u, KernelCommandsHelper<FamilyType>::computeSlmValues(16384));
        EXPECT_EQ(6u, KernelCommandsHelper<FamilyType>::computeSlmValues(16385));
        EXPECT_EQ(6u, KernelCommandsHelper<FamilyType>::computeSlmValues(32768));
        EXPECT_EQ(7u, KernelCommandsHelper<FamilyType>::computeSlmValues(32769));
        EXPECT_EQ(7u, KernelCommandsHelper<FamilyType>::computeSlmValues(65536));
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, KernelCommandsTest, GivenKernelWithSamplersWhenIndirectStateIsProgrammedThenBorderColorIsCorrectlyCopiedToDshAndSamplerStatesAreProgrammedWithPointer) {
    typedef typename FamilyType::BINDING_TABLE_STATE BINDING_TABLE_STATE;
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    CommandQueueHw<FamilyType> cmdQ(nullptr, pDevice, 0);
    MockKernelWithInternals kernelInternals(*pDevice);
    const size_t localWorkSizes[3]{1, 1, 1};

    auto &commandStream = cmdQ.getCS(1024);
    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::DYNAMIC_STATE, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::INDIRECT_OBJECT, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::SURFACE_STATE, 8192);

    const uint32_t borderColorSize = 64;
    const uint32_t samplerStateSize = sizeof(SAMPLER_STATE) * 2;

    SPatchSamplerStateArray samplerStateArray;
    samplerStateArray.BorderColorOffset = 0x0;
    samplerStateArray.Count = 2;
    samplerStateArray.Offset = borderColorSize;
    samplerStateArray.Size = samplerStateSize;
    samplerStateArray.Token = 1;

    char *mockDsh = new char[(borderColorSize + samplerStateSize) * 4];

    memset(mockDsh, 6, borderColorSize);
    memset(mockDsh + borderColorSize, 8, borderColorSize);

    kernelInternals.kernelInfo.heapInfo.pDsh = mockDsh;
    kernelInternals.kernelInfo.patchInfo.samplerStateArray = &samplerStateArray;

    uint64_t interfaceDescriptorTableOffset = dsh.getUsed();
    dsh.getSpace(sizeof(INTERFACE_DESCRIPTOR_DATA));
    dsh.getSpace(4);

    char *initialDshPointer = static_cast<char *>(dsh.getCpuBase()) + dsh.getUsed();
    char *borderColorPointer = alignUp(initialDshPointer, 64);
    uint32_t borderColorOffset = static_cast<uint32_t>(borderColorPointer - static_cast<char *>(dsh.getCpuBase()));

    SAMPLER_STATE *pSamplerState = reinterpret_cast<SAMPLER_STATE *>(mockDsh + borderColorSize);

    for (uint32_t i = 0; i < 2; i++) {
        pSamplerState[i].setIndirectStatePointer(0);
    }

    MockKernel *kernel = new MockKernel(kernelInternals.mockProgram, kernelInternals.kernelInfo, *pDevice);
    kernel->setCrossThreadData(kernelInternals.crossThreadData, sizeof(kernelInternals.crossThreadData));
    kernel->setSshLocal(kernelInternals.sshLocal, sizeof(kernelInternals.sshLocal));

    KernelCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        *kernel,
        8,
        localWorkSizes,
        interfaceDescriptorTableOffset,
        0,
        pDevice->getPreemptionMode(),
        nullptr);

    bool isMemorySame = memcmp(borderColorPointer, mockDsh, borderColorSize) == 0;
    EXPECT_TRUE(isMemorySame);

    SAMPLER_STATE *pSamplerStatesCopied = reinterpret_cast<SAMPLER_STATE *>(borderColorPointer + borderColorSize);

    for (uint32_t i = 0; i < 2; i++) {
        EXPECT_EQ(pSamplerState[i].getNonNormalizedCoordinateEnable(), pSamplerStatesCopied[i].getNonNormalizedCoordinateEnable());
        EXPECT_EQ(pSamplerState[i].getTcxAddressControlMode(), pSamplerStatesCopied[i].getTcxAddressControlMode());
        EXPECT_EQ(pSamplerState[i].getTcyAddressControlMode(), pSamplerStatesCopied[i].getTcyAddressControlMode());
        EXPECT_EQ(pSamplerState[i].getTczAddressControlMode(), pSamplerStatesCopied[i].getTczAddressControlMode());
        EXPECT_EQ(pSamplerState[i].getMinModeFilter(), pSamplerStatesCopied[i].getMinModeFilter());
        EXPECT_EQ(pSamplerState[i].getMagModeFilter(), pSamplerStatesCopied[i].getMagModeFilter());
        EXPECT_EQ(pSamplerState[i].getMipModeFilter(), pSamplerStatesCopied[i].getMipModeFilter());
        EXPECT_EQ(pSamplerState[i].getUAddressMinFilterRoundingEnable(), pSamplerStatesCopied[i].getUAddressMinFilterRoundingEnable());
        EXPECT_EQ(pSamplerState[i].getUAddressMagFilterRoundingEnable(), pSamplerStatesCopied[i].getUAddressMagFilterRoundingEnable());
        EXPECT_EQ(pSamplerState[i].getVAddressMinFilterRoundingEnable(), pSamplerStatesCopied[i].getVAddressMinFilterRoundingEnable());
        EXPECT_EQ(pSamplerState[i].getVAddressMagFilterRoundingEnable(), pSamplerStatesCopied[i].getVAddressMagFilterRoundingEnable());
        EXPECT_EQ(pSamplerState[i].getRAddressMagFilterRoundingEnable(), pSamplerStatesCopied[i].getRAddressMagFilterRoundingEnable());
        EXPECT_EQ(pSamplerState[i].getRAddressMinFilterRoundingEnable(), pSamplerStatesCopied[i].getRAddressMinFilterRoundingEnable());
        EXPECT_EQ(pSamplerState[i].getLodAlgorithm(), pSamplerStatesCopied[i].getLodAlgorithm());
        EXPECT_EQ(pSamplerState[i].getTextureLodBias(), pSamplerStatesCopied[i].getTextureLodBias());
        EXPECT_EQ(pSamplerState[i].getLodPreclampMode(), pSamplerStatesCopied[i].getLodPreclampMode());
        EXPECT_EQ(pSamplerState[i].getTextureBorderColorMode(), pSamplerStatesCopied[i].getTextureBorderColorMode());
        EXPECT_EQ(pSamplerState[i].getSamplerDisable(), pSamplerStatesCopied[i].getSamplerDisable());
        EXPECT_EQ(pSamplerState[i].getCubeSurfaceControlMode(), pSamplerStatesCopied[i].getCubeSurfaceControlMode());
        EXPECT_EQ(pSamplerState[i].getShadowFunction(), pSamplerStatesCopied[i].getShadowFunction());
        EXPECT_EQ(pSamplerState[i].getChromakeyMode(), pSamplerStatesCopied[i].getChromakeyMode());
        EXPECT_EQ(pSamplerState[i].getChromakeyIndex(), pSamplerStatesCopied[i].getChromakeyIndex());
        EXPECT_EQ(pSamplerState[i].getChromakeyEnable(), pSamplerStatesCopied[i].getChromakeyEnable());
        EXPECT_EQ(pSamplerState[i].getMaxLod(), pSamplerStatesCopied[i].getMaxLod());
        EXPECT_EQ(pSamplerState[i].getMinLod(), pSamplerStatesCopied[i].getMinLod());
        EXPECT_EQ(pSamplerState[i].getLodClampMagnificationMode(), pSamplerStatesCopied[i].getLodClampMagnificationMode());

        EXPECT_EQ(borderColorOffset, pSamplerStatesCopied[i].getIndirectStatePointer());
    }

    delete kernel;
    delete[] mockDsh;
}

using KernelCommandsHelperTests = ::testing::Test;

HWTEST_F(KernelCommandsHelperTests, givenCompareAddressAndDataWhenProgrammingSemaphoreWaitThenSetupAllFields) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    uint64_t compareAddress = 0x10000;
    uint32_t compareData = 1234;

    uint8_t buffer[1024] = {};
    LinearStream cmdStream(buffer, 1024);

    MI_SEMAPHORE_WAIT referenceCommand = MI_SEMAPHORE_WAIT::sInit();
    referenceCommand.setCompareOperation(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
    referenceCommand.setSemaphoreDataDword(compareData);
    referenceCommand.setSemaphoreGraphicsAddress(compareAddress);
    referenceCommand.setWaitMode(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE);

    KernelCommandsHelper<FamilyType>::programMiSemaphoreWait(cmdStream, compareAddress, compareData);
    EXPECT_EQ(sizeof(MI_SEMAPHORE_WAIT), cmdStream.getUsed());
    EXPECT_EQ(0, memcmp(&referenceCommand, buffer, sizeof(MI_SEMAPHORE_WAIT)));
}

typedef ExecutionModelKernelFixture ParentKernelCommandsFromBinaryTest;

HWTEST_P(ParentKernelCommandsFromBinaryTest, getSizeRequiredForExecutionModelForSurfaceStatesReturnsSizeOfBlocksPlusMaxBindingTableSizeForAllIDTEntriesAndSchedulerSSHSize) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;

    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {
        EXPECT_TRUE(pKernel->isParentKernel);

        size_t totalSize = 0;

        BlockKernelManager *blockManager = pKernel->getProgram()->getBlockKernelManager();
        uint32_t blockCount = static_cast<uint32_t>(blockManager->getCount());

        totalSize = BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE - 1; // for initial alignment

        uint32_t maxBindingTableCount = 0;

        for (uint32_t i = 0; i < blockCount; i++) {
            const KernelInfo *pBlockInfo = blockManager->getBlockKernelInfo(i);

            totalSize += pBlockInfo->heapInfo.pKernelHeader->SurfaceStateHeapSize;
            totalSize = alignUp(totalSize, BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);

            maxBindingTableCount = std::max(maxBindingTableCount, pBlockInfo->patchInfo.bindingTableState ? pBlockInfo->patchInfo.bindingTableState->Count : 0);
        }

        totalSize += maxBindingTableCount * sizeof(BINDING_TABLE_STATE) * DeviceQueue::interfaceDescriptorEntries;

        BuiltIns &builtIns = *pDevice->getExecutionEnvironment()->getBuiltIns();
        auto &scheduler = builtIns.getSchedulerKernel(*pContext);
        auto schedulerSshSize = scheduler.getSurfaceStateHeapSize();
        totalSize += schedulerSshSize + ((schedulerSshSize != 0) ? BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE : 0);

        totalSize = alignUp(totalSize, BINDING_TABLE_STATE::SURFACESTATEPOINTER_ALIGN_SIZE);

        EXPECT_EQ(totalSize, KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::SURFACE_STATE>(*pKernel));
    }
}

HWTEST_P(ParentKernelCommandsFromBinaryTest, getSizeRequiredForExecutionModelForIOHReturnsSchedulerSize) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;

    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {
        EXPECT_TRUE(pKernel->isParentKernel);

        BuiltIns &builtIns = *pDevice->getExecutionEnvironment()->getBuiltIns();
        auto &scheduler = builtIns.getSchedulerKernel(*pContext);
        size_t totalSize = KernelCommandsHelper<FamilyType>::getSizeRequiredIOH(scheduler);

        EXPECT_EQ(totalSize, KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::INDIRECT_OBJECT>(*pKernel));
    }
}

HWTEST_P(ParentKernelCommandsFromBinaryTest, getSizeRequiredForExecutionModelForGSH) {
    using BINDING_TABLE_STATE = typename FamilyType::BINDING_TABLE_STATE;

    if (std::string(pPlatform->getDevice(0)->getDeviceInfo().clVersion).find("OpenCL 2.") != std::string::npos) {
        EXPECT_TRUE(pKernel->isParentKernel);

        size_t totalSize = 0;

        EXPECT_EQ(totalSize, KernelCommandsHelper<FamilyType>::template getSizeRequiredForExecutionModel<IndirectHeap::GENERAL_STATE>(*pKernel));
    }
}

static const char *binaryFile = "simple_block_kernel";
static const char *KernelNames[] = {"kernel_reflection", "simple_block_kernel"};

INSTANTIATE_TEST_CASE_P(ParentKernelCommandsFromBinaryTest,
                        ParentKernelCommandsFromBinaryTest,
                        ::testing::Combine(
                            ::testing::Values(binaryFile),
                            ::testing::ValuesIn(KernelNames)));
