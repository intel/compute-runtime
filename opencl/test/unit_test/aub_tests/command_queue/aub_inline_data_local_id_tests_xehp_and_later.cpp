/*
 * Copyright (C) 2022-2025 Intel Corporation
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
#include "opencl/test/unit_test/command_queue/command_queue_fixture.h"
#include "opencl/test/unit_test/fixtures/buffer_fixture.h"
#include "opencl/test/unit_test/fixtures/simple_arg_kernel_fixture.h"
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
    TestVariables variables[5] = {};
    size_t variablesCount;

    HardwareParse hwParser;
};

struct InlineDataFixture : AubDispatchThreadDataFixture {
    void setUp() {
        debugRestorer = std::make_unique<DebugManagerStateRestore>();
        debugManager.flags.EnablePassInlineData.set(true);

        initializeKernel3Variables();
        initializeKernel4Variables();

        AubDispatchThreadDataFixture::setUp();

        setUpKernel3();
    }

    void initializeKernel4Variables() {
        kernelIds |= (1 << 4);
        variables[4].gwsSize = 1;
        variables[4].lwsSize = 1;
    }

    void initializeKernel3Variables() {
        kernelIds |= (1 << 3);
        variables[3].sizeUserMemory = 4096;
        variables[3].typeSize = sizeof(unsigned int);
        variables[3].gwsSize = 128;
        variables[3].lwsSize = 32;
    }

    void setUpKernel3() {
        memset(variables[3].destMemory, 0xFE, variables[3].sizeUserMemory);

        kernels[3]->setArg(0, variables[3].destBuffer);

        variables[3].sizeWrittenMemory = variables[3].gwsSize * variables[3].typeSize;
        variables[3].expectedMemory = alignedMalloc(variables[3].sizeWrittenMemory, 4096);
        memset(variables[3].expectedMemory, 0, variables[3].sizeWrittenMemory);
        variables[3].sizeRemainderMemory = variables[3].sizeUserMemory - variables[3].sizeWrittenMemory;
        variables[3].expectedRemainderMemory = alignedMalloc(variables[3].sizeRemainderMemory, 4096);
        memcpy_s(variables[3].expectedRemainderMemory,
                 variables[3].sizeRemainderMemory,
                 variables[3].destMemory,
                 variables[3].sizeRemainderMemory);

        variables[3].remainderDestMemory = static_cast<char *>(variables[3].destMemory) + variables[3].sizeWrittenMemory;
    }
};

using XeHPAndLaterAubInlineDataTest = Test<InlineDataFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubInlineDataTest, givenCrossThreadFitIntoSingleGrfWhenInlineDataAllowedThenCopyAllCrossThreadIntoInline) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto *kernel = kernels[3].get();

    if (!EncodeDispatchKernel<FamilyType>::inlineDataProgrammingRequired(kernel->getKernelInfo().kernelDescriptor)) {
        return;
    }

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {variables[4].gwsSize, 1, 1};
    size_t localWorkSize[3] = {variables[4].lwsSize, 1, 1};
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

    auto *kernel = this->kernels[3].get();
    if (!EncodeDispatchKernel<FamilyType>::inlineDataProgrammingRequired(kernel->getKernelInfo().kernelDescriptor)) {
        return;
    }

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {variables[3].gwsSize, 1, 1};
    size_t localWorkSize[3] = {variables[3].lwsSize, 1, 1};
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

    expectMemory<FamilyType>(variables[3].destMemory, variables[3].expectedMemory, variables[3].sizeWrittenMemory);
    expectMemory<FamilyType>(variables[3].remainderDestMemory, variables[3].expectedRemainderMemory, variables[3].sizeRemainderMemory);
}

struct HwLocalIdsFixture : AubDispatchThreadDataFixture {
    void setUp() {
        debugRestorer = std::make_unique<DebugManagerStateRestore>();
        debugManager.flags.EnableHwGenerationLocalIds.set(1);

        initializeKernel2Variables();

        AubDispatchThreadDataFixture::setUp();

        if (kernels[2]->getKernelInfo().kernelDescriptor.kernelAttributes.flags.passInlineData) {
            debugManager.flags.EnablePassInlineData.set(true);
        }

        setUpKernel2();
    }

    void initializeKernel2Variables() {
        kernelIds |= (1 << 2);
        variables[2].sizeUserMemory = 4096;
        variables[2].scalarArg = 0xAA;
        variables[2].typeSize = sizeof(unsigned int);
        variables[2].gwsSize = 256;
        variables[2].lwsSize = 32;
    }

    void setUpKernel2() {
        memset(variables[2].destMemory, 0xFE, variables[2].sizeUserMemory);

        kernels[2]->setArg(0, sizeof(variables[2].scalarArg), &variables[2].scalarArg);
        kernels[2]->setArg(1, variables[2].destBuffer);

        variables[2].sizeWrittenMemory = variables[2].gwsSize * variables[2].typeSize;
        variables[2].expectedMemory = alignedMalloc(variables[2].sizeWrittenMemory, 4096);
        unsigned int *expectedData = static_cast<unsigned int *>(variables[2].expectedMemory);
        for (size_t i = 0; i < variables[2].gwsSize; i++) {
            *(expectedData + i) = variables[2].scalarArg;
        }
        variables[2].sizeRemainderMemory = variables[2].sizeUserMemory - variables[2].sizeWrittenMemory;
        variables[2].expectedRemainderMemory = alignedMalloc(variables[2].sizeRemainderMemory, 4096);
        memcpy_s(variables[2].expectedRemainderMemory,
                 variables[2].sizeRemainderMemory,
                 variables[2].destMemory,
                 variables[2].sizeRemainderMemory);

        variables[2].remainderDestMemory = static_cast<char *>(variables[2].destMemory) + variables[2].sizeWrittenMemory;
    }
};

using XeHPAndLaterAubHwLocalIdsTest = Test<HwLocalIdsFixture>;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubHwLocalIdsTest, WhenEnqueueDimensionsArePow2ThenSetEmitLocalIdsAndGenerateLocalIdsFields) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    cl_uint workDim = 1;
    size_t globalWorkOffset[3] = {0, 0, 0};
    size_t globalWorkSize[3] = {variables[2].gwsSize, 1, 1};
    size_t localWorkSize[3] = {variables[2].lwsSize, 1, 1};
    cl_uint numEventsInWaitList = 0;
    cl_event *eventWaitList = nullptr;
    cl_event *event = nullptr;

    auto *kernel = this->kernels[2].get();

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
    auto kernelAllocationGpuAddr = isHeapless ? kernel->getKernelInfo().kernelAllocation->getGpuAddress()
                                              : kernel->getKernelInfo().kernelAllocation->getGpuAddressToPatch();
    auto skipOffset = kernel->getKernelInfo().kernelDescriptor.entryPoints.skipPerThreadDataLoad;
    uint64_t kernelStartPointer = kernelAllocationGpuAddr + skipOffset;

    auto &idd = walker->getInterfaceDescriptor();

    using KernelStartPointerType = decltype(idd.getKernelStartPointer());
    EXPECT_EQ(static_cast<KernelStartPointerType>(kernelStartPointer), idd.getKernelStartPointer());

    pCmdQ->flush();

    expectMemory<FamilyType>(variables[2].destMemory, variables[2].expectedMemory, variables[2].sizeWrittenMemory);
    expectMemory<FamilyType>(variables[2].remainderDestMemory, variables[2].expectedRemainderMemory, variables[2].sizeRemainderMemory);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubHwLocalIdsTest, givenNonPowOf2LocalWorkSizeButCompatibleWorkOrderWhenLocalIdsAreUsedThenDataVerifiesCorrectly) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    cl_uint workDim = 1;
    size_t globalWorkSize[3] = {200, 1, 1};
    size_t localWorkSize[3] = {200, 1, 1};

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
    if (kernel->getKernelInfo().kernelDescriptor.kernelAttributes.flags.requiresWorkgroupWalkOrder) {
        EXPECT_EQ(0u, walker->getGenerateLocalId());
    } else {

        EXPECT_EQ(expectedEmitLocal, walker->getEmitLocalId());
        EXPECT_EQ(1u, walker->getGenerateLocalId());
        EXPECT_EQ(0u, walker->getWalkOrder());
    }

    pCmdQ->flush();

    expectMemory<FamilyType>(variables[2].destMemory, variables[2].expectedMemory, globalWorkSize[0] * variables[2].typeSize);
}

struct HwLocalIdsWithSubGroups : AubDispatchThreadDataFixture {
    void setUp() {
        debugRestorer = std::make_unique<DebugManagerStateRestore>();
        debugManager.flags.EnableHwGenerationLocalIds.set(1);

        kernelIds |= (1 << 9);
        variables[0].sizeUserMemory = 16 * MemoryConstants::kiloByte;
        AubDispatchThreadDataFixture::setUp();

        memset(variables[0].destMemory, 0, variables[0].sizeUserMemory);
        variables[0].expectedMemory = alignedMalloc(variables[0].sizeUserMemory, 4096);
        kernels[9]->setArg(0, variables[0].destBuffer);
    }
};

using XeHPAndLaterAubHwLocalIdsWithSubgroupsTest = Test<HwLocalIdsWithSubGroups>;
HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubHwLocalIdsWithSubgroupsTest, givenKernelUsingSubgroupsWhenLocalIdsAreGeneratedByHwThenValuesAreCorrect) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    cl_uint workDim = 1;
    size_t globalWorkSize[3] = {256, 1, 1};
    size_t localWorkSize[3] = {256, 1, 1};

    auto *kernel = this->kernels[9].get();

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
    for (size_t i = 0; i < 3; i++) {
        EXPECT_EQ(kernel->getKernelInfo().kernelDescriptor.kernelAttributes.workgroupWalkOrder[i], HwWalkOrderHelper::compatibleDimensionOrders[walker->getWalkOrder()][i]);
    }

    pCmdQ->finish(false);

    // we expect sequence of local ids from 0..256
    auto expectedMemory = reinterpret_cast<uint32_t *>(variables[0].expectedMemory);
    auto currentWorkItem = 0u;

    while (currentWorkItem < localWorkSize[0]) {
        expectedMemory[0] = currentWorkItem++;
        expectedMemory++;
    }

    expectMemory<FamilyType>(variables[0].destMemory, variables[0].expectedMemory, ptrDiff(expectedMemory, variables[0].expectedMemory));
}
