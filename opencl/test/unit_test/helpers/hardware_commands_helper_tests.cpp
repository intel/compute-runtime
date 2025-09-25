/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/helpers/hardware_commands_helper_tests.h"

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/helpers/address_patch.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/kernel/implicit_args_helper.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/api/api.h"
#include "opencl/source/built_ins/builtins_dispatch_builder.h"
#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"

#include <iostream>
using namespace NEO;
#include "shared/test/common/test_macros/header/heapless_matchers.h"

void HardwareCommandsTest::SetUp() {
    ClDeviceFixture::setUp();
    ASSERT_NE(nullptr, pClDevice);
    cl_device_id device = pClDevice;
    ContextFixture::setUp(1, &device);
    ASSERT_NE(nullptr, pContext);
    BuiltInFixture::setUp(pDevice);
    ASSERT_NE(nullptr, pBuiltIns);

    mockKernelWithInternal = std::make_unique<MockKernelWithInternals>(*pClDevice, pContext);
}

void HardwareCommandsTest::TearDown() {
    mockKernelWithInternal.reset(nullptr);
    BuiltInFixture::tearDown();
    ContextFixture::tearDown();
    ClDeviceFixture::tearDown();
}

void HardwareCommandsTest::addSpaceForSingleKernelArg() {
    kernelArguments.resize(1);
    kernelArguments[0] = kernelArgInfo;
    mockKernelWithInternal->kernelInfo.addArgBuffer(0, 0, sizeof(uintptr_t));
    mockKernelWithInternal->mockKernel->setKernelArguments(kernelArguments);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsTest, WhenProgramInterfaceDescriptorDataIsCreatedThenOnlyRequiredSpaceOnIndirectHeapIsAllocated) {
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    GPGPU_WALKER walkerCmd{};
    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    std::unique_ptr<Image> srcImage(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, srcImage.get());
    std::unique_ptr<Image> dstImage(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, dstImage.get());

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyImageToImage3d,
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

    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::Type::dynamicState, 8192);
    auto usedIndirectHeapBefore = indirectHeap.getUsed();
    indirectHeap.getSpace(sizeof(INTERFACE_DESCRIPTOR_DATA));

    const uint32_t threadGroupCount = 1u;
    size_t crossThreadDataSize = kernel->getCrossThreadDataSize();
    HardwareCommandsHelper<FamilyType>::template sendInterfaceDescriptorData<GPGPU_WALKER, INTERFACE_DESCRIPTOR_DATA>(
        indirectHeap, 0, 0, crossThreadDataSize, 64, 0, 0, 0, threadGroupCount, 1, *kernel, 0, pDevice->getPreemptionMode(), *pDevice, &walkerCmd, nullptr);

    auto usedIndirectHeapAfter = indirectHeap.getUsed();
    EXPECT_EQ(sizeof(INTERFACE_DESCRIPTOR_DATA), usedIndirectHeapAfter - usedIndirectHeapBefore);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsTest, WhenMediaInterfaceDescriptorIsCreatedThenOnlyRequiredSpaceInCommandStreamIsAllocated) {
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

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsTest, WhenMediaStateFlushIsCreatedThenOnlyRequiredSpaceInCommandStreamIsAllocated) {
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
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    std::unique_ptr<Image> srcImage(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, srcImage.get());
    std::unique_ptr<Image> dstImage(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, dstImage.get());

    auto &compilerProductHelper = pClDevice->getCompilerProductHelper();
    bool heaplessAllowed = compilerProductHelper.isHeaplessModeEnabled(pClDevice->getHardwareInfo());
    auto builtin = EBuiltInOps::adjustImageBuiltinType<EBuiltInOps::copyImageToImage3d>(heaplessAllowed);
    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(builtin, *pClDevice);
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

    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::Type::dynamicState, 8192);
    auto usedBefore = indirectHeap.getUsed();
    auto sizeCrossThreadData = kernel->getCrossThreadDataSize();
    HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
        indirectHeap,
        *kernel,
        false,
        nullptr,
        sizeCrossThreadData,
        0,
        pClDevice->getRootDeviceEnvironment());

    auto usedAfter = indirectHeap.getUsed();
    EXPECT_EQ(kernel->getCrossThreadDataSize(), usedAfter - usedBefore);
}

HWTEST_F(HardwareCommandsTest, givenSendCrossThreadDataWhenWhenAddPatchInfoCommentsForAUBDumpIsNotSetThenAddPatchInfoDataOffsetsAreNotMoved) {
    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    MockContext context;

    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    auto kernelInfo = std::make_unique<KernelInfo>();

    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *kernelInfo, *pClDevice));

    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);

    PatchInfoData patchInfoData = {0xaaaaaaaa, 0, PatchInfoAllocationType::kernelArg, 0xbbbbbbbb, 0, PatchInfoAllocationType::indirectObjectHeap};
    kernel->getPatchInfoDataList().push_back(patchInfoData);
    auto sizeCrossThreadData = kernel->getCrossThreadDataSize();
    HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
        indirectHeap,
        *kernel,
        false,
        nullptr,
        sizeCrossThreadData,
        0,
        pClDevice->getRootDeviceEnvironment());

    ASSERT_EQ(1u, kernel->getPatchInfoDataList().size());
    EXPECT_EQ(0xaaaaaaaa, kernel->getPatchInfoDataList()[0].sourceAllocation);
    EXPECT_EQ(0u, kernel->getPatchInfoDataList()[0].sourceAllocationOffset);
    EXPECT_EQ(PatchInfoAllocationType::kernelArg, kernel->getPatchInfoDataList()[0].sourceType);
    EXPECT_EQ(0xbbbbbbbb, kernel->getPatchInfoDataList()[0].targetAllocation);
    EXPECT_EQ(0u, kernel->getPatchInfoDataList()[0].targetAllocationOffset);
    EXPECT_EQ(PatchInfoAllocationType::indirectObjectHeap, kernel->getPatchInfoDataList()[0].targetType);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsTest, givenIndirectHeapNotAllocatedFromInternalPoolWhenSendCrossThreadDataIsCalledThenOffsetZeroIsReturned) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    auto nonInternalAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap indirectHeap(nonInternalAllocation, false);

    auto sizeCrossThreadData = mockKernelWithInternal->mockKernel->getCrossThreadDataSize();
    auto offset = HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
        indirectHeap,
        *mockKernelWithInternal->mockKernel,
        false,
        nullptr,
        sizeCrossThreadData,
        0,
        pClDevice->getRootDeviceEnvironment());
    EXPECT_EQ(0u, offset);
    pDevice->getMemoryManager()->freeGraphicsMemory(nonInternalAllocation);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsTest, givenIndirectHeapAllocatedFromInternalPoolWhenSendCrossThreadDataIsCalledThenHeapBaseOffsetIsReturned) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    auto internalAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties(pDevice->getRootDeviceIndex(), true, MemoryConstants::pageSize, AllocationType::internalHeap, pDevice->getDeviceBitfield()));
    IndirectHeap indirectHeap(internalAllocation, true);
    auto expectedOffset = internalAllocation->getGpuAddressToPatch();

    auto sizeCrossThreadData = mockKernelWithInternal->mockKernel->getCrossThreadDataSize();
    auto offset = HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
        indirectHeap,
        *mockKernelWithInternal->mockKernel,
        false,
        nullptr,
        sizeCrossThreadData,
        0,
        pClDevice->getRootDeviceEnvironment());
    EXPECT_EQ(expectedOffset, offset);

    pDevice->getMemoryManager()->freeGraphicsMemory(internalAllocation);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsTest, givenSendCrossThreadDataWhenWhenAddPatchInfoCommentsForAUBDumpIsSetThenAddPatchInfoDataOffsetsAreMoved) {
    DebugManagerStateRestore dbgRestore;
    debugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    MockContext context;

    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    auto kernelInfo = std::make_unique<KernelInfo>();

    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *kernelInfo, *pClDevice));

    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
    indirectHeap.getSpace(128u);

    PatchInfoData patchInfoData1 = {0xaaaaaaaa, 0, PatchInfoAllocationType::kernelArg, 0xbbbbbbbb, 0, PatchInfoAllocationType::indirectObjectHeap};
    PatchInfoData patchInfoData2 = {0xcccccccc, 0, PatchInfoAllocationType::indirectObjectHeap, 0xdddddddd, 0, PatchInfoAllocationType::defaultType};

    kernel->getPatchInfoDataList().push_back(patchInfoData1);
    kernel->getPatchInfoDataList().push_back(patchInfoData2);
    auto sizeCrossThreadData = kernel->getCrossThreadDataSize();
    auto offsetCrossThreadData = HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
        indirectHeap,
        *kernel,
        false,
        nullptr,
        sizeCrossThreadData,
        0,
        pClDevice->getRootDeviceEnvironment());

    ASSERT_NE(0u, offsetCrossThreadData);
    EXPECT_EQ(128u, offsetCrossThreadData);

    ASSERT_EQ(2u, kernel->getPatchInfoDataList().size());
    EXPECT_EQ(0xaaaaaaaa, kernel->getPatchInfoDataList()[0].sourceAllocation);
    EXPECT_EQ(0u, kernel->getPatchInfoDataList()[0].sourceAllocationOffset);
    EXPECT_EQ(PatchInfoAllocationType::kernelArg, kernel->getPatchInfoDataList()[0].sourceType);
    EXPECT_NE(0xbbbbbbbb, kernel->getPatchInfoDataList()[0].targetAllocation);
    EXPECT_EQ(indirectHeap.getGraphicsAllocation()->getGpuAddress(), kernel->getPatchInfoDataList()[0].targetAllocation);
    EXPECT_NE(0u, kernel->getPatchInfoDataList()[0].targetAllocationOffset);
    EXPECT_EQ(offsetCrossThreadData, kernel->getPatchInfoDataList()[0].targetAllocationOffset);
    EXPECT_EQ(PatchInfoAllocationType::indirectObjectHeap, kernel->getPatchInfoDataList()[0].targetType);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsTest, WhenAllocatingIndirectStateResourceThenCorrectSizeIsAllocated) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    std::unique_ptr<Image> srcImage(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, srcImage.get());
    std::unique_ptr<Image> dstImage(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, dstImage.get());

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyImageToImage3d,
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
    const uint32_t threadGroupCount = 1u;

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::dynamicState, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::surfaceState, 8192);
    auto usedBeforeCS = commandStream.getUsed();
    auto usedBeforeDSH = dsh.getUsed();
    auto usedBeforeIOH = ioh.getUsed();
    auto usedBeforeSSH = ssh.getUsed();

    dsh.align(EncodeStates<FamilyType>::alignInterfaceDescriptorData);
    size_t idToffset = dsh.getUsed();
    dsh.getSpace(sizeof(INTERFACE_DESCRIPTOR_DATA));

    HardwareCommandsHelper<FamilyType>::sendMediaInterfaceDescriptorLoad(
        commandStream,
        idToffset,
        sizeof(INTERFACE_DESCRIPTOR_DATA));
    uint32_t interfaceDescriptorIndex = 0;
    auto isCcsUsed = EngineHelpers::isCcs(cmdQ.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*kernel);

    HardwareCommandsHelper<FamilyType>::template sendIndirectState<GPGPU_WALKER, INTERFACE_DESCRIPTOR_DATA>(
        commandStream,
        dsh,
        ioh,
        ssh,
        *kernel,
        kernel->getKernelStartAddress(true, kernelUsesLocalIds, isCcsUsed, false),
        kernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        threadGroupCount,
        idToffset,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        0,
        *pDevice);

    // It's okay these are EXPECT_GE as they're only going to be used for
    // estimation purposes to avoid OOM.
    auto usedAfterDSH = dsh.getUsed();
    auto usedAfterIOH = ioh.getUsed();
    auto usedAfterSSH = ssh.getUsed();
    auto sizeRequiredDSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredDSH(*kernel);
    auto sizeRequiredIOH = HardwareCommandsHelper<FamilyType>::getSizeRequiredIOH(*kernel, localWorkSizes, pDevice->getRootDeviceEnvironment());
    auto sizeRequiredSSH = HardwareCommandsHelper<FamilyType>::getSizeRequiredSSH(*kernel);

    EXPECT_GE(sizeRequiredDSH, usedAfterDSH - usedBeforeDSH);
    EXPECT_GE(sizeRequiredIOH, usedAfterIOH - usedBeforeIOH);
    EXPECT_GE(sizeRequiredSSH, usedAfterSSH - usedBeforeSSH);

    auto usedAfterCS = commandStream.getUsed();
    EXPECT_GE(HardwareCommandsHelper<FamilyType>::getSizeRequiredCS(), usedAfterCS - usedBeforeCS);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsTest, givenKernelWithFourBindingTableEntriesWhenIndirectStateIsEmittedThenInterfaceDescriptorContainsCorrectBindingTableEntryCount) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

    auto expectedBindingTableCount = 3u;
    mockKernelWithInternal->mockKernel->numberOfBindingTableStates = expectedBindingTableCount;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::dynamicState, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::surfaceState, 8192);
    const size_t localWorkSize = 256;
    const size_t localWorkSizes[3]{localWorkSize, 1, 1};
    const uint32_t threadGroupCount = 1u;
    uint32_t interfaceDescriptorIndex = 0;
    auto isCcsUsed = EngineHelpers::isCcs(cmdQ.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*mockKernelWithInternal->mockKernel);

    HardwareCommandsHelper<FamilyType>::template sendIndirectState<GPGPU_WALKER, INTERFACE_DESCRIPTOR_DATA>(
        commandStream,
        dsh,
        ioh,
        ssh,
        *mockKernelWithInternal->mockKernel,
        mockKernelWithInternal->mockKernel->getKernelStartAddress(true, kernelUsesLocalIds, isCcsUsed, false),
        mockKernelWithInternal->mockKernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        threadGroupCount,
        0,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        0,
        *pDevice);

    auto interfaceDescriptor = reinterpret_cast<INTERFACE_DESCRIPTOR_DATA *>(dsh.getCpuBase());
    if (EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
        EXPECT_EQ(expectedBindingTableCount, interfaceDescriptor->getBindingTableEntryCount());
    } else {
        EXPECT_EQ(0u, interfaceDescriptor->getBindingTableEntryCount());
    }
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsTest, givenKernelWith100BindingTableEntriesWhenIndirectStateIsEmittedThenInterfaceDescriptorHas31BindingTableEntriesSet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

    auto expectedBindingTableCount = 100u;
    mockKernelWithInternal->mockKernel->numberOfBindingTableStates = expectedBindingTableCount;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::dynamicState, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::surfaceState, 8192);
    const size_t localWorkSize = 256;
    const size_t localWorkSizes[3]{localWorkSize, 1, 1};
    const uint32_t threadGroupCount = 1u;
    uint32_t interfaceDescriptorIndex = 0;
    auto isCcsUsed = EngineHelpers::isCcs(cmdQ.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*mockKernelWithInternal->mockKernel);

    HardwareCommandsHelper<FamilyType>::template sendIndirectState<GPGPU_WALKER, INTERFACE_DESCRIPTOR_DATA>(
        commandStream,
        dsh,
        ioh,
        ssh,
        *mockKernelWithInternal->mockKernel,
        mockKernelWithInternal->mockKernel->getKernelStartAddress(true, kernelUsesLocalIds, isCcsUsed, false),
        mockKernelWithInternal->mockKernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        threadGroupCount,
        0,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        0,
        *pDevice);

    auto interfaceDescriptor = reinterpret_cast<INTERFACE_DESCRIPTOR_DATA *>(dsh.getCpuBase());
    if (EncodeSurfaceState<FamilyType>::doBindingTablePrefetch()) {
        EXPECT_EQ(31u, interfaceDescriptor->getBindingTableEntryCount());
    } else {
        EXPECT_EQ(0u, interfaceDescriptor->getBindingTableEntryCount());
    }
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsTest, whenSendingIndirectStateThenKernelsWalkOrderIsTakenIntoAccount) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    std::unique_ptr<Image> img(Image2dHelperUlt<>::create(pContext));

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyImageToImage3d,
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
    const uint32_t threadGroupCount = 1u;

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::dynamicState, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::surfaceState, 8192);

    dsh.align(EncodeStates<FamilyType>::alignInterfaceDescriptorData);
    size_t idToffset = dsh.getUsed();
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

    HardwareCommandsHelper<FamilyType>::template sendIndirectState<GPGPU_WALKER, INTERFACE_DESCRIPTOR_DATA>(
        commandStream,
        dsh,
        ioh,
        ssh,
        mockKernel,
        mockKernel.getKernelStartAddress(true, kernelUsesLocalIds, isCcsUsed, false),
        modifiedKernelInfo.getMaxSimdSize(),
        localWorkSizes,
        threadGroupCount,
        idToffset,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        0,
        *pDevice);

    constexpr uint32_t grfSize = sizeof(typename FamilyType::GRF);
    size_t localWorkSize = localWorkSizeX * localWorkSizeY * localWorkSizeZ;
    auto numChannels = modifiedKernelInfo.kernelDescriptor.kernelAttributes.numLocalIdChannels;
    auto numGrf = GrfConfig::defaultGrfNumber;
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    size_t expectedIohSize = PerThreadDataHelper::getPerThreadDataSizeTotal(modifiedKernelInfo.getMaxSimdSize(), grfSize, numGrf, numChannels, localWorkSize, rootDeviceEnvironment);
    ASSERT_LE(expectedIohSize, ioh.getUsed());

    auto expectedLocalIds = alignedMalloc(expectedIohSize, 64);
    generateLocalIDs(expectedLocalIds, modifiedKernelInfo.getMaxSimdSize(),
                     std::array<uint16_t, 3>{{localWorkSizeX, localWorkSizeY, localWorkSizeZ}},
                     std::array<uint8_t, 3>{{modifiedKernelInfo.kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[0],
                                             modifiedKernelInfo.kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[1],
                                             modifiedKernelInfo.kernelDescriptor.kernelAttributes.workgroupDimensionsOrder[2]}},
                     false, grfSize, numGrf, rootDeviceEnvironment);

    EXPECT_EQ(0, memcmp(expectedLocalIds, ioh.getCpuBase(), expectedIohSize));
    alignedFree(expectedLocalIds);
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsTest, WhenSendingIndirectStateThenBindingTableStatesPointersAreCorrect) {
    typedef typename FamilyType::RENDER_SURFACE_STATE RENDER_SURFACE_STATE;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);
    std::unique_ptr<Image> dstImage(Image2dHelperUlt<>::create(pContext));
    ASSERT_NE(nullptr, dstImage.get());

    auto &builder = BuiltInDispatchBuilderOp::getBuiltinDispatchInfoBuilder(EBuiltInOps::copyBufferToImage3d,
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
    const uint32_t threadGroupCount = 1u;

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::dynamicState, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::surfaceState, 8192);

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

    HardwareCommandsHelper<FamilyType>::template sendIndirectState<GPGPU_WALKER, INTERFACE_DESCRIPTOR_DATA>(
        commandStream,
        dsh,
        ioh,
        ssh,
        *kernel,
        kernel->getKernelStartAddress(true, kernelUsesLocalIds, isCcsUsed, false),
        kernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        threadGroupCount,
        0,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        0,
        *pDevice);

    EXPECT_EQ(sshUsed + 0x00000000u, *(&bindingTableStatesPointers[0]));
    EXPECT_EQ(sshUsed + 0x00000040u, *(&bindingTableStatesPointers[1]));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsTest, WhenGettingBindingTableStateThenSurfaceStatePointersAreCorrect) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    // define kernel info
    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 32;

    // define patch offsets for global, constant, private, event pool and default device queue surfaces
    pKernelInfo->setGlobalVariablesSurface(8, 0, 0);
    pKernelInfo->setGlobalConstantsSurface(8, 8, 64);
    pKernelInfo->setPrivateMemory(32, false, 8, 16, 128);
    pKernelInfo->setDeviceSideEnqueueDefaultQueueSurface(8, 32, 256);

    // create program with valid context
    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));

    // setup global memory
    char globalBuffer[16];
    GraphicsAllocation gfxGlobalAlloc(0, 1u /*num gmms*/, AllocationType::unknown, globalBuffer, castToUint64(globalBuffer), 0llu, sizeof(globalBuffer), MemoryPool::memoryNull, MemoryManager::maxOsContextCount);
    program.setGlobalSurface(&gfxGlobalAlloc);

    // setup constant memory
    char constBuffer[16];
    GraphicsAllocation gfxConstAlloc(0, 1u /*num gmms*/, AllocationType::unknown, constBuffer, castToUint64(constBuffer), 0llu, sizeof(constBuffer), MemoryPool::memoryNull, MemoryManager::maxOsContextCount);
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
    pKernelInfo->heapInfo.surfaceStateHeapSize = sshSize;

    // setup kernel heap
    uint32_t kernelIsa[32];
    pKernelInfo->heapInfo.pKernelHeap = kernelIsa;
    pKernelInfo->heapInfo.kernelHeapSize = sizeof(kernelIsa);

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

        auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::dynamicState, 8192);
        auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
        auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::surfaceState, 8192);

        // Initialize binding table state pointers with pattern
        EXPECT_EQ(numSurfaces, pKernel->getNumberOfBindingTableStates());

        const size_t localWorkSizes[3]{256, 1, 1};
        const uint32_t threadGroupCount = 1u;

        dsh.getSpace(sizeof(INTERFACE_DESCRIPTOR_DATA));

        ssh.getSpace(ssbaOffset); // offset local ssh from surface state base address

        uint32_t localSshOffset = static_cast<uint32_t>(ssh.getUsed());

        // push surfaces states and binding table to given ssh heap
        uint32_t interfaceDescriptorIndex = 0;
        auto isCcsUsed = EngineHelpers::isCcs(cmdQ.getGpgpuEngine().osContext->getEngineType());
        auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*pKernel);

        HardwareCommandsHelper<FamilyType>::template sendIndirectState<GPGPU_WALKER, INTERFACE_DESCRIPTOR_DATA>(
            commandStream,
            dsh,
            ioh,
            ssh,
            *pKernel,
            pKernel->getKernelStartAddress(true, kernelUsesLocalIds, isCcsUsed, false),
            pKernel->getKernelInfo().getMaxSimdSize(),
            localWorkSizes,
            threadGroupCount,
            0,
            interfaceDescriptorIndex,
            pDevice->getPreemptionMode(),
            pWalkerCmd,
            nullptr,
            true,
            0,
            *pDevice);

        bti = reinterpret_cast<typename FamilyType::BINDING_TABLE_STATE *>(reinterpret_cast<unsigned char *>(ssh.getCpuBase()) + localSshOffset + btiOffset);
        for (uint32_t i = 0; i < numSurfaces; ++i) {
            uint32_t expected = localSshOffset + i * sizeof(typename FamilyType::RENDER_SURFACE_STATE);
            EXPECT_EQ(expected, bti[i].getSurfaceStatePointer());
        }

        program.setGlobalSurface(nullptr);
        program.setConstantSurface(nullptr);

        // exhaust space to trigger reload
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
    pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(surfaceStateHeap);

    pKernelInfo->addArgBuffer(0, 0, 0, 0);
    pKernelInfo->setAddressQualifier(0, KernelArgMetadata::AddrGlobal);

    // initialize kernel
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::surfaceState, 8192);

    ssh.align(8);
    auto usedBefore = ssh.getUsed();

    // Initialize binding table state pointers with pattern
    auto numSurfaceStates = pKernel->getNumberOfBindingTableStates();
    EXPECT_EQ(0u, numSurfaceStates);

    // set binding table states
    auto dstBindingTablePointer = HardwareCommandsHelper<FamilyType>::checkForAdditionalBTAndSetBTPointer(ssh, *pKernel);
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
    pKernelInfo->heapInfo.surfaceStateHeapSize = sizeof(surfaceStateHeap);

    // initialize kernel
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());

    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::surfaceState, 8192);

    // Initialize binding table state pointers with pattern
    auto numSurfaceStates = pKernel->getNumberOfBindingTableStates();
    EXPECT_EQ(0u, numSurfaceStates);

    auto dstBindingTablePointer = HardwareCommandsHelper<FamilyType>::checkForAdditionalBTAndSetBTPointer(ssh, *pKernel);
    EXPECT_EQ(0u, dstBindingTablePointer);

    dstBindingTablePointer = HardwareCommandsHelper<FamilyType>::checkForAdditionalBTAndSetBTPointer(ssh, *pKernel);
    EXPECT_EQ(0u, dstBindingTablePointer);

    pKernelInfo->setBindingTable(64, 0);

    dstBindingTablePointer = HardwareCommandsHelper<FamilyType>::checkForAdditionalBTAndSetBTPointer(ssh, *pKernel);
    EXPECT_EQ(0u, dstBindingTablePointer);

    delete pKernel;
}

HWTEST2_F(HardwareCommandsTest, givenNoBTEntriesInKernelDescriptorAndGTPinInitializedWhenSettingBTPointerThenBTPointerIsSet, IsHeapfulRequired) {
    isGTPinInitialized = true;

    auto pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    ASSERT_EQ(0u, pKernelInfo->kernelDescriptor.payloadMappings.bindingTable.numEntries);

    MockContext context;
    MockProgram program(&context, false, toClDeviceVector(*pClDevice));

    auto pKernel = std::make_unique<MockKernel>(&program, *pKernelInfo, *pClDevice);

    constexpr auto mockSshSize{256u};
    constexpr auto mockBTOffset{32u};
    auto mockSsh = std::make_unique<char[]>(mockSshSize);
    ASSERT_EQ(CL_SUCCESS, pKernel->initialize());
    pKernel->resizeSurfaceStateHeap(mockSsh.get(), mockSshSize, 1u, mockBTOffset);
    mockSsh.release();

    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::surfaceState, 8192);

    auto dstBindingTablePointer = HardwareCommandsHelper<FamilyType>::checkForAdditionalBTAndSetBTPointer(ssh, *pKernel);
    EXPECT_NE(0u, dstBindingTablePointer);
    isGTPinInitialized = false;
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsTest, GivenKernelWithInvalidSamplerStateArrayWhenSendIndirectStateIsCalledThenInterfaceDescriptorIsNotPopulated) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;
    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::dynamicState, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::surfaceState, 8192);
    const size_t localWorkSize = 256;
    const size_t localWorkSizes[3]{localWorkSize, 1, 1};
    const uint32_t threadGroupCount = 1u;
    uint32_t interfaceDescriptorIndex = 0;
    auto isCcsUsed = EngineHelpers::isCcs(cmdQ.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*mockKernelWithInternal->mockKernel);

    // Undefined Offset, Defined BorderColorOffset
    mockKernelWithInternal->kernelInfo.setSamplerTable(0, 2, undefined<uint16_t>);

    HardwareCommandsHelper<FamilyType>::template sendIndirectState<GPGPU_WALKER, INTERFACE_DESCRIPTOR_DATA>(
        commandStream,
        dsh,
        ioh,
        ssh,
        *mockKernelWithInternal->mockKernel,
        mockKernelWithInternal->mockKernel->getKernelStartAddress(true, kernelUsesLocalIds, isCcsUsed, false),
        mockKernelWithInternal->mockKernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        threadGroupCount,
        0,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        0,
        *pDevice);

    auto interfaceDescriptor = reinterpret_cast<INTERFACE_DESCRIPTOR_DATA *>(dsh.getCpuBase());
    EXPECT_EQ(0U, interfaceDescriptor->getSamplerStatePointer());
    EXPECT_EQ(0U, interfaceDescriptor->getSamplerCount());

    // Defined Offset, Undefined BorderColorOffset
    mockKernelWithInternal->kernelInfo.setSamplerTable(undefined<uint16_t>, 2, 0);

    HardwareCommandsHelper<FamilyType>::template sendIndirectState<GPGPU_WALKER, INTERFACE_DESCRIPTOR_DATA>(
        commandStream,
        dsh,
        ioh,
        ssh,
        *mockKernelWithInternal->mockKernel,
        mockKernelWithInternal->mockKernel->getKernelStartAddress(true, kernelUsesLocalIds, isCcsUsed, false),
        mockKernelWithInternal->mockKernel->getKernelInfo().getMaxSimdSize(),
        localWorkSizes,
        threadGroupCount,
        0,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        0,
        *pDevice);

    interfaceDescriptor = reinterpret_cast<INTERFACE_DESCRIPTOR_DATA *>(dsh.getCpuBase());
    EXPECT_EQ(0U, interfaceDescriptor->getSamplerStatePointer());
    EXPECT_EQ(0U, interfaceDescriptor->getSamplerCount());
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsTest, GivenKernelWithSamplersWhenIndirectStateIsProgrammedThenBorderColorIsCorrectlyCopiedToDshAndSamplerStatesAreProgrammedWithPointer) {
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using GPGPU_WALKER = typename FamilyType::GPGPU_WALKER;

    CommandQueueHw<FamilyType> cmdQ(nullptr, pClDevice, 0, false);
    const size_t localWorkSizes[3]{1, 1, 1};
    const uint32_t threadGroupCount = 1u;

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<GPGPU_WALKER *>(commandStream.getSpace(sizeof(GPGPU_WALKER)));
    *pWalkerCmd = FamilyType::cmdInitGpgpuWalker;

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::dynamicState, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::surfaceState, 8192);

    const uint32_t samplerTableOffset = 64;
    const uint32_t samplerStateSize = sizeof(SAMPLER_STATE) * 2;
    mockKernelWithInternal->kernelInfo.setSamplerTable(0, 2, static_cast<DynamicStateHeapOffset>(samplerTableOffset));

    const uint32_t mockDshSize = (samplerTableOffset + samplerStateSize) * 4;
    char *mockDsh = new char[mockDshSize];
    memset(mockDsh, 6, samplerTableOffset);
    memset(mockDsh + samplerTableOffset, 8, samplerTableOffset);
    mockKernelWithInternal->kernelInfo.heapInfo.pDsh = mockDsh;
    mockKernelWithInternal->kernelInfo.heapInfo.dynamicStateHeapSize = mockDshSize;
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

    HardwareCommandsHelper<FamilyType>::template sendIndirectState<GPGPU_WALKER, INTERFACE_DESCRIPTOR_DATA>(
        commandStream,
        dsh,
        ioh,
        ssh,
        *mockKernelWithInternal->mockKernel,
        mockKernelWithInternal->mockKernel->getKernelStartAddress(true, kernelUsesLocalIds, isCcsUsed, false),
        8,
        localWorkSizes,
        threadGroupCount,
        interfaceDescriptorTableOffset,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        nullptr,
        true,
        0,
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

HWTEST2_F(HardwareCommandsTest, givenBindlessKernelWithBufferArgWhenSendIndirectStateThenSurfaceStateIsCopiedToHeapAndCrossThreadDataIsCorrectlyPatched, IsHeapfulRequiredAndAtLeastXeCore) {

    using WalkerType = typename FamilyType::COMPUTE_WALKER;
    using InterfaceDescriptorType = typename WalkerType::InterfaceDescriptorType;

    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    auto &commandStream = cmdQ.getCS(1024);
    auto pWalkerCmd = static_cast<WalkerType *>(commandStream.getSpace(sizeof(WalkerType)));

    // define kernel info
    std::unique_ptr<MockKernelInfo> pKernelInfo = std::make_unique<MockKernelInfo>();
    pKernelInfo->kernelDescriptor.kernelAttributes.simdSize = 1;
    pKernelInfo->addArgBuffer(0, 0x30, sizeof(void *), 0x0);
    pKernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = KernelDescriptor::AddressingMode::BindlessAndStateless;

    const auto bindlessOffset = 0x10;
    pKernelInfo->argAsPtr(0).bindless = bindlessOffset;
    pKernelInfo->kernelDescriptor.initBindlessOffsetToSurfaceState();

    pKernelInfo->kernelDescriptor.kernelAttributes.crossThreadDataSize = 1024;

    MockKernel mockKernel(mockKernelWithInternal->mockKernel->getProgram(), *pKernelInfo, *pClDevice);

    auto retVal = mockKernel.initialize();
    EXPECT_EQ(0, retVal);

    memset(mockKernel.getSurfaceStateHeap(), 0x22, mockKernel.getSurfaceStateHeapSize());
    memset(mockKernel.getCrossThreadData(), 0x00, mockKernel.getCrossThreadDataSize());

    auto &dsh = cmdQ.getIndirectHeap(IndirectHeap::Type::dynamicState, 8192);
    auto &ioh = cmdQ.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
    auto &ssh = cmdQ.getIndirectHeap(IndirectHeap::Type::surfaceState, 8192);

    const auto expectedDestinationInHeap = ssh.getSpace(0);
    const uint64_t bindlessSurfaceStateBaseOffset = ptrDiff(ssh.getSpace(0), ssh.getCpuBase());

    const size_t localWorkSize = 256;
    const size_t localWorkSizes[3]{localWorkSize, 1, 1};
    const uint32_t threadGroupCount = 1u;
    uint32_t interfaceDescriptorIndex = 0;
    auto isCcsUsed = EngineHelpers::isCcs(cmdQ.getGpgpuEngine().osContext->getEngineType());
    auto kernelUsesLocalIds = HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(mockKernel);

    InterfaceDescriptorType interfaceDescriptorData;
    HardwareCommandsHelper<FamilyType>::template sendIndirectState<WalkerType, InterfaceDescriptorType>(
        commandStream,
        dsh,
        ioh,
        ssh,
        mockKernel,
        mockKernel.getKernelStartAddress(true, kernelUsesLocalIds, isCcsUsed, false),
        pKernelInfo->getMaxSimdSize(),
        localWorkSizes,
        threadGroupCount,
        0,
        interfaceDescriptorIndex,
        pDevice->getPreemptionMode(),
        pWalkerCmd,
        &interfaceDescriptorData,
        true,
        0,
        *pDevice);

    EXPECT_EQ(0, std::memcmp(expectedDestinationInHeap, mockKernel.getSurfaceStateHeap(), mockKernel.getSurfaceStateHeapSize()));

    const auto &gfxCoreHelper = mockKernel.getGfxCoreHelper();
    const auto surfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();

    const auto ssIndex = pKernelInfo->kernelDescriptor.bindlessArgsMap.find(bindlessOffset)->second;
    const auto surfaceStateOffset = static_cast<uint32_t>(bindlessSurfaceStateBaseOffset + ssIndex * surfaceStateSize);
    const auto expectedPatchValue = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(surfaceStateOffset));
    const auto expectedPatchLocation = reinterpret_cast<uint32_t *>(ptrOffset(mockKernel.getCrossThreadData(), bindlessOffset));

    EXPECT_EQ(expectedPatchValue, *expectedPatchLocation);
}

HWTEST_F(HardwareCommandsTest, whenNumLocalIdsIsBiggerThanZeroThenExpectLocalIdsInUseIsTrue) {
    mockKernelWithInternal->kernelInfo.kernelDescriptor.kernelAttributes.numLocalIdChannels = 1;
    EXPECT_TRUE(HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*mockKernelWithInternal->mockKernel));
}

HWTEST_F(HardwareCommandsTest, whenNumLocalIdsIsZeroThenExpectLocalIdsInUseIsFalse) {
    mockKernelWithInternal->kernelInfo.kernelDescriptor.kernelAttributes.numLocalIdChannels = 0;
    EXPECT_FALSE(HardwareCommandsHelper<FamilyType>::kernelUsesLocalIds(*mockKernelWithInternal->mockKernel));
}

struct HardwareCommandsImplicitArgsTests : Test<ClDeviceFixture> {

    void SetUp() override {
        ClDeviceFixture::setUp();
        indirectHeapAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});

        memset(&expectedImplicitArgs, 0, sizeof(expectedImplicitArgs));
        expectedImplicitArgs.header.structSize = ImplicitArgsV0::getSize();

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
        ClDeviceFixture::tearDown();
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
        kernel.implicitArgsVersion = 0;
        ASSERT_EQ(CL_SUCCESS, kernel.initialize());
        auto pImplicitArgs = kernel.getImplicitArgs();

        ASSERT_NE(nullptr, pImplicitArgs);

        kernel.setCrossThreadData(nullptr, sizeof(uint64_t));

        kernel.setWorkDim(expectedImplicitArgs.numWorkDim);
        kernel.setLocalWorkSizeValues(expectedImplicitArgs.localSizeX, expectedImplicitArgs.localSizeY, expectedImplicitArgs.localSizeZ);
        kernel.setGlobalWorkSizeValues(static_cast<uint32_t>(expectedImplicitArgs.globalSizeX), static_cast<uint32_t>(expectedImplicitArgs.globalSizeY), static_cast<uint32_t>(expectedImplicitArgs.globalSizeZ));
        kernel.setGlobalWorkOffsetValues(static_cast<uint32_t>(expectedImplicitArgs.globalOffsetX), static_cast<uint32_t>(expectedImplicitArgs.globalOffsetY), static_cast<uint32_t>(expectedImplicitArgs.globalOffsetZ));
        kernel.setNumWorkGroupsValues(expectedImplicitArgs.groupCountX, expectedImplicitArgs.groupCountY, expectedImplicitArgs.groupCountZ);
        const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
        implicitArgsProgrammingSize = ImplicitArgsHelper::getSizeForImplicitArgsPatching(pImplicitArgs, kernel.getDescriptor(), false, rootDeviceEnvironment);

        auto sizeCrossThreadData = kernel.getCrossThreadDataSize();
        using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
        HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
            indirectHeap,
            kernel,
            false,
            nullptr,
            sizeCrossThreadData,
            0,
            pClDevice->getRootDeviceEnvironment());

        EXPECT_LE(implicitArgsProgrammingSize, indirectHeap.getUsed());

        if (FamilyType::supportsCmdSet(IGFX_XE_HP_CORE)) {
            expectedImplicitArgs.localIdTablePtr = indirectHeapAllocation->getGpuAddress();
        }
    }

    ImplicitArgsV0 expectedImplicitArgs = {{ImplicitArgsV0::getSize(), 0}};
    GraphicsAllocation *indirectHeapAllocation = nullptr;
    std::array<uint8_t, 3> workgroupDimOrder{0, 1, 2};
    uint32_t implicitArgsProgrammingSize = 0u;
};

HWCMDTEST_F(IGFX_XE_HP_CORE, HardwareCommandsImplicitArgsTests, givenXeHpAndLaterPlatformWhenSendingIndirectStateForKernelWithImplicitArgsThenImplicitArgsAreSentToIndirectHeapWithLocalIds) {
    dispatchKernelWithImplicitArgs<FamilyType>();

    auto localIdsProgrammingSize = implicitArgsProgrammingSize - ImplicitArgsV0::getAlignedSize();
    auto implicitArgsInIndirectData = ptrOffset(indirectHeapAllocation->getUnderlyingBuffer(), localIdsProgrammingSize);
    EXPECT_EQ(0, memcmp(implicitArgsInIndirectData, &expectedImplicitArgs, ImplicitArgsV0::getSize()));
}

HWCMDTEST_F(IGFX_GEN12LP_CORE, HardwareCommandsImplicitArgsTests, givenPreXeHpPlatformWhenSendingIndirectStateForKernelWithImplicitArgsThenImplicitArgsAreSentToIndirectHeapWithoutLocalIds) {
    dispatchKernelWithImplicitArgs<FamilyType>();

    auto implicitArgsInIndirectData = indirectHeapAllocation->getUnderlyingBuffer();
    EXPECT_EQ(0, memcmp(implicitArgsInIndirectData, &expectedImplicitArgs, ImplicitArgsV0::getSize()));

    auto crossThreadDataInIndirectData = ptrOffset(indirectHeapAllocation->getUnderlyingBuffer(), ImplicitArgsV0::getAlignedSize());

    auto programmedImplicitArgsGpuVA = reinterpret_cast<uint64_t *>(crossThreadDataInIndirectData)[0];
    EXPECT_EQ(indirectHeapAllocation->getGpuAddress(), programmedImplicitArgsGpuVA);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HardwareCommandsImplicitArgsTests, givenKernelWithImplicitArgsAndRuntimeLocalIdsGenerationWhenSendingIndirectStateThenLocalIdsAreGeneratedAndCorrectlyProgrammedInCrossThreadData) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHwGenerationLocalIds.set(0);

    workgroupDimOrder[0] = 2;
    workgroupDimOrder[1] = 1;
    workgroupDimOrder[2] = 0;

    std::array<uint16_t, 3> localSize{2, 3, 4};
    size_t totalLocalSize = localSize[0] * localSize[1] * localSize[2];

    expectedImplicitArgs.localSizeX = localSize[0];
    expectedImplicitArgs.localSizeY = localSize[1];
    expectedImplicitArgs.localSizeZ = localSize[2];

    dispatchKernelWithImplicitArgs<FamilyType>();

    auto grfSize = ImplicitArgsHelper::getGrfSize(expectedImplicitArgs.simdWidth);
    auto numGrf = GrfConfig::defaultGrfNumber;
    auto expectedLocalIds = alignedMalloc(implicitArgsProgrammingSize - ImplicitArgsV0::getAlignedSize(), MemoryConstants::cacheLineSize);
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    generateLocalIDs(expectedLocalIds, expectedImplicitArgs.simdWidth, localSize, workgroupDimOrder, false, grfSize, numGrf, rootDeviceEnvironment);

    auto localIdsProgrammingSize = implicitArgsProgrammingSize - ImplicitArgsV0::getAlignedSize();
    size_t sizeForLocalIds = PerThreadDataHelper::getPerThreadDataSizeTotal(expectedImplicitArgs.simdWidth, grfSize, numGrf, 3u, totalLocalSize, rootDeviceEnvironment);

    EXPECT_EQ(0, memcmp(expectedLocalIds, indirectHeapAllocation->getUnderlyingBuffer(), sizeForLocalIds));
    alignedFree(expectedLocalIds);

    auto implicitArgsInIndirectData = ptrOffset(indirectHeapAllocation->getUnderlyingBuffer(), localIdsProgrammingSize);
    EXPECT_EQ(0, memcmp(implicitArgsInIndirectData, &expectedImplicitArgs, ImplicitArgsV0::getSize()));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HardwareCommandsImplicitArgsTests, givenKernelWithImplicitArgsAndHwLocalIdsGenerationWhenSendingIndirectStateThenLocalIdsAreGeneratedAndCorrectlyProgrammedInCrossThreadData) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableHwGenerationLocalIds.set(1);

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

    auto grfSize = ImplicitArgsHelper::getGrfSize(expectedImplicitArgs.simdWidth);
    auto numGrf = GrfConfig::defaultGrfNumber;
    auto expectedLocalIds = alignedMalloc(implicitArgsProgrammingSize - ImplicitArgsV0::getAlignedSize(), MemoryConstants::cacheLineSize);
    const auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();
    generateLocalIDs(expectedLocalIds, expectedImplicitArgs.simdWidth, localSize, expectedDimOrder, false, grfSize, numGrf, rootDeviceEnvironment);

    auto localIdsProgrammingSize = implicitArgsProgrammingSize - ImplicitArgsV0::getAlignedSize();
    size_t sizeForLocalIds = PerThreadDataHelper::getPerThreadDataSizeTotal(expectedImplicitArgs.simdWidth, grfSize, numGrf, 3u, totalLocalSize, rootDeviceEnvironment);

    EXPECT_EQ(0, memcmp(expectedLocalIds, indirectHeapAllocation->getUnderlyingBuffer(), sizeForLocalIds));
    alignedFree(expectedLocalIds);

    auto implicitArgsInIndirectData = ptrOffset(indirectHeapAllocation->getUnderlyingBuffer(), localIdsProgrammingSize);
    EXPECT_EQ(0, memcmp(implicitArgsInIndirectData, &expectedImplicitArgs, ImplicitArgsV0::getSize()));
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

    auto localIdsProgrammingSize = implicitArgsProgrammingSize - ImplicitArgsV0::getAlignedSize();

    EXPECT_EQ(alignUp(sizeof(expectedLocalIds), MemoryConstants::cacheLineSize), localIdsProgrammingSize);

    auto implicitArgsInIndirectData = ptrOffset(indirectHeapAllocation->getUnderlyingBuffer(), localIdsProgrammingSize);
    EXPECT_EQ(0, memcmp(implicitArgsInIndirectData, &expectedImplicitArgs, ImplicitArgsV0::getSize()));
}

using HardwareCommandsTestXeHpAndLater = HardwareCommandsTest;

HWCMDTEST_F(IGFX_XE_HP_CORE, HardwareCommandsTestXeHpAndLater, givenIndirectHeapNotAllocatedFromInternalPoolWhenSendCrossThreadDataIsCalledThenOffsetZeroIsReturned) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    auto nonInternalAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{pDevice->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap indirectHeap(nonInternalAllocation, false);

    auto expectedOffset = is64bit ? 0u : indirectHeap.getHeapGpuBase();

    auto sizeCrossThreadData = mockKernelWithInternal->mockKernel->getCrossThreadDataSize();
    auto offset = HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
        indirectHeap,
        *mockKernelWithInternal->mockKernel,
        false,
        nullptr,
        sizeCrossThreadData,
        0,
        pClDevice->getRootDeviceEnvironment());
    EXPECT_EQ(expectedOffset, offset);
    pDevice->getMemoryManager()->freeGraphicsMemory(nonInternalAllocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HardwareCommandsTestXeHpAndLater, givenIndirectHeapAllocatedFromInternalPoolWhenSendCrossThreadDataIsCalledThenHeapBaseOffsetIsReturned) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    auto internalAllocation = pDevice->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties(pDevice->getRootDeviceIndex(), true, MemoryConstants::pageSize, AllocationType::internalHeap, pDevice->getDeviceBitfield()));
    IndirectHeap indirectHeap(internalAllocation, true);
    auto expectedOffset = is64bit ? internalAllocation->getGpuAddressToPatch() : 0u;

    auto sizeCrossThreadData = mockKernelWithInternal->mockKernel->getCrossThreadDataSize();
    auto offset = HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
        indirectHeap,
        *mockKernelWithInternal->mockKernel,
        false,
        nullptr,
        sizeCrossThreadData,
        0,
        pClDevice->getRootDeviceEnvironment());
    EXPECT_EQ(expectedOffset, offset);

    pDevice->getMemoryManager()->freeGraphicsMemory(internalAllocation);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, HardwareCommandsTestXeHpAndLater, givenSendCrossThreadDataWhenWhenAddPatchInfoCommentsForAUBDumpIsSetThenAddPatchInfoDataOffsetsAreMoved) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    DebugManagerStateRestore dbgRestore;
    debugManager.flags.AddPatchInfoCommentsForAUBDump.set(true);

    CommandQueueHw<FamilyType> cmdQ(pContext, pClDevice, 0, false);

    MockContext context;

    MockProgram program(&context, false, toClDeviceVector(*pClDevice));
    auto kernelInfo = std::make_unique<KernelInfo>();

    std::unique_ptr<MockKernel> kernel(new MockKernel(&program, *kernelInfo, *pClDevice));

    auto &indirectHeap = cmdQ.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
    indirectHeap.getSpace(128u);

    PatchInfoData patchInfoData1 = {0xaaaaaaaa, 0, PatchInfoAllocationType::kernelArg, 0xbbbbbbbb, 0, PatchInfoAllocationType::indirectObjectHeap};
    PatchInfoData patchInfoData2 = {0xcccccccc, 0, PatchInfoAllocationType::indirectObjectHeap, 0xdddddddd, 0, PatchInfoAllocationType::defaultType};

    kernel->getPatchInfoDataList().push_back(patchInfoData1);
    kernel->getPatchInfoDataList().push_back(patchInfoData2);
    auto sizeCrossThreadData = kernel->getCrossThreadDataSize();
    auto offsetCrossThreadData = HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
        indirectHeap,
        *kernel,
        false,
        nullptr,
        sizeCrossThreadData,
        0,
        pClDevice->getRootDeviceEnvironment());

    auto expectedOffsetRelativeToIohBase = alignUp(128u, FamilyType::cacheLineSize);
    auto iohBaseAddress = is64bit ? 0u : indirectHeap.getHeapGpuBase();

    ASSERT_NE(0u, offsetCrossThreadData);
    EXPECT_EQ(iohBaseAddress + expectedOffsetRelativeToIohBase, offsetCrossThreadData);

    ASSERT_EQ(2u, kernel->getPatchInfoDataList().size());
    EXPECT_EQ(0xaaaaaaaa, kernel->getPatchInfoDataList()[0].sourceAllocation);
    EXPECT_EQ(0u, kernel->getPatchInfoDataList()[0].sourceAllocationOffset);
    EXPECT_EQ(PatchInfoAllocationType::kernelArg, kernel->getPatchInfoDataList()[0].sourceType);
    EXPECT_NE(0xbbbbbbbb, kernel->getPatchInfoDataList()[0].targetAllocation);
    EXPECT_EQ(indirectHeap.getGraphicsAllocation()->getGpuAddress(), kernel->getPatchInfoDataList()[0].targetAllocation);
    EXPECT_NE(0u, kernel->getPatchInfoDataList()[0].targetAllocationOffset);
    EXPECT_EQ(expectedOffsetRelativeToIohBase, kernel->getPatchInfoDataList()[0].targetAllocationOffset);
    EXPECT_EQ(PatchInfoAllocationType::indirectObjectHeap, kernel->getPatchInfoDataList()[0].targetType);
}
