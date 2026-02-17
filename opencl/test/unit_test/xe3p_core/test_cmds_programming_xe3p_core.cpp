/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/indirect_heap/indirect_heap_type.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/allocation_type.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/test/unit_test/fixtures/ult_command_stream_receiver_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"

#include "encode_surface_state_args.h"
#include "gtest/gtest.h"
#include "hw_cmds_xe3p_core.h"
#include "per_product_test_definitions.h"

#include <memory>

using namespace NEO;
using CmdsProgrammingTestsXe3pCore = UltCommandStreamReceiverTest;

XE3P_CORETEST_F(CmdsProgrammingTestsXe3pCore, givenL3ToL1DebugFlagWhenStatelessMocsIsProgrammedThenItHasL1CachingOn) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    DebugManagerStateRestore restore;
    debugManager.flags.ForceL1Caching.set(1u);

    auto &commandStreamReceiver = pDevice->getUltCommandStreamReceiver<FamilyType>();
    flushTask(commandStreamReceiver);

    HardwareParse hwParserCsr;
    hwParserCsr.parseCommands<FamilyType>(commandStreamReceiver.commandStream, 0);
    hwParserCsr.findHardwareCommands<FamilyType>();
    ASSERT_NE(nullptr, hwParserCsr.cmdStateBaseAddress);

    auto stateBaseAddress = static_cast<STATE_BASE_ADDRESS *>(hwParserCsr.cmdStateBaseAddress);

    auto actualL1CachePolicy = static_cast<uint8_t>(stateBaseAddress->getL1CacheControlCachePolicy());

    EXPECT_EQ(pDevice->getProductHelper().getL1CachePolicy(false), actualL1CachePolicy);
}

XE3P_CORETEST_F(CmdsProgrammingTestsXe3pCore, whenAppendingRssThenProgramWtL1CachePolicy) {
    auto memoryManager = pDevice->getExecutionEnvironment()->memoryManager.get();
    size_t allocationSize = MemoryConstants::pageSize;
    AllocationProperties properties(pDevice->getRootDeviceIndex(), allocationSize, AllocationType::buffer, pDevice->getDeviceBitfield());
    auto allocation = memoryManager->allocateGraphicsMemoryWithProperties(properties);

    auto rssCmd = FamilyType::cmdInitRenderSurfaceState;

    MockContext context(pClDevice);
    auto multiGraphicsAllocation = MultiGraphicsAllocation(pClDevice->getRootDeviceIndex());
    multiGraphicsAllocation.addAllocation(allocation);

    std::unique_ptr<BufferHw<FamilyType>> buffer(static_cast<BufferHw<FamilyType> *>(
        BufferHw<FamilyType>::create(&context, {}, 0, 0, allocationSize, nullptr, nullptr, std::move(multiGraphicsAllocation), false, false, false)));

    NEO::EncodeSurfaceStateArgs args;
    args.outMemory = &rssCmd;
    args.graphicsAddress = allocation->getGpuAddress();
    args.size = allocation->getUnderlyingBufferSize();
    args.mocs = buffer->getMocsValue(false, false, pClDevice->getRootDeviceIndex());
    args.numAvailableDevices = pClDevice->getNumGenericSubDevices();
    args.allocation = allocation;
    args.gmmHelper = pClDevice->getGmmHelper();
    args.areMultipleSubDevicesInContext = true;

    EncodeSurfaceState<FamilyType>::encodeBuffer(args);

    EXPECT_EQ(pDevice->getProductHelper().getL1CachePolicy(false), rssCmd.getL1CacheControlCachePolicy());
}

XE3P_CORETEST_F(CmdsProgrammingTestsXe3pCore, givenAlignedCacheableReadOnlyBufferThenChoseOclBufferConstPolicy) {
    MockContext context;
    const auto size = MemoryConstants::pageSize;
    const auto ptr = (void *)alignedMalloc(size * 2, MemoryConstants::pageSize);
    const auto flags = CL_MEM_USE_HOST_PTR | CL_MEM_READ_ONLY;

    auto retVal = CL_SUCCESS;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(
        &context,
        flags,
        size,
        ptr,
        retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    typename FamilyType::RENDER_SURFACE_STATE surfaceState = {};
    buffer->setArgStateful(&surfaceState, false, false, false, false, context.getDevice(0)->getDevice(), false);

    const auto expectedMocs = context.getDevice(0)->getGmmHelper()->getL1EnabledMOCS();
    const auto actualMocs = surfaceState.getMemoryObjectControlState();
    EXPECT_EQ(expectedMocs, actualMocs);

    auto actualL1CachePolicy = static_cast<uint8_t>(surfaceState.getL1CacheControlCachePolicy());

    EXPECT_EQ(pDevice->getProductHelper().getL1CachePolicy(false), actualL1CachePolicy);

    alignedFree(ptr);
}

XE3P_CORETEST_F(CmdsProgrammingTestsXe3pCore, givenHeaplessAndOffsetCrossThreadDataWhenSendCrossThreadDataThenIndirectDataAddressIsCorrect) {

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    MockKernelWithInternals kernel(*pClDevice);
    MockContext context(pClDevice);
    MockCommandQueue commandQueue(&context, pClDevice, nullptr, false);
    commandQueue.heaplessModeEnabled = true;

    auto &indirectHeap = commandQueue.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
    indirectHeap.getSpace(1024u); // create offset for cross thread data
    auto offsetCrossThreadData = indirectHeap.getUsed();

    auto sizeCrossThreadData = kernel.mockKernel->getCrossThreadDataSize();
    kernel.kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.offset = 0;
    kernel.kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.pointerSize = 0x8;

    DefaultWalkerType cmd;
    HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
        indirectHeap,
        *kernel.mockKernel,
        true,
        &cmd,
        sizeCrossThreadData,
        0,
        pClDevice->getRootDeviceEnvironment());

    auto *inlineData = reinterpret_cast<uint64_t *>(cmd.getInlineDataPointer());

    EXPECT_NE(0ull, offsetCrossThreadData);
    auto expectedIndirectDataAddress = indirectHeap.getHeapGpuBase() +
                                       indirectHeap.getHeapGpuStartOffset() +
                                       offsetCrossThreadData;

    EXPECT_EQ(0, memcmp(&expectedIndirectDataAddress, inlineData, sizeof(uint64_t)));
}

XE3P_CORETEST_F(CmdsProgrammingTestsXe3pCore, givenHeaplessAndPartOfIndirectDataPointerAddressUndefinedWhenSendCrossThreadDataThenInlineDataIsNotProgrammed) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    MockKernelWithInternals kernel(*pClDevice);
    MockContext context(pClDevice);
    MockCommandQueue commandQueue(&context, pClDevice, nullptr, false);
    commandQueue.heaplessModeEnabled = true;

    auto &indirectHeap = commandQueue.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);

    auto sizeCrossThreadData = kernel.mockKernel->getCrossThreadDataSize();
    std::vector<uint8_t> crossThreadData(sizeCrossThreadData, 0xAB);
    kernel.mockKernel->setCrossThreadData(crossThreadData.data(), sizeCrossThreadData);

    struct TestParam {
        InlineDataOffset offset;
        uint8_t pointerSize;
    };

    std::vector<TestParam> testParams = {
        {undefined<InlineDataOffset>, 0u},
        {0u, undefined<uint8_t>}};

    DefaultWalkerType cmd;

    for (const auto &testParam : testParams) {
        kernel.kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.offset = testParam.offset;
        kernel.kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.pointerSize = testParam.pointerSize;

        HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
            indirectHeap,
            *kernel.mockKernel,
            true,
            &cmd,
            sizeCrossThreadData,
            0,
            pClDevice->getRootDeviceEnvironment());

        auto *inlineData = reinterpret_cast<uint64_t *>(cmd.getInlineDataPointer());

        // Verify cross-thread data is preserved (no patching occurred)
        EXPECT_EQ(0, memcmp(crossThreadData.data(), inlineData, cmd.getInlineDataSize()));
    }
}

XE3P_CORETEST_F(CmdsProgrammingTestsXe3pCore, givenHeaplessAndScratchPointerAddressWhenSendCrossThreadDataThenScratchPointerIsCorrect) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    MockKernelWithInternals kernel(*pClDevice);
    MockContext context(pClDevice);
    MockCommandQueue commandQueue(&context, pClDevice, nullptr, false);
    commandQueue.heaplessModeEnabled = true;

    auto &indirectHeap = commandQueue.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);

    uint64_t expectedScratchPointer = 0x1234u;

    auto sizeCrossThreadData = kernel.mockKernel->getCrossThreadDataSize();
    kernel.kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.offset = 0x0;
    kernel.kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.pointerSize = sizeof(expectedScratchPointer);

    DefaultWalkerType cmd;
    HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
        indirectHeap,
        *kernel.mockKernel,
        true,
        &cmd,
        sizeCrossThreadData,
        expectedScratchPointer,
        pClDevice->getRootDeviceEnvironment());

    auto *inlineData = reinterpret_cast<uint64_t *>(cmd.getInlineDataPointer());

    EXPECT_EQ(0, memcmp(&expectedScratchPointer, inlineData, sizeof(expectedScratchPointer)));
}

XE3P_CORETEST_F(CmdsProgrammingTestsXe3pCore, givenHeaplessAndPartOfScratchPointerAddressUndefinedWhenSendCrossThreadDataThenInlineDataIsNotProgrammed) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    MockKernelWithInternals kernel(*pClDevice);
    MockContext context(pClDevice);
    MockCommandQueue commandQueue(&context, pClDevice, nullptr, false);
    commandQueue.heaplessModeEnabled = true;

    auto &indirectHeap = commandQueue.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);

    auto sizeCrossThreadData = kernel.mockKernel->getCrossThreadDataSize();
    std::vector<uint8_t> crossThreadData(sizeCrossThreadData, 0xAB);
    kernel.mockKernel->setCrossThreadData(crossThreadData.data(), sizeCrossThreadData);

    uint64_t scratchPointer = 0x1234u;
    DefaultWalkerType cmd;

    struct TestParam {
        uint8_t pointerSize;
        InlineDataOffset offset;
    };

    std::vector<TestParam> testParams = {
        {undefined<uint8_t>, 0u},
        {0x8u, undefined<InlineDataOffset>}};

    for (const auto &testParam : testParams) {
        kernel.kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.pointerSize = testParam.pointerSize;
        kernel.kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.scratchPointerAddress.offset = testParam.offset;

        HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
            indirectHeap,
            *kernel.mockKernel,
            true,
            &cmd,
            sizeCrossThreadData,
            scratchPointer,
            pClDevice->getRootDeviceEnvironment());

        auto *inlineData = reinterpret_cast<uint64_t *>(cmd.getInlineDataPointer());

        EXPECT_EQ(0, memcmp(crossThreadData.data(), inlineData, cmd.getInlineDataSize()));
    }
}

XE3P_CORETEST_F(CmdsProgrammingTestsXe3pCore, givenHeaplessAndOffsetCrossThreadDataAndImplicitArgsWhenSendCrossThreadDataThenIndirectDataAddressIsCorrect) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    MockKernelWithInternals kernel(*pClDevice);
    MockContext context(pClDevice);
    MockCommandQueue commandQueue(&context, pClDevice, nullptr, false);
    commandQueue.heaplessModeEnabled = true;

    auto &indirectHeap = commandQueue.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
    indirectHeap.getSpace(1024u); // create offset for cross thread data
    auto offsetCrossThreadData = indirectHeap.getUsed();

    auto sizeCrossThreadData = kernel.mockKernel->getCrossThreadDataSize();
    kernel.kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.offset = 0;
    kernel.kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.pointerSize = 0x8;

    kernel.mockKernel->pImplicitArgs = std::make_unique<NEO::ImplicitArgs>();
    memset(kernel.mockKernel->pImplicitArgs.get(), 0, NEO::ImplicitArgsV1::getSize());
    kernel.mockKernel->pImplicitArgs->v1.header.structVersion = 0;
    kernel.mockKernel->pImplicitArgs->v1.header.structSize = NEO::ImplicitArgsV1::getSize();
    kernel.mockKernel->pImplicitArgs->setLocalSize(1, 1, 1);

    indirectHeap.align(FamilyType::cacheLineSize);
    auto expectCrossThreadDataOffset = indirectHeap.getUsed();
    DefaultWalkerType cmd;
    HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
        indirectHeap,
        *kernel.mockKernel,
        true,
        &cmd,
        sizeCrossThreadData,
        0,
        pClDevice->getRootDeviceEnvironment());

    auto *inlineData = reinterpret_cast<uint64_t *>(cmd.getInlineDataPointer());

    EXPECT_EQ(expectCrossThreadDataOffset, offsetCrossThreadData);
    auto expectedIndirectDataAddress = indirectHeap.getHeapGpuBase() +
                                       indirectHeap.getHeapGpuStartOffset() +
                                       offsetCrossThreadData;

    EXPECT_EQ(0, memcmp(&expectedIndirectDataAddress, inlineData, sizeof(uint64_t)));
}

XE3P_CORETEST_F(CmdsProgrammingTestsXe3pCore, givenHeaplessAndImplicitArgsAndScratchAddressWhenSendCrossThreadDataThenScratchIsPatchedInImplicitArgs) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    MockKernelWithInternals kernel(*pClDevice);
    MockContext context(pClDevice);
    MockCommandQueue commandQueue(&context, pClDevice, nullptr, false);
    commandQueue.heaplessModeEnabled = true;

    auto &indirectHeap = commandQueue.getIndirectHeap(IndirectHeap::Type::indirectObject, 8192);
    indirectHeap.getSpace(1024u);

    auto sizeCrossThreadData = kernel.mockKernel->getCrossThreadDataSize();
    kernel.kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.offset = 0;
    kernel.kernelInfo.kernelDescriptor.payloadMappings.implicitArgs.indirectDataPointerAddress.pointerSize = 0x8;

    kernel.mockKernel->pImplicitArgs = std::make_unique<NEO::ImplicitArgs>();
    memset(kernel.mockKernel->pImplicitArgs.get(), 0, NEO::ImplicitArgsV1::getSize());

    kernel.mockKernel->pImplicitArgs->v1.header.structVersion = 1;
    kernel.mockKernel->pImplicitArgs->v1.header.structSize = NEO::ImplicitArgsV1::getSize();
    kernel.mockKernel->pImplicitArgs->setLocalSize(1, 1, 1);

    uint64_t expectedScratchAddress = 0x1234u;

    DefaultWalkerType cmd;
    HardwareCommandsHelper<FamilyType>::template sendCrossThreadData<DefaultWalkerType>(
        indirectHeap,
        *kernel.mockKernel,
        true,
        &cmd,
        sizeCrossThreadData,
        expectedScratchAddress,
        pClDevice->getRootDeviceEnvironment());

    auto scratchAddressProgrammedInImplicitArgs = kernel.mockKernel->pImplicitArgs.get()->v1.scratchPtr;

    EXPECT_EQ(expectedScratchAddress, scratchAddressProgrammedInImplicitArgs);
}
