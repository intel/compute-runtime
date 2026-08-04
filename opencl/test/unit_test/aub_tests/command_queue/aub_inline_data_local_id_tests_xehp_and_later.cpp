/*
 * Copyright (C) 2022-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/hw_walk_order.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/test/unit_test/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/aub_kernel_fixture.h"
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/indirect_heap/indirect_heap_fixture.h"

using namespace NEO;

struct AubDispatchThreadDataFixture : public KernelAUBFixture<SimpleKernelFixture> {
    struct TestVariables {
        Buffer *destBuffer = nullptr;
        void *destMemory = nullptr;
        size_t sizeUserMemory = 0;
        size_t sizeWrittenMemory = 0;
        size_t sizeRemainderMemory = 0;
        void *expectedMemory = nullptr;
        void *expectedRemainderMemory = nullptr;
        char *remainderDestMemory = nullptr;
        unsigned int scalarArg = 0;
        size_t typeSize = 0;
        size_t gwsSize = 0;
        size_t lwsSize = 0;
    };
    void setUp() {
        KernelAUBFixture<SimpleKernelFixture>::setUp();
        variablesCount = arrayCount(variables);

        BufferDefaults::context = context;
        for (size_t i = 0; i < variablesCount; i++) {
            if (variables[i].sizeUserMemory) {
                variables[i].destBuffer = Buffer::create(
                    context,
                    CL_MEM_READ_WRITE | CL_MEM_FORCE_HOST_MEMORY_INTEL,
                    variables[i].sizeUserMemory,
                    nullptr,
                    retVal);
                ASSERT_NE(nullptr, variables[i].destBuffer);
                variables[i].destMemory = reinterpret_cast<void *>(variables[i].destBuffer->getCpuAddressForMapping());
            }
        }
    }

    void tearDown() {
        pCmdQ->flush();

        for (size_t i = 0; i < variablesCount; i++) {
            if (variables[i].destBuffer) {
                delete variables[i].destBuffer;
                variables[i].destBuffer = nullptr;
            }
            if (variables[i].expectedMemory) {
                alignedFree(variables[i].expectedMemory);
                variables[i].expectedMemory = nullptr;
            }
            if (variables[i].expectedRemainderMemory) {
                alignedFree(variables[i].expectedRemainderMemory);
                variables[i].expectedRemainderMemory = nullptr;
            }
        }
        BufferDefaults::context = nullptr;
        KernelAUBFixture<SimpleKernelFixture>::tearDown();
    }

    std::unique_ptr<DebugManagerStateRestore> debugRestorer;
    TestVariables variables[3] = {};
    size_t variablesCount;

    HardwareParse hwParser;
};

struct InlineDataFixture : AubDispatchThreadDataFixture {
    void setUp() {
        debugRestorer = std::make_unique<DebugManagerStateRestore>();
        debugManager.flags.EnablePassInlineData.set(true);

        initializeKernel1Variables();

        AubDispatchThreadDataFixture::setUp();

        setUpKernel1();
    }

    void initializeKernel1Variables() {
        kernelIds |= (1 << 1);
        variables[1].sizeUserMemory = 4096;
        variables[1].typeSize = sizeof(unsigned int);
        variables[1].gwsSize = 128;
        variables[1].lwsSize = 32;
    }

    void setUpKernel1() {
        memset(variables[1].destMemory, 0xFE, variables[1].sizeUserMemory);

        kernels[1]->setArg(0, variables[1].destBuffer);

        variables[1].sizeWrittenMemory = variables[1].gwsSize * variables[1].typeSize;
        variables[1].expectedMemory = alignedMalloc(variables[1].sizeWrittenMemory, 4096);
        memset(variables[1].expectedMemory, 0, variables[1].sizeWrittenMemory);
        variables[1].sizeRemainderMemory = variables[1].sizeUserMemory - variables[1].sizeWrittenMemory;
        variables[1].expectedRemainderMemory = alignedMalloc(variables[1].sizeRemainderMemory, 4096);
        memcpy_s(variables[1].expectedRemainderMemory,
                 variables[1].sizeRemainderMemory,
                 variables[1].destMemory,
                 variables[1].sizeRemainderMemory);

        variables[1].remainderDestMemory = static_cast<char *>(variables[1].destMemory) + variables[1].sizeWrittenMemory;
    }
};

using XeHPAndLaterAubInlineDataTest = Test<InlineDataFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubInlineDataTest, givenCrossThreadFitIntoSingleGrfWhenInlineDataAllowedThenCopyAllCrossThreadIntoInline) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto *kernel = kernels[1].get();

    if (!EncodeDispatchKernel<FamilyType>::inlineDataProgrammingRequired(kernel->getKernelInfo().kernelDescriptor)) {
        return;
    }

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {1, 1, 1};
    size_t localWorkSize[3] = {1, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    auto retVal = pCmdQ->enqueueKernel(
        kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    hwParser.parseCommands<FamilyType>(pCmdQ->getCS(0), 0);
    hwParser.findHardwareCommands<FamilyType>();
    EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());
    auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);

    auto localId = kernel->getKernelInfo().kernelDescriptor.kernelAttributes.localId;
    uint32_t expectedEmitLocal = 0;
    if (localId[0]) {
        expectedEmitLocal |= (1 << 0);
    }
    if (localId[1]) {
        expectedEmitLocal |= (1 << 1);
    }
    if (localId[2]) {
        expectedEmitLocal |= (1 << 2);
    }

    EXPECT_EQ(1u, walker->getEmitInlineParameter());
    EXPECT_EQ(expectedEmitLocal, walker->getEmitLocalId());

    constexpr auto inlineSize = WalkerType::getInlineDataSize();
    constexpr bool isHeapless = FamilyType::template isHeaplessMode<WalkerType>();
    constexpr auto offsetInBytes = isHeapless ? 16u : 0u;
    auto *inlineDataPointer = reinterpret_cast<uint8_t *>(walker->getInlineDataPointer());
    inlineDataPointer = ptrOffset(inlineDataPointer, offsetInBytes);

    auto *crossThreadData = reinterpret_cast<uint8_t *>(kernel->getCrossThreadData());
    crossThreadData = ptrOffset(crossThreadData, offsetInBytes);

    auto crossThreadDataSize = kernel->getCrossThreadDataSize();
    auto sizeToCompare = std::min(inlineSize - offsetInBytes, crossThreadDataSize - offsetInBytes);

    EXPECT_EQ(0, memcmp(inlineDataPointer, crossThreadData, sizeToCompare));

    // this kernel does nothing, so no expectMemory because only such kernel can fit into single GRF
    // this is for sake of testing inline data data copying by COMPUTE_WALKER
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubInlineDataTest, givenCrossThreadSizeMoreThanSingleGrfWhenInlineDataAllowedThenCopyGrfCrossThreadToInline) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto *kernel = this->kernels[1].get();
    if (!EncodeDispatchKernel<FamilyType>::inlineDataProgrammingRequired(kernel->getKernelInfo().kernelDescriptor)) {
        return;
    }

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {variables[1].gwsSize, 1, 1};
    size_t localWorkSize[3] = {variables[1].lwsSize, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    IndirectHeap &ih = pCmdQ->getIndirectHeap(IndirectHeap::Type::indirectObject, 2048);

    auto retVal = pCmdQ->enqueueKernel(
        kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    pCmdQ->flush();

    hwParser.parseCommands<FamilyType>(pCmdQ->getCS(0), 0);
    hwParser.findHardwareCommands<FamilyType>();
    EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

    auto localId = kernel->getKernelInfo().kernelDescriptor.kernelAttributes.localId;
    uint32_t expectedEmitLocal = 0;
    if (localId[0]) {
        expectedEmitLocal |= (1 << 0);
    }
    if (localId[1]) {
        expectedEmitLocal |= (1 << 1);
    }
    if (localId[2]) {
        expectedEmitLocal |= (1 << 2);
    }

    auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);
    EXPECT_EQ(1u, walker->getEmitInlineParameter());

    EXPECT_EQ(expectedEmitLocal, walker->getEmitLocalId());

    constexpr auto inlineSize = WalkerType::getInlineDataSize();
    constexpr bool isHeapless = FamilyType::template isHeaplessMode<WalkerType>();

    constexpr auto offsetInBytes = isHeapless ? 16u : 0u;
    auto *inlineDataPointer = reinterpret_cast<uint8_t *>(walker->getInlineDataPointer());
    inlineDataPointer = ptrOffset(inlineDataPointer, offsetInBytes);

    auto *crossThreadData = reinterpret_cast<uint8_t *>(kernel->getCrossThreadData());
    crossThreadData = ptrOffset(crossThreadData, offsetInBytes);
    size_t crossThreadDataSize = kernel->getCrossThreadDataSize();

    EXPECT_EQ(0, memcmp(inlineDataPointer, crossThreadData, std::min(static_cast<size_t>(inlineSize), crossThreadDataSize) - offsetInBytes));

    if (inlineSize < crossThreadDataSize) {
        crossThreadDataSize -= inlineSize;
        crossThreadData += inlineSize - offsetInBytes;

        void *payloadData = ih.getCpuBase();

        auto pImplicitArgs = kernel->getImplicitArgs();
        if (pImplicitArgs) {
            payloadData = ptrOffset(payloadData, pImplicitArgs->getAlignedSize());
        }
        EXPECT_EQ(0, memcmp(payloadData, crossThreadData, crossThreadDataSize));
    }

    expectMemory<FamilyType>(variables[1].destMemory, variables[1].expectedMemory, variables[1].sizeWrittenMemory);
    expectMemory<FamilyType>(variables[1].remainderDestMemory, variables[1].expectedRemainderMemory, variables[1].sizeRemainderMemory);
}

struct HwLocalIdsFixture : AubDispatchThreadDataFixture {
    void setUp() {
        debugRestorer = std::make_unique<DebugManagerStateRestore>();
        debugManager.flags.EnableHwGenerationLocalIds.set(1);

        initializeKernel0Variables();

        AubDispatchThreadDataFixture::setUp();

        if (kernels[0]->getKernelInfo().kernelDescriptor.kernelAttributes.flags.passInlineData) {
            debugManager.flags.EnablePassInlineData.set(true);
        }

        setUpKernel0();
    }

    void initializeKernel0Variables() {
        kernelIds |= (1 << 0);
        variables[0].sizeUserMemory = 4096;
        variables[0].scalarArg = 0xAA;
        variables[0].typeSize = sizeof(unsigned int);
        variables[0].gwsSize = 256;
        variables[0].lwsSize = 32;
    }

    void setUpKernel0() {
        memset(variables[0].destMemory, 0xFE, variables[0].sizeUserMemory);

        kernels[0]->setArg(0, sizeof(variables[0].scalarArg), &variables[0].scalarArg);
        kernels[0]->setArg(1, variables[0].destBuffer);

        variables[0].sizeWrittenMemory = variables[0].gwsSize * variables[0].typeSize;
        variables[0].expectedMemory = alignedMalloc(variables[0].sizeWrittenMemory, 4096);
        unsigned int *expectedData = static_cast<unsigned int *>(variables[0].expectedMemory);
        for (size_t i = 0; i < variables[0].gwsSize; i++) {
            *(expectedData + i) = variables[0].scalarArg;
        }
        variables[0].sizeRemainderMemory = variables[0].sizeUserMemory - variables[0].sizeWrittenMemory;
        variables[0].expectedRemainderMemory = alignedMalloc(variables[0].sizeRemainderMemory, 4096);
        memcpy_s(variables[0].expectedRemainderMemory,
                 variables[0].sizeRemainderMemory,
                 variables[0].destMemory,
                 variables[0].sizeRemainderMemory);

        variables[0].remainderDestMemory = static_cast<char *>(variables[0].destMemory) + variables[0].sizeWrittenMemory;
    }
};

using XeHPAndLaterAubHwLocalIdsTest = Test<HwLocalIdsFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubHwLocalIdsTest, WhenEnqueueDimensionsArePow2ThenSetEmitLocalIdsAndGenerateLocalIdsFields) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {variables[0].gwsSize, 1, 1};
    size_t localWorkSize[3] = {variables[0].lwsSize, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    auto *kernel = this->kernels[0].get();

    auto retVal = pCmdQ->enqueueKernel(
        kernel,
        workDim,
        globalWorkOffset,
        globalWorkSize,
        localWorkSize,
        numEventsInWaitList,
        eventWaitList,
        event);
    ASSERT_EQ(CL_SUCCESS, retVal);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(pCmdQ->getCS(0), 0);
    hwParser.findHardwareCommands<FamilyType>();
    EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

    auto localId = kernel->getKernelInfo().kernelDescriptor.kernelAttributes.localId;
    uint32_t expectedEmitLocal = 0;
    if (localId[0]) {
        expectedEmitLocal |= (1 << 0);
    }
    if (localId[1]) {
        expectedEmitLocal |= (1 << 1);
    }
    if (localId[2]) {
        expectedEmitLocal |= (1 << 2);
    }

    auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);
    EXPECT_EQ(expectedEmitLocal, walker->getEmitLocalId());
    EXPECT_EQ(1u, walker->getGenerateLocalId());

    constexpr bool isHeapless = FamilyType::template isHeaplessMode<WalkerType>();
    auto offsetInParentAllocation = kernel->getKernelInfo().getIsaOffsetInParentAllocation();
    auto kernelAllocationGpuAddr = isHeapless ? kernel->getKernelInfo().getIsaGraphicsAllocation()->getGpuAddress() + offsetInParentAllocation
                                              : kernel->getKernelInfo().getIsaGraphicsAllocation()->getGpuAddressToPatch() + offsetInParentAllocation;

    auto skipOffset = kernel->getKernelInfo().kernelDescriptor.entryPoints.skipPerThreadDataLoad;
    uint64_t kernelStartPointer = kernelAllocationGpuAddr + skipOffset;

    auto &idd = walker->getInterfaceDescriptor();

    using KernelStartPointerType = decltype(idd.getKernelStartPointer());
    EXPECT_EQ(static_cast<KernelStartPointerType>(kernelStartPointer), idd.getKernelStartPointer());

    pCmdQ->flush();

    expectMemory<FamilyType>(variables[0].destMemory, variables[0].expectedMemory, variables[0].sizeWrittenMemory);
    expectMemory<FamilyType>(variables[0].remainderDestMemory, variables[0].expectedRemainderMemory, variables[0].sizeRemainderMemory);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubHwLocalIdsTest, givenNonPowOf2LocalWorkSizeButCompatibleWorkOrderWhenLocalIdsAreUsedThenDataVerifiesCorrectly) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    cl_uint workDim = 1;
    size_t globalWorkSize[3] = {200, 1, 1};
    size_t localWorkSize[3] = {200, 1, 1};

    auto *kernel = this->kernels[0].get();

    auto retVal = pCmdQ->enqueueKernel(
        kernel,
        workDim,
        nullptr,
        globalWorkSize,
        localWorkSize,
        0,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(pCmdQ->getCS(0), 0);
    hwParser.findHardwareCommands<FamilyType>();
    EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

    auto &kernelAttributes = kernel->getKernelInfo().kernelDescriptor.kernelAttributes;

    auto localId = kernelAttributes.localId;
    uint32_t expectedEmitLocal = 0;
    if (localId[0]) {
        expectedEmitLocal |= (1 << 0);
    }
    if (localId[1]) {
        expectedEmitLocal |= (1 << 1);
    }
    if (localId[2]) {
        expectedEmitLocal |= (1 << 2);
    }

    auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);
    if (walker->getGenerateLocalId()) {
        EXPECT_EQ(expectedEmitLocal, walker->getEmitLocalId());
    }

    pCmdQ->flush();

    expectMemory<FamilyType>(variables[0].destMemory, variables[0].expectedMemory, globalWorkSize[0] * variables[0].typeSize);
}

struct HwLocalIdsWithSubGroups : AubDispatchThreadDataFixture {
    void setUp() {
        debugRestorer = std::make_unique<DebugManagerStateRestore>();
        debugManager.flags.EnableHwGenerationLocalIds.set(1);

        kernelIds |= (1 << 2);
        variables[2].sizeUserMemory = 16 * MemoryConstants::kiloByte;
        AubDispatchThreadDataFixture::setUp();

        memset(variables[2].destMemory, 0, variables[2].sizeUserMemory);
        variables[2].expectedMemory = alignedMalloc(variables[2].sizeUserMemory, 4096);
        kernels[2]->setArg(0, variables[2].destBuffer);
    }
};

using XeHPAndLaterAubHwLocalIdsWithSubgroupsTest = Test<HwLocalIdsWithSubGroups>;
HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubHwLocalIdsWithSubgroupsTest, givenKernelUsingSubgroupsWhenLocalIdsAreGeneratedByHwThenValuesAreCorrect) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    cl_uint workDim = 1;
    size_t globalWorkSize[3] = {256, 1, 1};
    size_t localWorkSize[3] = {256, 1, 1};

    auto *kernel = this->kernels[2].get();

    auto retVal = pCmdQ->enqueueKernel(
        kernel,
        workDim,
        nullptr,
        globalWorkSize,
        localWorkSize,
        0,
        nullptr,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(pCmdQ->getCS(0), 0);
    hwParser.findHardwareCommands<FamilyType>();
    EXPECT_NE(hwParser.itorWalker, hwParser.cmdList.end());

    auto &kernelAttributes = kernel->getKernelInfo().kernelDescriptor.kernelAttributes;

    auto localId = kernelAttributes.localId;
    uint32_t expectedEmitLocal = 0;
    if (localId[0]) {
        expectedEmitLocal |= (1 << 0);
    }
    if (localId[1]) {
        expectedEmitLocal |= (1 << 1);
    }
    if (localId[2]) {
        expectedEmitLocal |= (1 << 2);
    }

    auto walker = genCmdCast<WalkerType *>(*hwParser.itorWalker);
    EXPECT_EQ(expectedEmitLocal, walker->getEmitLocalId());
    EXPECT_EQ(1u, walker->getGenerateLocalId());
    EXPECT_EQ(HwWalkOrderHelper::linearWalkIndex, walker->getWalkOrder());
    if (kernelAttributes.numLocalIdChannels != 1) {
        for (size_t i = 0; i < 3; i++) {
            EXPECT_EQ(kernelAttributes.workgroupWalkOrder[i], HwWalkOrderHelper::compatibleDimensionOrders[walker->getWalkOrder()][i]);
        }
    }

    pCmdQ->finish(false);

    // we expect sequence of local ids from 0..256
    auto expectedMemory = reinterpret_cast<uint32_t *>(variables[2].expectedMemory);
    auto currentWorkItem = 0u;

    while (currentWorkItem < localWorkSize[0]) {
        expectedMemory[0] = currentWorkItem++;
        expectedMemory++;
    }

    expectMemory<FamilyType>(variables[2].destMemory, variables[2].expectedMemory, ptrDiff(expectedMemory, variables[2].expectedMemory));
}
