/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/helpers/hardware_commands_helper_tests.h"

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test_checks_shared.h"
#include "shared/test/unit_test/utilities/base_object_utils.h"

#include "opencl/source/api/api.h"
#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"

#include <iostream>
using namespace NEO;

void HardwareCommandsTest::SetUp() {
    ClDeviceFixture::SetUp();
    ASSERT_NE(nullptr, pClDevice);
    cl_device_id device = pClDevice;
    ContextFixture::SetUp(1, &device);
    ASSERT_NE(nullptr, pContext);
    BuiltInFixture::SetUp(pDevice);
    ASSERT_NE(nullptr, pBuiltIns);

    mockKernelWithInternal = std::make_unique<MockKernelWithInternals>(*pClDevice, pContext);
}

void HardwareCommandsTest::TearDown() {
    mockKernelWithInternal.reset(nullptr);
    BuiltInFixture::TearDown();
    ContextFixture::TearDown();
    ClDeviceFixture::TearDown();
}

void HardwareCommandsTest::addSpaceForSingleKernelArg() {
    kernelArguments.resize(1);
    kernelArguments[0] = kernelArgInfo;
    mockKernelWithInternal->kernelInfo.addArgBuffer(0, 0, sizeof(uintptr_t));
    mockKernelWithInternal->mockKernel->setKernelArguments(kernelArguments);
    mockKernelWithInternal->mockKernel->kernelArgRequiresCacheFlush.resize(1);
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, WhenProgramInterfaceDescriptorDataIsCreatedThenOnlyRequiredSpaceOnIndirectHeapIsAllocated) {
    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    std::unique_ptr<Image> srcImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, srcImage.get());
    std::unique_ptr<Image> dstImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, dstImage.get());

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImageToImage3d,
                                                                            cmdQ.getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcImage.get();
    dc.dstMemObj = dstImage.get();
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 8192);
    auto usedIndirectHeapBefore = indirectHeap.getUsed();
    indirectHeap.getSpace(sizeof(INTERFACE_DESCRIPTOR_DATA));

    size_t crossThreadDataSize = kernel->getCrossThreadDataSize();
    HardwareCommandsHelper<FamilyType>::sendInterfaceDescriptorData(
        indirectHeap, 0, 0, crossThreadDataSize, 64, 0, 0, 0, 1, *kernel, 0, pDevice->getPreemptionMode(), nullptr, *pDevice);

    auto usedIndirectHeapAfter = indirectHeap.getUsed();
    EXPECT_EQ(sizeof(INTERFACE_DESCRIPTOR_DATA), usedIndirectHeapAfter - usedIndirectHeapBefore);
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, WhenMediaInterfaceDescriptorIsCreatedThenOnlyRequiredSpaceInCommandStreamIsAllocated) {
    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);

    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
    typedef typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD MEDIA_INTERFACE_DESCRIPTOR_LOAD;
    typedef typename FamilyType::MEDIA_STATE_FLUSH MEDIA_STATE_FLUSH;

    auto &commandStream = cmdQ.getCS(1024);
    auto usedBefore = commandStream.getUsed();

    HardwareCommandsHelper<FamilyType>::sendMediaInterfaceDescriptorLoad(commandStream,
                                                                         0,
                                                                         sizeof(INTERFACE_DESCRIPTOR_DATA));

    auto usedAfter = commandStream.getUsed();
    EXPECT_EQ(sizeof(MEDIA_INTERFACE_DESCRIPTOR_LOAD) + sizeof(MEDIA_STATE_FLUSH), usedAfter - usedBefore);
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, WhenMediaStateFlushIsCreatedThenOnlyRequiredSpaceInCommandStreamIsAllocated) {
    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);

    typedef typename FamilyType::INTERFACE_DESCRIPTOR_DATA INTERFACE_DESCRIPTOR_DATA;
    typedef typename FamilyType::MEDIA_STATE_FLUSH MEDIA_STATE_FLUSH;

    auto &commandStream = cmdQ.getCS(1024);
    auto usedBefore = commandStream.getUsed();

    HardwareCommandsHelper<FamilyType>::sendMediaStateFlush(commandStream,
                                                            sizeof(INTERFACE_DESCRIPTOR_DATA));

    auto usedAfter = commandStream.getUsed();
    EXPECT_EQ(sizeof(MEDIA_STATE_FLUSH), usedAfter - usedBefore);
}

HWTEST_F(HardwareCommandsTest, WhenCrossThreadDataIsCreatedThenOnlyRequiredSpaceOnIndirectHeapIsAllocated) {
    REQUIRE_IMAGES_OR_SKIP(defaultHwInfo);

    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    std::unique_ptr<Image> srcImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, srcImage.get());
    std::unique_ptr<Image> dstImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, dstImage.get());

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImageToImage3d,
                                                                            cmdQ.getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcImage.get();
    dc.dstMemObj = dstImage.get();
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 8192);
    auto usedBefore = indirectHeap.getUsed();
    auto sizeCrossThreadData = kernel->getCrossThreadDataSize();
    HardwareCommandsHelper<FamilyType>::sendCrossThreadData(
        indirectHeap,
        *kernel,
        false,
        nullptr,
        sizeCrossThreadData);

    auto usedAfter = indirectHeap.getUsed();
    EXPECT_EQ(kernel->getCrossThreadDataSize(), usedAfter - usedBefore);
}

HWTEST_F(HardwareCommandsTest, givenSendCrossThreadDataWhenWhenAddPatchInfoCommentsForAUBDumpIsNotSetThenAddPatchInfoDataOffsetsAreNotMoved) {
    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    MockContext context;

    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    auto kernelInfo = std::make_unique<KernelInfo>();

    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *kernelInfo, *pClDevice));

    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 8192);

    PatchInfoData patchInfoData = {0xaaaaaaaa, 0, PatchInfoAllocationType::KernelArg, 0xbbbbbbbb, 0, PatchInfoAllocationType::IndirectObjectHeap};
    kernel->getPatchInfoDataList().push_back(patchInfoData);
    auto sizeCrossThreadData = kernel->getCrossThreadDataSize();
    HardwareCommandsHelper<FamilyType>::sendCrossThreadData(
        indirectHeap,
        *kernel,
        false,
        nullptr,
        sizeCrossThreadData);

    ASSERT_EQ(1u, kernel->getPatchInfoDataList().size());
    EXPECT_EQ(0xaaaaaaaa, kernel->getPatchInfoDataList()[0].sourceAllocation);
    EXPECT_EQ(0u, kernel->getPatchInfoDataList()[0].sourceAllocationOffset);
    EXPECT_EQ(PatchInfoAllocationType::KernelArg, kernel->getPatchInfoDataList()[0].sourceType);
    EXPECT_EQ(0xbbbbbbbb, kernel->getPatchInfoDataList()[0].targetAllocation);
    EXPECT_EQ(0u, kernel->getPatchInfoDataList()[0].targetAllocationOffset);
    EXPECT_EQ(PatchInfoAllocationType::IndirectObjectHeap, kernel->getPatchInfoDataList()[0].targetType);
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, givenIndirectHeapNotAllocatedFromInternalPoolWhenSendCrossThreadDataIsCalledThenOffsetZeroIsReturned) {
    auto nonInternalAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap indirectHeap(nonInternalAllocation, false);

    auto sizeCrossThreadData = mockKernelWithInternal->mockKernel->getCrossThreadDataSize();
    auto offset = HardwareCommandsHelper<FamilyType>::sendCrossThreadData(
        indirectHeap,
        *mockKernelWithInternal->mockKernel,
        false,
        nullptr,
        sizeCrossThreadData);
    EXPECT_EQ(0u, offset);
    pDevice->getMemoryManager()->freeGraphicsMemory(nonInternalAllocation);
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, givenIndirectHeapAllocatedFromInternalPoolWhenSendCrossThreadDataIsCalledThenHeapBaseOffsetIsReturned) {
    auto internalAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties(pDevice->getRootDeviceIndex(), true, MemoryConstants::pageSize, AllocationType::INTERNAL_HEAP, pDevice->getDeviceBitfield()));
    IndirectHeap indirectHeap(internalAllocation, true);
    auto expectedOffset = internalAllocation->getGpuAddressToPatch();

    auto sizeCrossThreadData = mockKernelWithInternal->mockKernel->getCrossThreadDataSize();
    auto offset = HardwareCommandsHelper<FamilyType>::sendCrossThreadData(
        indirectHeap,
        *mockKernelWithInternal->mockKernel,
        false,
        nullptr,
        sizeCrossThreadData);
    EXPECT_EQ(expectedOffset, offset);

    pDevice->getMemoryManager()->freeGraphicsMemory(internalAllocation);
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, givenSendCrossThreadDataWhenWhenAddPatchInfoCommentsForAUBDumpIsSetThenAddPatchInfoDataOffsetsAreMoved) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    MockContext context;

    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    auto kernelInfo = std::make_unique<KernelInfo>();

    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *kernelInfo, *pClDevice));

    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 8192);
    indirectHeap.getSpace(128u);

    PatchInfoData patchInfoData1 = {0xaaaaaaaa, 0, PatchInfoAllocationType::KernelArg, 0xbbbbbbbb, 0, PatchInfoAllocationType::IndirectObjectHeap};
    PatchInfoData patchInfoData2 = {0xcccccccc, 0, PatchInfoAllocationType::IndirectObjectHeap, 0xdddddddd, 0, PatchInfoAllocationType::Default};

    kernel->getPatchInfoDataList().push_back(patchInfoData1);
    kernel->getPatchInfoDataList().push_back(patchInfoData2);
    auto sizeCrossThreadData = kernel->getCrossThreadDataSize();
    auto offsetCrossThreadData = HardwareCommandsHelper<FamilyType>::sendCrossThreadData(
        indirectHeap,
        *kernel,
        false,
        nullptr,
        sizeCrossThreadData);

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

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, WhenAllocatingIndirectStateResourceThenCorrectSizeIsAllocated) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    std::unique_ptr<Image> srcImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, srcImage.get());
    std::unique_ptr<Image> dstImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, dstImage.get());

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImageToImage3d,
                                                                            cmdQ.getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcMemObj = srcImage.get();
    dc.dstMemObj = dstImage.get();
    dc.srcOffset = {0, 0, 0};
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    const size_t localWorkSize = 256;
    const size_t localWorkSizes[3]{localWorkSize, 1, 1};

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 8192);
    auto usedBeforeCS = commandStream.getUsed();
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    dsh.align(EncodeStates<FamilyType>::alignInterfaceDescriptorData);
    size_t IDToffset = dsh.getUsed();
    dsh.getSpace(sizeof(INTERFACE_DESCRIPTOR_DATA));

    HardwareCommandsHelper<FamilyType>::sendMediaInterfaceDescriptorLoad(
        commandStream,
        IDToffset,
        sizeof(INTERFACE_DESCRIPTOR_DATA));
    uint32_t interfaceDescriptorIndex = 0;
    auto isCcsUsed = EngineHelpers::isCcs(cmdQ.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*kernel);

    HardwareCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        *kernel,
        kernel->getKernelStartOffset(true, kernelUsesLocalIds, isCcsUsed),
        kernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        IDToffset,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        *pDevice);

    // It's okay these are EXPECT_GE as they're only going to be used for
    // estimation purposes to avoid OOM.
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();
    auto sizeRequiredDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel);
    auto sizeRequiredIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel, localWorkSize);
    auto sizeRequiredSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel);

    EXPECT_GE(sizeRequiredDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(sizeRequiredIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(sizeRequiredSSH, usedAfterSSH - usedBeforeSSH);

    auto usedAfterCS = commandStream.getUsed();
    EXPECT_GE(HardwareCommandsHelper<FamilyType>::getSizeRequiredCS(), usedAfterCS - usedBeforeCS);
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, givenKernelWithFourBindingTableEntriesWhenIndirectStateIsEmittedThenInterfaceDescriptorContainsCorrectBindingTableEntryCount) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

    auto expectedBindingTableCount = 3u;
    mockKernelWithInternal->mockKernel->numberOfBindingTableStates = expectedBindingTableCount;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 8192);
    const size_t localWorkSize = 256;
    const size_t localWorkSizes[3]{localWorkSize, 1, 1};
    uint32_t interfaceDescriptorIndex = 0;
    auto isCcsUsed = EngineHelpers::isCcs(cmdQ.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*mockKernelWithInternal->mockKernel);

    HardwareCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        *mockKernelWithInternal->mockKernel,
        mockKernelWithInternal->mockKernel->getKernelStartOffset(true, kernelUsesLocalIds, isCcsUsed),
        mockKernelWithInternal->mockKernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        0,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        *pDevice);

    auto interfaceDescriptor = reinterpret_cast<INTERFACE_DESCRIPTOR_DATA *>(dsh.getCpuBase());
    if (EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
        EXPECT_EQ(expectedBindingTableCount, interfaceDescriptor->getBindingTableEntryCount());
    } else {
        EXPECT_EQ(0u, interfaceDescriptor->getBindingTableEntryCount());
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, givenKernelWith100BindingTableEntriesWhenIndirectStateIsEmittedThenInterfaceDescriptorHas31BindingTableEntriesSet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

    auto expectedBindingTableCount = 100u;
    mockKernelWithInternal->mockKernel->numberOfBindingTableStates = expectedBindingTableCount;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 8192);
    const size_t localWorkSize = 256;
    const size_t localWorkSizes[3]{localWorkSize, 1, 1};
    uint32_t interfaceDescriptorIndex = 0;
    auto isCcsUsed = EngineHelpers::isCcs(cmdQ.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*mockKernelWithInternal->mockKernel);

    HardwareCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        *mockKernelWithInternal->mockKernel,
        mockKernelWithInternal->mockKernel->getKernelStartOffset(true, kernelUsesLocalIds, isCcsUsed),
        mockKernelWithInternal->mockKernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        0,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        *pDevice);

    auto interfaceDescriptor = reinterpret_cast<INTERFACE_DESCRIPTOR_DATA *>(dsh.getCpuBase());
    if (EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
        EXPECT_EQ(31u, interfaceDescriptor->getBindingTableEntryCount());
    } else {
        EXPECT_EQ(0u, interfaceDescriptor->getBindingTableEntryCount());
    }
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, whenSendingIndirectStateThenKernelsWalkOrderIsTakenIntoAccount) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    std::unique_ptr<Image> img(Image2dHelper<>::create(pContext));

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyImageToImage3d,
                                                                            cmdQ.getClDevice());

    BuiltinOpParams dc;
    dc.srcMemObj = img.get();
    dc.dstMemObj = img.get();
    dc.size = {1, 1, 1};

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    ASSERT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    const size_t localWorkSizeX = 2;
    const size_t localWorkSizeY = 3;
    const size_t localWorkSizeZ = 4;
    const size_t localWorkSizes[3]{localWorkSizeX, localWorkSizeY, localWorkSizeZ};

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 8192);

    dsh.align(EncodeStates<FamilyType>::alignInterfaceDescriptorData);
    size_t IDToffset = dsh.getUsed();
    dsh.getSpace(sizeof(INTERFACE_DESCRIPTOR_DATA));

    KernelInfo modifiedKernelInfo = {};
    modifiedKernelInfo.kernelDescriptor.kernelAttributes.workgroupWalkOrder[0] = 2;
    modifiedKernelInfo.kernelDescriptor.kernelAttributes.workgroupWalkOrder[1] = 1;
    modifiedKernelInfo.kernelDescriptor.kernelAttributes.workgroupWalkOrder[2] = 0;
    modifiedKernelInfo.kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[0] = 2;
    modifiedKernelInfo.kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[1] = 1;
    modifiedKernelInfo.kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[2] = 0;
    modifiedKernelInfo.kernelDescriptor.kernelAttributes.simdSize = 16;
    modifiedKernelInfo.kernelDescriptor.kernelAttributes.numLocalIdChannels = 3;
    MockKernel mockKernel(kernel->getProgram(), modifiedKernelInfo, *pClDevice);
    uint32_t interfaceDescriptorIndex = 0;
    auto isCcsUsed = EngineHelpers::isCcs(cmdQ.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(mockKernel);

    HardwareCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        mockKernel,
        mockKernel.getKernelStartOffset(true, kernelUsesLocalIds, isCcsUsed),
        modifiedKernelInfo.getMaxSimdSize(),
        localWorkSizes,
        IDToffset,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        *pDevice);

    constexpr uint32_t grfSize = sizeof(typename FamilyType::GRF);
    size_t localWorkSize = localWorkSizeX * localWorkSizeY * localWorkSizeZ;
    auto numChannels = modifiedKernelInfo.kernelDescriptor.kernelAttributes.numLocalIdChannels;
    size_t expectedIohSize = PerThreadDataHelper::getPerThreadDataSizeTotal(modifiedKernelInfo.getMaxSimdSize(), grfSize, numChannels, localWorkSize);
    ASSERT_LE(expectedIohSize, ioh.getUsed());

    auto expectedLocalIds = alignedMalloc(expectedIohSize, 64);
    generateLocalIDs(expectedLocalIds, modifiedKernelInfo.getMaxSimdSize(),
                     std::array<uint16_t, 3>{{localWorkSizeX, localWorkSizeY, localWorkSizeZ}},
                     std::array<uint8_t, 3>{{modifiedKernelInfo.kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[0],
                                             modifiedKernelInfo.kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[1],
                                             modifiedKernelInfo.kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[2]}},
                     false, grfSize);

    EXPECT_EQ(0, memcmp(expectedLocalIds, ioh.getCpuBase(), expectedIohSize));
    alignedFree(expectedLocalIds);
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, WhenSendingIndirectStateThenBindingTableStatesPointersAreCorrect) {
    typedef typename FamilyType::BINDING_TABLE_STATE BINDING_TABLE_STATE;
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);
    std::unique_ptr<Image> dstImage(Image2dHelper<>::create(pContext));
    ASSERT_NE(nullptr, dstImage.get());

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::CopyBufferToImage3d,
                                                                            cmdQ.getClDevice());
    ASSERT_NE(nullptr, &builder);

    BuiltinOpParams dc;
    dc.srcPtr = nullptr;
    dc.dstMemObj = dstImage.get();
    dc.dstOffset = {0, 0, 0};
    dc.size = {1, 1, 1};
    dc.dstRowPitch = 0;
    dc.dstSlicePitch = 0;

    MultiDispatchInfo multiDispatchInfo(dc);
    builder.buildDispatchInfos(multiDispatchInfo);
    EXPECT_NE(0u, multiDispatchInfo.size());

    auto kernel = multiDispatchInfo.begin()->getKernel();
    ASSERT_NE(nullptr, kernel);

    const size_t localWorkSizes[3]{256, 1, 1};

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 8192);

    auto sshUsed = ssh.getUsed();

    // Obtain where the pointers will be stored
    const auto &kernelInfo = kernel->getKernelInfo();
    auto numSurfaceStates = kernelInfo.kernelDescriptor.payloadMappings.bindingTable.numEntries;
    EXPECT_EQ(2u, numSurfaceStates);
    size_t bindingTableStateSize = numSurfaceStates * sizeof(RENDER_SURFACE_STATE);
    uint32_t *bindingTableStatesPointers = reinterpret_cast<uint32_t *>(
        reinterpret_cast<uint8_t *>(ssh.getCpuBase()) + ssh.getUsed() + bindingTableStateSize);
    for (auto i = 0u; i < numSurfaceStates; i++) {
        *(&bindingTableStatesPointers[i]) = 0xDEADBEEF;
    }

    uint32_t interfaceDescriptorIndex = 0;
    auto isCcsUsed = EngineHelpers::isCcs(cmdQ.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*kernel);

    HardwareCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        *kernel,
        kernel->getKernelStartOffset(true, kernelUsesLocalIds, isCcsUsed),
        kernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        0,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        *pDevice);

    EXPECT_EQ(sshUsed + 0x00000000u, *(&bindingTableStatesPointers[0]));
    EXPECT_EQ(sshUsed + 0x00000040u, *(&bindingTableStatesPointers[1]));
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, WhenGettingBindingTableStateThenSurfaceStatePointersAreCorrect) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    // define kernel info
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;

    // define patch offsets for global, constant, private, event pool and default device queue surfaces
    pKernelInfo->setGlobalVariablesSurface(8, 0, 0);
    pKernelInfo->setGlobalConstantsSurface(8, 8, 64);
    pKernelInfo->setPrivateMemory(32, false, 8, 16, 128);
    pKernelInfo->setDeviceSideEnqueueEventPoolSurface(8, 24, 192);
    pKernelInfo->setDeviceSideEnqueueDefaultQueueSurface(8, 32, 256);

    // create program with valid context
    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));

    // setup global memory
    char globalBuffer[16];
    GraphicsAllocation gfxGlobalAlloc(0, AllocationType::UNKNOWN, globalBuffer, castToUint64(globalBuffer), 0llu, sizeof(globalBuffer), MemoryPool::MemoryNull, MemoryManager::maxOsContextCount);
    program.setGlobalSurface(&gfxGlobalAlloc);

    // setup constant memory
    char constBuffer[16];
    GraphicsAllocation gfxConstAlloc(0, AllocationType::UNKNOWN, constBuffer, castToUint64(constBuffer), 0llu, sizeof(constBuffer), MemoryPool::MemoryNull, MemoryManager::maxOsContextCount);
    program.setConstantSurface(&gfxConstAlloc);

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // setup surface state heap
    constexpr uint32_t numSurfaces = 5;
    constexpr uint32_t sshSize = numSurfaces * sizeof(typename FamilyType::RENDER_SURFACE_STATE) + numSurfaces * sizeof(typename FamilyType::BINDING_TABLE_STATE);
    unsigned char *surfaceStateHeap = reinterpret_cast<unsigned char *>(alignedMalloc(sshSize, sizeof(typename FamilyType::RENDER_SURFACE_STATE)));

    uint32_t btiOffset = static_cast<uint32_t>(numSurfaces * sizeof(typename FamilyType::RENDER_SURFACE_STATE));
    auto bti = reinterpret_cast<typename FamilyType::BINDING_TABLE_STATE *>(surfaceStateHeap + btiOffset);
    for (uint32_t i = 0; i < numSurfaces; ++i) {
        bti[i].setSurfaceStatePointer(i * sizeof(typename FamilyType::RENDER_SURFACE_STATE));
    }
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.SurfaceStateHeapSize = sshSize;

    // setup kernel heap
    uint32_t kernelIsa[32];
    pKernelInfo->heapInfo.pKernelHeap = kernelIsa;
    pKernelInfo->heapInfo.KernelHeapSize = sizeof(kernelIsa);

    pKernelInfo->setBindingTable(btiOffset, 5);
    pKernelInfo->setLocalIds({1, 1, 1});

    // initialize kernel
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    // setup cross thread data
    char pCrossThreadData[64];
    pKernel->setCrossThreadData(pCrossThreadData, sizeof(pCrossThreadData));

    // try with different offsets to surface state base address
    for (uint32_t ssbaOffset : {0U, (uint32_t)sizeof(typename FamilyType::RENDER_SURFACE_STATE)}) {
        CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);

        auto &commandStream = cmdQ.getCS(1024);
        auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
        *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

        auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 8192);
        auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 8192);
        auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 8192);

        // Initialize binding table state pointers with pattern
        EXPECT_EQ(numSurfaces, pKernel->getNumberOfBindingTableStates());

        const size_t localWorkSizes[3]{256, 1, 1};

        dsh.getSpace(sizeof(INTERFACE_DESCRIPTOR_DATA));

        ssh.getSpace(ssbaOffset); // offset local ssh from surface state base address

        uint32_t localSshOffset = static_cast<uint32_t>(ssh.getUsed());

        // push surfaces states and binding table to given ssh heap
        uint32_t interfaceDescriptorIndex = 0;
        auto isCcsUsed = EngineHelpers::isCcs(cmdQ.getGpgpuEngine().osContext->getEngineType());
        auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*pKernel);

        HardwareCommandsHelper<FamilyType>::sendIndirectState(
            commandStream,
            dsh,
            ioh,
            ssh,
            *pKernel,
            pKernel->getKernelStartOffset(true, kernelUsesLocalIds, isCcsUsed),
            pKernel->getKernelInfo().getMaxSimdSize(),
            localWorkSizes,
            0,
            interfaceDescriptorIndex,
            pDevice->getPreemptionMode(),
            pWalkerCmd,
            nullptr,
            true,
            *pDevice);

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

HWTEST_F(HardwareCommandsTest, GivenBuffersNotRequiringSshWhenSettingBindingTableStatesForKernelThenSshIsNotUsed) {
    // define kernel info
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    // create program with valid context
    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // setup surface state heap
    char surfaceStateHeap[256];
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

    pKernelInfo->addArgBuffer(0, 0, 0, 0);
    pKernelInfo->setAddressQualifier(0, KernelArgMetadata::AddrGlobal);

    // initialize kernel
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 8192);

    ssh.align(8);
    auto usedBefore = ssh.getUsed();

    // Initialize binding table state pointers with pattern
    auto numSurfaceStates = pKernel->getNumberOfBindingTableStates();
    EXPECT_EQ(0u, numSurfaceStates);

    // set binding table states
    auto dstBindingTablePointer = pushBindingTableAndSurfaceStates<FamilyType>(ssh, *pKernel);
    EXPECT_EQ(0u, dstBindingTablePointer);

    auto usedAfter = ssh.getUsed();

    EXPECT_EQ(usedBefore, usedAfter);
    ssh.align(8);
    EXPECT_EQ(usedAfter, ssh.getUsed());

    delete pKernel;
}

HWTEST_F(HardwareCommandsTest, GivenZeroSurfaceStatesWhenSettingBindingTableStatesThenPointerIsZero) {
    // define kernel info
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;

    // create program with valid context
    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));

    // create kernel
    MockKernel *pKernel = new MockKernel(&program, *pKernelInfo, *pClDevice);

    // setup surface state heap
    char surfaceStateHeap[256];
    pKernelInfo->heapInfo.pSsh = surfaceStateHeap;
    pKernelInfo->heapInfo.SurfaceStateHeapSize = sizeof(surfaceStateHeap);

    // initialize kernel
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 8192);

    // Initialize binding table state pointers with pattern
    auto numSurfaceStates = pKernel->getNumberOfBindingTableStates();
    EXPECT_EQ(0u, numSurfaceStates);

    auto dstBindingTablePointer = pushBindingTableAndSurfaceStates<FamilyType>(ssh, *pKernel);
    EXPECT_EQ(0u, dstBindingTablePointer);

    dstBindingTablePointer = pushBindingTableAndSurfaceStates<FamilyType>(ssh, *pKernel);
    EXPECT_EQ(0u, dstBindingTablePointer);

    pKernelInfo->setBindingTable(64, 0);

    dstBindingTablePointer = pushBindingTableAndSurfaceStates<FamilyType>(ssh, *pKernel);
    EXPECT_EQ(0u, dstBindingTablePointer);

    delete pKernel;
}
HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, GivenKernelWithInvalidSamplerStateArrayWhenSendIndirectStateIsCalledThenInterfaceDescriptorIsNotPopulated) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 8192);
    const size_t localWorkSize = 256;
    const size_t localWorkSizes[3]{localWorkSize, 1, 1};
    uint32_t interfaceDescriptorIndex = 0;
    auto isCcsUsed = EngineHelpers::isCcs(cmdQ.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*mockKernelWithInternal->mockKernel);

    //Undefined Offset, Defined BorderColorOffset
    mockKernelWithInternal->kernelInfo.setSamplerTable(0, 2, undefined<uint16_t>);

    HardwareCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        *mockKernelWithInternal->mockKernel,
        mockKernelWithInternal->mockKernel->getKernelStartOffset(true, kernelUsesLocalIds, isCcsUsed),
        mockKernelWithInternal->mockKernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        0,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        *pDevice);

    auto interfaceDescriptor = reinterpret_cast<INTERFACE_DESCRIPTOR_DATA *>(dsh.getCpuBase());
    EXPECT_EQ(0U, interfaceDescriptor->getSamplerStatePointer());
    EXPECT_EQ(0U, interfaceDescriptor->getSamplerCount());

    //Defined Offset, Undefined BorderColorOffset
    mockKernelWithInternal->kernelInfo.setSamplerTable(undefined<uint16_t>, 2, 0);

    HardwareCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        *mockKernelWithInternal->mockKernel,
        mockKernelWithInternal->mockKernel->getKernelStartOffset(true, kernelUsesLocalIds, isCcsUsed),
        mockKernelWithInternal->mockKernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        0,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        *pDevice);

    interfaceDescriptor = reinterpret_cast<INTERFACE_DESCRIPTOR_DATA *>(dsh.getCpuBase());
    EXPECT_EQ(0U, interfaceDescriptor->getSamplerStatePointer());
    EXPECT_EQ(0U, interfaceDescriptor->getSamplerCount());
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, GivenKernelWithSamplersWhenIndirectStateIsProgrammedThenBorderColorIsCorrectlyCopiedToDshAndSamplerStatesAreProgrammedWithPointer) {
    typedef typename FamilyType::BINDING_TABLE_STATE BINDING_TABLE_STATE;
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
    const size_t localWorkSizes[3]{1, 1, 1};

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::DYNAMIC_STATE, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::SURFACE_STATE, 8192);

    const uint32_t samplerTableOffset = 64;
    const uint32_t samplerStateSize = sizeof(SAMPLER_STATE) * 2;
    mockKernelWithInternal->kernelInfo.setSamplerTable(0, 2, static_cast<DynamicStateHeapOffset>(samplerTableOffset));

    const uint32_t mockDshSize = (samplerTableOffset + samplerStateSize) * 4;
    char *mockDsh = new char[mockDshSize];
    memset(mockDsh, 6, samplerTableOffset);
    memset(mockDsh + samplerTableOffset, 8, samplerTableOffset);
    mockKernelWithInternal->kernelInfo.heapInfo.pDsh = mockDsh;
    mockKernelWithInternal->kernelInfo.heapInfo.DynamicStateHeapSize = mockDshSize;
    uint64_t interfaceDescriptorTableOffset = dsh.getUsed();
    dsh.getSpace(sizeof(INTERFACE_DESCRIPTOR_DATA));
    dsh.getSpace(4);

    char *initialDshPointer = static_cast<char *>(dsh.getCpuBase()) + dsh.getUsed();
    char *borderColorPointer = alignUp(initialDshPointer, 64);
    uint32_t borderColorOffset = static_cast<uint32_t>(borderColorPointer - static_cast<char *>(dsh.getCpuBase()));

    SAMPLER_STATE *pSamplerState = reinterpret_cast<SAMPLER_STATE *>(mockDsh + samplerTableOffset);

    for (uint32_t i = 0; i < 2; i++) {
        pSamplerState[i].setIndirectStatePointer(0);
    }

    mockKernelWithInternal->mockKernel->setCrossThreadData(mockKernelWithInternal->crossThreadData, sizeof(mockKernelWithInternal->crossThreadData));
    mockKernelWithInternal->mockKernel->setSshLocal(mockKernelWithInternal->sshLocal, sizeof(mockKernelWithInternal->sshLocal));
    uint32_t interfaceDescriptorIndex = 0;
    auto isCcsUsed = EngineHelpers::isCcs(cmdQ.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*mockKernelWithInternal->mockKernel);

    HardwareCommandsHelper<FamilyType>::sendIndirectState(
        commandStream,
        dsh,
        ioh,
        ssh,
        *mockKernelWithInternal->mockKernel,
        mockKernelWithInternal->mockKernel->getKernelStartOffset(true, kernelUsesLocalIds, isCcsUsed),
        8,
        localWorkSizes,
        interfaceDescriptorTableOffset,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        *pDevice);

    bool isMemorySame = memcmp(borderColorPointer, mockDsh, samplerTableOffset) == 0;
    EXPECT_TRUE(isMemorySame);

    SAMPLER_STATE *pSamplerStatesCopied = reinterpret_cast<SAMPLER_STATE *>(borderColorPointer + samplerTableOffset);

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

    delete[] mockDsh;
}

HWTEST_F(HardwareCommandsTest, givenEnabledPassInlineDataWhenKernelAllowsInlineThenReturnTrue) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnablePassInlineData.set(1u);

    uint32_t crossThreadData[8];
    mockKernelWithInternal->kernelInfo.kernelDescriptor.kernelAttributes.flags.passInlineData = true;
    mockKernelWithInternal->mockKernel->setCrossThreadData(crossThreadData, sizeof(crossThreadData));

    EXPECT_TRUE(HardwareCommandsHelper<FamilyType>::inlineDataProgrammingRequired(*mockKernelWithInternal->mockKernel));
}

HWTEST_F(HardwareCommandsTest, givenNoDebugSettingsWhenDefaultModeIsExcercisedThenWeFollowKernelSettingForInlineProgramming) {
    mockKernelWithInternal->kernelInfo.kernelDescriptor.kernelAttributes.flags.passInlineData = true;
    EXPECT_TRUE(HardwareCommandsHelper<FamilyType>::inlineDataProgrammingRequired(*mockKernelWithInternal->mockKernel));
}

HWTEST_F(HardwareCommandsTest, givenDisabledPassInlineDataWhenKernelAllowsInlineThenReturnFalse) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnablePassInlineData.set(0u);
    mockKernelWithInternal->kernelInfo.kernelDescriptor.kernelAttributes.flags.passInlineData = true;
    EXPECT_FALSE(HardwareCommandsHelper<FamilyType>::inlineDataProgrammingRequired(*mockKernelWithInternal->mockKernel));
}

HWTEST_F(HardwareCommandsTest, givenEnabledPassInlineDataWhenKernelDisallowsInlineThenReturnFalse) {
    DebugManagerStateRestore restore;
    DebugManager.flags.EnablePassInlineData.set(1u);

    uint32_t crossThreadData[8];

    mockKernelWithInternal->kernelInfo.kernelDescriptor.kernelAttributes.flags.passInlineData = false;
    mockKernelWithInternal->mockKernel->setCrossThreadData(crossThreadData, sizeof(crossThreadData));

    EXPECT_FALSE(HardwareCommandsHelper<FamilyType>::inlineDataProgrammingRequired(*mockKernelWithInternal->mockKernel));
}

HWTEST_F(HardwareCommandsTest, whenNumLocalIdsIsBiggerThanZeroThenExpectLocalIdsInUseIsTrue) {
    mockKernelWithInternal->kernelInfo.kernelDescriptor.kernelAttributes.numLocalIdChannels = 1;
    EXPECT_TRUE(HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*mockKernelWithInternal->mockKernel));
}

HWTEST_F(HardwareCommandsTest, whenNumLocalIdsIsZeroThenExpectLocalIdsInUseIsFalse) {
    mockKernelWithInternal->kernelInfo.kernelDescriptor.kernelAttributes.numLocalIdChannels = 0;
    EXPECT_FALSE(HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*mockKernelWithInternal->mockKernel));
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, givenCacheFlushAfterWalkerEnabledWhenProgramGlobalSurfacePresentThenExpectCacheFlushCommand) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MEDIA_STATE_FLUSH = typename FamilyType::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
    auto &commandStream = cmdQ.getCS(1024);

    MockGraphicsAllocation globalAllocation;
    mockKernelWithInternal->mockProgram->setGlobalSurface(&globalAllocation);

    Kernel::CacheFlushAllocationsVec allocs;
    mockKernelWithInternal->mockKernel->getAllocationsForCacheFlush(allocs);
    EXPECT_NE(allocs.end(), std::find(allocs.begin(), allocs.end(), &globalAllocation));

    size_t expectedSize = sizeof(PIPE_CONTROL);
    size_t actualSize = HardwareCommandsHelper<FamilyType>::getSizeRequiredForCacheFlush(cmdQ, mockKernelWithInternal->mockKernel, 0U);
    EXPECT_EQ(expectedSize, actualSize);

    HardwareCommandsHelper<FamilyType>::programCacheFlushAfterWalkerCommand(&commandStream, cmdQ, mockKernelWithInternal->mockKernel, 0U);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(commandStream);
    PIPE_CONTROL *pipeControl = hwParse.getCommand<PIPE_CONTROL>();
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControl->getDcFlushEnable());

    mockKernelWithInternal->mockProgram->setGlobalSurface(nullptr);
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, givenCacheFlushAfterWalkerEnabledWhenSvmAllocationsSetAsCacheFlushRequiringThenExpectCacheFlushCommand) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MEDIA_STATE_FLUSH = typename FamilyType::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
    auto &commandStream = cmdQ.getCS(1024);

    char buff[MemoryConstants::pageSize * 2];
    MockGraphicsAllocation svmAllocation1{alignUp(buff, MemoryConstants::pageSize), MemoryConstants::pageSize};
    mockKernelWithInternal->mockKernel->kernelSvmGfxAllocations.push_back(&svmAllocation1);
    MockGraphicsAllocation svmAllocation2{alignUp(buff, MemoryConstants::pageSize), MemoryConstants::pageSize};
    svmAllocation2.setFlushL3Required(false);
    mockKernelWithInternal->mockKernel->kernelSvmGfxAllocations.push_back(&svmAllocation2);
    mockKernelWithInternal->mockKernel->svmAllocationsRequireCacheFlush = true;

    Kernel::CacheFlushAllocationsVec allocs;
    mockKernelWithInternal->mockKernel->getAllocationsForCacheFlush(allocs);
    EXPECT_NE(allocs.end(), std::find(allocs.begin(), allocs.end(), &svmAllocation1));
    EXPECT_EQ(allocs.end(), std::find(allocs.begin(), allocs.end(), &svmAllocation2));

    size_t expectedSize = sizeof(PIPE_CONTROL);
    size_t actualSize = HardwareCommandsHelper<FamilyType>::getSizeRequiredForCacheFlush(cmdQ, mockKernelWithInternal->mockKernel, 0U);
    EXPECT_EQ(expectedSize, actualSize);

    HardwareCommandsHelper<FamilyType>::programCacheFlushAfterWalkerCommand(&commandStream, cmdQ, mockKernelWithInternal->mockKernel, 0U);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(commandStream);
    PIPE_CONTROL *pipeControl = hwParse.getCommand<PIPE_CONTROL>();
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControl->getDcFlushEnable());
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, givenCacheFlushAfterWalkerEnabledWhenKernelArgIsSetAsCacheFlushRequiredThenExpectCacheFlushCommand) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using MEDIA_STATE_FLUSH = typename FamilyType::MEDIA_STATE_FLUSH;
    using MEDIA_INTERFACE_DESCRIPTOR_LOAD = typename FamilyType::MEDIA_INTERFACE_DESCRIPTOR_LOAD;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(1);

    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
    auto &commandStream = cmdQ.getCS(1024);

    addSpaceForSingleKernelArg();
    MockGraphicsAllocation cacheRequiringAllocation;
    mockKernelWithInternal->mockKernel->kernelArgRequiresCacheFlush.resize(2);
    mockKernelWithInternal->mockKernel->kernelArgRequiresCacheFlush[0] = &cacheRequiringAllocation;

    Kernel::CacheFlushAllocationsVec allocs;
    mockKernelWithInternal->mockKernel->getAllocationsForCacheFlush(allocs);
    EXPECT_NE(allocs.end(), std::find(allocs.begin(), allocs.end(), &cacheRequiringAllocation));

    size_t expectedSize = sizeof(PIPE_CONTROL);
    size_t actualSize = HardwareCommandsHelper<FamilyType>::getSizeRequiredForCacheFlush(cmdQ, mockKernelWithInternal->mockKernel, 0U);
    EXPECT_EQ(expectedSize, actualSize);

    HardwareCommandsHelper<FamilyType>::programCacheFlushAfterWalkerCommand(&commandStream, cmdQ, mockKernelWithInternal->mockKernel, 0U);

    HardwareParse hwParse;
    hwParse.parseCommands<FamilyType>(commandStream);
    PIPE_CONTROL *pipeControl = hwParse.getCommand<PIPE_CONTROL>();
    ASSERT_NE(nullptr, pipeControl);
    EXPECT_TRUE(pipeControl->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControl->getDcFlushEnable());
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsTest, givenCacheFlushAfterWalkerDisabledWhenGettingRequiredCacheFlushSizeThenReturnZero) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(0);

    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);

    size_t expectedSize = 0U;
    size_t actualSize = HardwareCommandsHelper<FamilyType>::getSizeRequiredForCacheFlush(cmdQ, mockKernelWithInternal->mockKernel, 0U);
    EXPECT_EQ(expectedSize, actualSize);
}

TEST_F(HardwareCommandsTest, givenCacheFlushAfterWalkerEnabledWhenPlatformNotSupportFlushThenExpectNoCacheAllocationForFlush) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(-1);
    hardwareInfo.capabilityTable.supportCacheFlushAfterWalker = false;

    StackVec<GraphicsAllocation *, 32> allocationsForCacheFlush;
    mockKernelWithInternal->mockKernel->getAllocationsForCacheFlush(allocationsForCacheFlush);
    EXPECT_EQ(0U, allocationsForCacheFlush.size());
}

using KernelCacheFlushTests = Test<HelloWorldFixture<HelloWorldFixtureFactory>>;

HWTEST_F(KernelCacheFlushTests, givenLocallyUncachedBufferWhenGettingAllocationsForFlushThenEmptyVectorIsReturned) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.EnableCacheFlushAfterWalker.set(-1);
    DebugManager.flags.CreateMultipleSubDevices.set(2);
    auto context = std::make_unique<MockContext>();
    auto kernel = std::unique_ptr<Kernel>(Kernel::create(pProgram, pProgram->getKernelInfoForKernel("CopyBuffer"), *context->getDevice(0), &retVal));

    cl_mem_properties_intel bufferPropertiesUncachedResource[] = {CL_MEM_FLAGS_INTEL, CL_MEM_LOCALLY_UNCACHED_RESOURCE, 0};
    auto bufferLocallyUncached = clCreateBufferWithPropertiesINTEL(context.get(), bufferPropertiesUncachedResource, 0, 1, nullptr, nullptr);
    kernel->setArg(0, sizeof(bufferLocallyUncached), &bufferLocallyUncached);

    using CacheFlushAllocationsVec = StackVec<GraphicsAllocation *, 32>;
    CacheFlushAllocationsVec cacheFlushVec;
    kernel->getAllocationsForCacheFlush(cacheFlushVec);
    EXPECT_EQ(0u, cacheFlushVec.size());

    auto bufferRegular = clCreateBufferWithPropertiesINTEL(context.get(), nullptr, 0, 1, nullptr, nullptr);
    kernel->setArg(1, sizeof(bufferRegular), &bufferRegular);

    kernel->getAllocationsForCacheFlush(cacheFlushVec);
    size_t expectedCacheFlushVecSize = (hardwareInfo.capabilityTable.supportCacheFlushAfterWalker ? 1u : 0u);
    EXPECT_EQ(expectedCacheFlushVecSize, cacheFlushVec.size());

    clReleaseMemObject(bufferLocallyUncached);
    clReleaseMemObject(bufferRegular);
}

struct HardwareCommandsImplicitArgsTests : Test<ClDeviceFixture> {

    void SetUp() override {
        ClDeviceFixture::SetUp();
        indirectHeapAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

        expectedImplicitArgs.numWorkDim = 3;
        expectedImplicitArgs.simdWidth = 32;
        expectedImplicitArgs.localSizeX = 2;
        expectedImplicitArgs.localSizeY = 3;
        expectedImplicitArgs.localSizeZ = 4;
        expectedImplicitArgs.globalOffsetX = 1;
        expectedImplicitArgs.globalOffsetY = 2;
        expectedImplicitArgs.globalOffsetZ = 3;
        expectedImplicitArgs.groupCountX = 2;
        expectedImplicitArgs.groupCountY = 1;
        expectedImplicitArgs.groupCountZ = 3;
    }

    void TearDown() override {
        pDevice->getMemoryManager()->freeGraphicsMemory(indirectHeapAllocation);
        ClDeviceFixture::TearDown();
    }

    template <typename FamilyType>
    void dispatchKernelWithImplicitArgs() {
        expectedImplicitArgs.globalSizeX = expectedImplicitArgs.localSizeX * expectedImplicitArgs.groupCountX;
        expectedImplicitArgs.globalSizeY = expectedImplicitArgs.localSizeY * expectedImplicitArgs.groupCountY;
        expectedImplicitArgs.globalSizeZ = expectedImplicitArgs.localSizeZ * expectedImplicitArgs.groupCountZ;

        IndirectHeap indirectHeap(indirectHeapAllocation, false);

        auto pKernelInfo = std::make_unique<MockKernelInfo>();
        pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = expectedImplicitArgs.simdWidth;
        UnitTestHelper<FamilyType>::adjustKernelDescriptorForImplicitArgs(pKernelInfo->kernelDescriptor);
        pKernelInfo->kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[0] = workgroupDimOrder[0];
        pKernelInfo->kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[1] = workgroupDimOrder[1];
        pKernelInfo->kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[2] = workgroupDimOrder[2];

        MockContext context(pClDevice);
        MockProgram program(&context, false, toClDeviceVector(*pClDevice));

        MockKernel kernel(&program, *pKernelInfo, *pClDevice);
        ASSERT_EQ(CL_SUCCESS, kernel.initialize());
        auto pImplicitArgs = kernel.getImplicitArgs();

        ASSERT_NE(nullptr, pImplicitArgs);

        kernel.setCrossThreadData(nullptr, sizeof(uint64_t));

        kernel.setWorkDim(expectedImplicitArgs.numWorkDim);
        kernel.setLocalWorkSizeValues(expectedImplicitArgs.localSizeX, expectedImplicitArgs.localSizeY, expectedImplicitArgs.localSizeZ);
        kernel.setGlobalWorkSizeValues(static_cast<uint32_t>(expectedImplicitArgs.globalSizeX), static_cast<uint32_t>(expectedImplicitArgs.globalSizeY), static_cast<uint32_t>(expectedImplicitArgs.globalSizeZ));
        kernel.setGlobalWorkOffsetValues(static_cast<uint32_t>(expectedImplicitArgs.globalOffsetX), static_cast<uint32_t>(expectedImplicitArgs.globalOffsetY), static_cast<uint32_t>(expectedImplicitArgs.globalOffsetZ));
        kernel.setNumWorkGroupsValues(expectedImplicitArgs.groupCountX, expectedImplicitArgs.groupCountY, expectedImplicitArgs.groupCountZ);

        implicitArgsProgrammingSize = ImplicitArgsHelper::getSizeForImplicitArgsPatching(pImplicitArgs, kernel.getDescriptor(), pDevice->getHardwareInfo());

        auto sizeCrossThreadData = kernel.getCrossThreadDataSize();
        HardwareCommandsHelper<FamilyType>::sendCrossThreadData(
            indirectHeap,
            kernel,
            false,
            nullptr,
            sizeCrossThreadData);

        EXPECT_LE(implicitArgsProgrammingSize, indirectHeap.getUsed());

        if (FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
            expectedImplicitArgs.localIdTablePtr = indirectHeapAllocation->getGpuAddress();
        }
    }

    ImplicitArgs expectedImplicitArgs = {sizeof(ImplicitArgs)};
    GraphicsAllocation *indirectHeapAllocation = nullptr;
    std::array<uint8_t, 3> workgroupDimOrder{0, 1, 2};
    uint32_t implicitArgsProgrammingSize = 0u;
};

HWCMDTEST_F(IGFX_XE_HP_CORE, HardwareCommandsImplicitArgsTests, givenXeHpAndLaterPlatformWhenSendingIndirectStateForKernelWithImplicitArgsThenImplicitArgsAreSentToIndirectHeapWithLocalIds) {
    dispatchKernelWithImplicitArgs<FamilyType>();

    auto localIdsProgrammingSize = implicitArgsProgrammingSize - sizeof(ImplicitArgs);
    auto implicitArgsInIndirectData = ptrOffset(indirectHeapAllocation->getUnderlyingBuffer(), localIdsProgrammingSize);
    EXPECT_EQ(0, memcmp(implicitArgsInIndirectData, &expectedImplicitArgs, sizeof(ImplicitArgs)));
}

HWCMDTEST_F(IGFX_GEN8_CORE, HardwareCommandsImplicitArgsTests, givenPreXeHpPlatformWhenSendingIndirectStateForKernelWithImplicitArgsThenImplicitArgsAreSentToIndirectHeapWithoutLocalIds) {
    dispatchKernelWithImplicitArgs<FamilyType>();

    auto implicitArgsInIndirectData = indirectHeapAllocation->getUnderlyingBuffer();
    EXPECT_EQ(0, memcmp(implicitArgsInIndirectData, &expectedImplicitArgs, sizeof(ImplicitArgs)));

    auto crossThreadDataInIndirectData = ptrOffset(indirectHeapAllocation->getUnderlyingBuffer(), 0x80);

    auto programmedImplicitArgsGpuVA = reinterpret_cast<uint64_t *>(crossThreadDataInIndirectData)[0];
    EXPECT_EQ(indirectHeapAllocation->getGpuAddress(), programmedImplicitArgsGpuVA);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HardwareCommandsImplicitArgsTests, givenKernelWithImplicitArgsAndRuntimeLocalIdsGenerationWhenSendingIndirectStateThenLocalIdsAreGeneratedAndCorrectlyProgrammedInCrossThreadData) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableHwGenerationLocalIds.set(0);

    workgroupDimOrder[0] = 2;
    workgroupDimOrder[1] = 1;
    workgroupDimOrder[2] = 0;

    std::array<uint16_t, 3> localSize{2, 3, 4};
    size_t totalLocalSize = localSize[0] * localSize[1] * localSize[2];

    expectedImplicitArgs.localSizeX = localSize[0];
    expectedImplicitArgs.localSizeY = localSize[1];
    expectedImplicitArgs.localSizeZ = localSize[2];

    dispatchKernelWithImplicitArgs<FamilyType>();

    auto grfSize = ImplicitArgsHelper::getGrfSize(expectedImplicitArgs.simdWidth, sizeof(typename FamilyType::GRF));
    auto expectedLocalIds = alignedMalloc(implicitArgsProgrammingSize - sizeof(ImplicitArgs), MemoryConstants::cacheLineSize);
    generateLocalIDs(expectedLocalIds, expectedImplicitArgs.simdWidth, localSize, workgroupDimOrder, false, grfSize);

    auto localIdsProgrammingSize = implicitArgsProgrammingSize - sizeof(ImplicitArgs);
    size_t sizeForLocalIds = PerThreadDataHelper::getPerThreadDataSizeTotal(expectedImplicitArgs.simdWidth, grfSize, 3u, totalLocalSize);

    EXPECT_EQ(0, memcmp(expectedLocalIds, indirectHeapAllocation->getUnderlyingBuffer(), sizeForLocalIds));
    alignedFree(expectedLocalIds);

    auto implicitArgsInIndirectData = ptrOffset(indirectHeapAllocation->getUnderlyingBuffer(), localIdsProgrammingSize);
    EXPECT_EQ(0, memcmp(implicitArgsInIndirectData, &expectedImplicitArgs, sizeof(ImplicitArgs)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HardwareCommandsImplicitArgsTests, givenKernelWithImplicitArgsAndHwLocalIdsGenerationWhenSendingIndirectStateThenLocalIdsAreGeneratedAndCorrectlyProgrammedInCrossThreadData) {
    DebugManagerStateRestore restorer;
    DebugManager.flags.EnableHwGenerationLocalIds.set(1);

    workgroupDimOrder[0] = 2;
    workgroupDimOrder[1] = 1;
    workgroupDimOrder[2] = 0;

    std::array<uint8_t, 3> expectedDimOrder = {0, 2, 1};

    std::array<uint16_t, 3> localSize{2, 3, 4};
    size_t totalLocalSize = localSize[0] * localSize[1] * localSize[2];

    expectedImplicitArgs.localSizeX = localSize[0];
    expectedImplicitArgs.localSizeY = localSize[1];
    expectedImplicitArgs.localSizeZ = localSize[2];

    dispatchKernelWithImplicitArgs<FamilyType>();

    auto grfSize = ImplicitArgsHelper::getGrfSize(expectedImplicitArgs.simdWidth, sizeof(typename FamilyType::GRF));
    auto expectedLocalIds = alignedMalloc(implicitArgsProgrammingSize - sizeof(ImplicitArgs), MemoryConstants::cacheLineSize);
    generateLocalIDs(expectedLocalIds, expectedImplicitArgs.simdWidth, localSize, expectedDimOrder, false, grfSize);

    auto localIdsProgrammingSize = implicitArgsProgrammingSize - sizeof(ImplicitArgs);
    size_t sizeForLocalIds = PerThreadDataHelper::getPerThreadDataSizeTotal(expectedImplicitArgs.simdWidth, grfSize, 3u, totalLocalSize);

    EXPECT_EQ(0, memcmp(expectedLocalIds, indirectHeapAllocation->getUnderlyingBuffer(), sizeForLocalIds));
    alignedFree(expectedLocalIds);

    auto implicitArgsInIndirectData = ptrOffset(indirectHeapAllocation->getUnderlyingBuffer(), localIdsProgrammingSize);
    EXPECT_EQ(0, memcmp(implicitArgsInIndirectData, &expectedImplicitArgs, sizeof(ImplicitArgs)));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HardwareCommandsImplicitArgsTests, givenKernelWithImplicitArgsWhenSendingIndirectStateWithSimd1ThenLocalIdsAreGeneratedCorrectly) {
    workgroupDimOrder[0] = 2;
    workgroupDimOrder[1] = 1;
    workgroupDimOrder[2] = 0;

    expectedImplicitArgs.simdWidth = 1;
    expectedImplicitArgs.localSizeX = 2;
    expectedImplicitArgs.localSizeY = 2;
    expectedImplicitArgs.localSizeZ = 1;

    dispatchKernelWithImplicitArgs<FamilyType>();

    uint16_t expectedLocalIds[][3] = {{0, 0, 0},
                                      {0, 1, 0},
                                      {0, 0, 1},
                                      {0, 1, 1}};

    EXPECT_EQ(0, memcmp(expectedLocalIds, indirectHeapAllocation->getUnderlyingBuffer(), sizeof(expectedLocalIds)));

    auto localIdsProgrammingSize = implicitArgsProgrammingSize - sizeof(ImplicitArgs);

    EXPECT_EQ(alignUp(sizeof(expectedLocalIds), MemoryConstants::cacheLineSize), localIdsProgrammingSize);

    auto implicitArgsInIndirectData = ptrOffset(indirectHeapAllocation->getUnderlyingBuffer(), localIdsProgrammingSize);
    EXPECT_EQ(0, memcmp(implicitArgsInIndirectData, &expectedImplicitArgs, sizeof(ImplicitArgs)));
}

using HardwareCommandsTestXeHpAndLater = HardwareCommandsTest;

HWCMDTEST_F(IGFX_XE_HP_CORE, HardwareCommandsTestXeHpAndLater, givenIndirectHeapNotAllocatedFromInternalPoolWhenSendCrossThreadDataIsCalledThenOffsetZeroIsReturned) {
    auto nonInternalAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap indirectHeap(nonInternalAllocation, false);

    auto expectedOffset = is64bit ? 0u : indirectHeap.getHeapGpuBase();

    auto sizeCrossThreadData = mockKernelWithInternal->mockKernel->getCrossThreadDataSize();
    auto offset = HardwareCommandsHelper<FamilyType>::sendCrossThreadData(
        indirectHeap,
        *mockKernelWithInternal->mockKernel,
        false,
        nullptr,
        sizeCrossThreadData);
    EXPECT_EQ(expectedOffset, offset);
    pDevice->getMemoryManager()->freeGraphicsMemory(nonInternalAllocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HardwareCommandsTestXeHpAndLater, givenIndirectHeapAllocatedFromInternalPoolWhenSendCrossThreadDataIsCalledThenHeapBaseOffsetIsReturned) {
    auto internalAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties(pDevice->getRootDeviceIndex(), true, MemoryConstants::pageSize, AllocationType::INTERNAL_HEAP, pDevice->getDeviceBitfield()));
    IndirectHeap indirectHeap(internalAllocation, true);
    auto expectedOffset = is64bit ? internalAllocation->getGpuAddressToPatch() : 0u;

    auto sizeCrossThreadData = mockKernelWithInternal->mockKernel->getCrossThreadDataSize();
    auto offset = HardwareCommandsHelper<FamilyType>::sendCrossThreadData(
        indirectHeap,
        *mockKernelWithInternal->mockKernel,
        false,
        nullptr,
        sizeCrossThreadData);
    EXPECT_EQ(expectedOffset, offset);

    pDevice->getMemoryManager()->freeGraphicsMemory(internalAllocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HardwareCommandsTestXeHpAndLater, givenSendCrossThreadDataWhenWhenAddPatchInfoCommentsForAUBDumpIsSetThenAddPatchInfoDataOffsetsAreMoved) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    MockContext context;

    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    auto kernelInfo = std::make_unique<KernelInfo>();

    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *kernelInfo, *pClDevice));

    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::Type::INDIRECT_OBJECT, 8192);
    indirectHeap.getSpace(128u);

    PatchInfoData patchInfoData1 = {0xaaaaaaaa, 0, PatchInfoAllocationType::KernelArg, 0xbbbbbbbb, 0, PatchInfoAllocationType::IndirectObjectHeap};
    PatchInfoData patchInfoData2 = {0xcccccccc, 0, PatchInfoAllocationType::IndirectObjectHeap, 0xdddddddd, 0, PatchInfoAllocationType::Default};

    kernel->getPatchInfoDataList().push_back(patchInfoData1);
    kernel->getPatchInfoDataList().push_back(patchInfoData2);
    auto sizeCrossThreadData = kernel->getCrossThreadDataSize();
    auto offsetCrossThreadData = HardwareCommandsHelper<FamilyType>::sendCrossThreadData(
        indirectHeap,
        *kernel,
        false,
        nullptr,
        sizeCrossThreadData);

    auto expectedOffsetRelativeToIohBase = 128u;
    auto iohBaseAddress = is64bit ? 0u : indirectHeap.getHeapGpuBase();

    ASSERT_NE(0u, offsetCrossThreadData);
    EXPECT_EQ(iohBaseAddress + expectedOffsetRelativeToIohBase, offsetCrossThreadData);

    ASSERT_EQ(2u, kernel->getPatchInfoDataList().size());
    EXPECT_EQ(0xaaaaaaaa, kernel->getPatchInfoDataList()[0].sourceAllocation);
    EXPECT_EQ(0u, kernel->getPatchInfoDataList()[0].sourceAllocationOffset);
    EXPECT_EQ(PatchInfoAllocationType::KernelArg, kernel->getPatchInfoDataList()[0].sourceType);
    EXPECT_NE(0xbbbbbbbb, kernel->getPatchInfoDataList()[0].targetAllocation);
    EXPECT_EQ(indirectHeap.getGraphicsAllocation()->getGpuAddress(), kernel->getPatchInfoDataList()[0].targetAllocation);
    EXPECT_NE(0u, kernel->getPatchInfoDataList()[0].targetAllocationOffset);
    EXPECT_EQ(expectedOffsetRelativeToIohBase, kernel->getPatchInfoDataList()[0].targetAllocationOffset);
    EXPECT_EQ(PatchInfoAllocationType::IndirectObjectHeap, kernel->getPatchInfoDataList()[0].targetType);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HardwareCommandsTestXeHpAndLater, whenGetSizeRequiredForCacheFlushIsCalledThenExceptionIsThrown) {
    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    EXPECT_ANY_THROW(HardwareCommandsHelper<FamilyType>::getSizeRequiredForCacheFlush(cmdQ, nullptr, 0));
}
