/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/hw_walk_order.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/aub_tests/kernel/kernel_aub_fixture.h"

#include "implicit_args.h"

#include <algorithm>
#include <vector>

namespace L0 {
namespace ult {

struct AubDispatchThreadDataFixtureL0 : public KernelAubFixtureL0 {
    template <typename FamilyType>
    typename FamilyType::DefaultWalkerType *parseWalker() {
        this->hwParser.parseCommands<FamilyType>(this->getCommandListStream(), 0);
        this->hwParser.findHardwareCommands<FamilyType>();
        if (this->hwParser.itorWalker == this->hwParser.cmdList.end()) {
            return nullptr;
        }
        return genCmdCast<typename FamilyType::DefaultWalkerType *>(*this->hwParser.itorWalker);
    }

    void *createDestinationBuffer(size_t size, uint8_t pattern) {
        auto buffer = this->allocateHostMemory(size, MemoryConstants::pageSize);
        if (buffer != nullptr) {
            memset(buffer, pattern, size);
        }
        return buffer;
    }

    static std::vector<uint32_t> getExpectedMemory(size_t size, uint8_t initialPattern, uint32_t writtenCount, uint32_t writtenValue) {
        uint32_t initialValue = 0;
        memset(&initialValue, initialPattern, sizeof(initialValue));
        std::vector<uint32_t> expectedMemory(size / sizeof(uint32_t), initialValue);
        std::fill_n(expectedMemory.begin(), writtenCount, writtenValue);
        return expectedMemory;
    }

    NEO::HardwareParse hwParser;
};

struct InlineDataFixtureL0 : public AubDispatchThreadDataFixtureL0 {
    void setUp() {
        AubDispatchThreadDataFixtureL0::setUp();

        this->kernel = this->createKernel("simple_kernels", "simple_kernel_1", true);
        ASSERT_NE(nullptr, this->kernel);

        this->destMemory = this->createDestinationBuffer(this->sizeUserMemory, this->initialPattern);
        ASSERT_NE(nullptr, this->destMemory);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 0, sizeof(void *), &this->destMemory));
    }

    ze_kernel_handle_t kernel = nullptr;
    void *destMemory = nullptr;
    const size_t sizeUserMemory = 4096;
    const uint8_t initialPattern = 0xFE;
    const uint32_t gwsSize = 128;
    const uint32_t lwsSize = 32;
};

using XeHPAndLaterAubInlineDataTestL0 = Test<InlineDataFixtureL0>;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubInlineDataTestL0, givenCrossThreadFitIntoSingleGrfWhenInlineDataAllowedThenCopyAllCrossThreadIntoInline) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto l0Kernel = toKernel(this->kernel);
    if (!NEO::EncodeDispatchKernel<FamilyType>::inlineDataProgrammingRequired(l0Kernel->getKernelDescriptor())) {
        GTEST_SKIP();
    }

    this->writeHostMemory<FamilyType>(this->destMemory);

    const uint32_t groupSize[3] = {1, 1, 1};
    const uint32_t groupCount[3] = {1, 1, 1};
    this->appendLaunch(this->kernel, groupSize, groupCount);
    this->executeAndSynchronize();

    auto walker = this->parseWalker<FamilyType>();
    ASSERT_NE(nullptr, walker);

    EXPECT_EQ(1u, walker->getEmitInlineParameter());
    EXPECT_EQ(getExpectedEmitLocalId(l0Kernel->getKernelDescriptor()), walker->getEmitLocalId());

    constexpr auto inlineSize = WalkerType::getInlineDataSize();
    constexpr bool isHeapless = FamilyType::template isHeaplessMode<WalkerType>();
    constexpr auto offsetInBytes = isHeapless ? 16u : 0u;
    auto inlineDataPointer = ptrOffset(reinterpret_cast<const uint8_t *>(walker->getInlineDataPointer()), offsetInBytes);
    auto crossThreadData = ptrOffset(l0Kernel->getCrossThreadData(), offsetInBytes);
    const uint32_t crossThreadDataSize = l0Kernel->getCrossThreadDataSize();
    const auto sizeToCompare = std::min(inlineSize - offsetInBytes, crossThreadDataSize - offsetInBytes);

    EXPECT_EQ(0, memcmp(inlineDataPointer, crossThreadData, sizeToCompare));

    const uint32_t expectedValue = 0;
    this->expectMemory<FamilyType>(this->getGpuPointer(this->destMemory), &expectedValue, sizeof(expectedValue));
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubInlineDataTestL0, givenCrossThreadSizeMoreThanSingleGrfWhenInlineDataAllowedThenCopyGrfCrossThreadToInline) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    auto l0Kernel = toKernel(this->kernel);
    if (!NEO::EncodeDispatchKernel<FamilyType>::inlineDataProgrammingRequired(l0Kernel->getKernelDescriptor())) {
        GTEST_SKIP();
    }

    this->writeHostMemory<FamilyType>(this->destMemory);

    auto ioh = this->commandList->getCmdContainer().getIndirectHeap(NEO::HeapType::indirectObject);
    ASSERT_NE(nullptr, ioh);
    const auto iohUsedBefore = ioh->getUsed();

    const uint32_t groupSize[3] = {this->lwsSize, 1, 1};
    const uint32_t groupCount[3] = {this->gwsSize / this->lwsSize, 1, 1};
    this->appendLaunch(this->kernel, groupSize, groupCount);
    this->executeAndSynchronize();

    auto walker = this->parseWalker<FamilyType>();
    ASSERT_NE(nullptr, walker);

    EXPECT_EQ(1u, walker->getEmitInlineParameter());
    EXPECT_EQ(getExpectedEmitLocalId(l0Kernel->getKernelDescriptor()), walker->getEmitLocalId());

    constexpr auto inlineSize = WalkerType::getInlineDataSize();
    constexpr bool isHeapless = FamilyType::template isHeaplessMode<WalkerType>();
    constexpr auto offsetInBytes = isHeapless ? 16u : 0u;
    auto inlineDataPointer = ptrOffset(reinterpret_cast<const uint8_t *>(walker->getInlineDataPointer()), offsetInBytes);
    auto crossThreadData = ptrOffset(l0Kernel->getCrossThreadData(), offsetInBytes);
    size_t crossThreadDataSize = l0Kernel->getCrossThreadDataSize();

    EXPECT_EQ(0, memcmp(inlineDataPointer, crossThreadData, std::min(static_cast<size_t>(inlineSize), crossThreadDataSize) - offsetInBytes));

    if (inlineSize < crossThreadDataSize) {
        crossThreadDataSize -= inlineSize;
        crossThreadData = ptrOffset(crossThreadData, inlineSize - offsetInBytes);

        const void *payloadData = alignUp(ptrOffset(ioh->getCpuBase(), iohUsedBefore), FamilyType::cacheLineSize);
        auto pImplicitArgs = l0Kernel->getImplicitArgs();
        if (pImplicitArgs) {
            payloadData = ptrOffset(payloadData, pImplicitArgs->getAlignedSize());
        }
        EXPECT_EQ(0, memcmp(payloadData, crossThreadData, crossThreadDataSize));
    }

    auto expectedMemory = getExpectedMemory(this->sizeUserMemory, this->initialPattern, this->gwsSize, 0u);
    this->expectMemory<FamilyType>(this->getGpuPointer(this->destMemory), expectedMemory.data(), this->sizeUserMemory);
}

struct HwLocalIdsFixtureL0 : public AubDispatchThreadDataFixtureL0 {
    void setUp() {
        NEO::debugManager.flags.EnableHwGenerationLocalIds.set(1);
        AubDispatchThreadDataFixtureL0::setUp();

        this->kernel = this->createKernel("simple_kernels", "simple_kernel_0", true);
        ASSERT_NE(nullptr, this->kernel);

        this->destMemory = this->createDestinationBuffer(this->sizeUserMemory, this->initialPattern);
        ASSERT_NE(nullptr, this->destMemory);

        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 0, sizeof(this->scalarArg), &this->scalarArg));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 1, sizeof(void *), &this->destMemory));

        this->expectedMemory = getExpectedMemory(this->sizeUserMemory, this->initialPattern, this->gwsSize, this->scalarArg);
    }

    ze_kernel_handle_t kernel = nullptr;
    void *destMemory = nullptr;
    std::vector<uint32_t> expectedMemory;
    const size_t sizeUserMemory = 4096;
    const uint8_t initialPattern = 0xFE;
    const uint32_t scalarArg = 0xAA;
    const uint32_t gwsSize = 256;
    const uint32_t lwsSize = 32;
};

using XeHPAndLaterAubHwLocalIdsTestL0 = Test<HwLocalIdsFixtureL0>;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubHwLocalIdsTestL0, WhenEnqueueDimensionsArePow2ThenSetEmitLocalIdsAndGenerateLocalIdsFields) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    this->writeHostMemory<FamilyType>(this->destMemory);

    const uint32_t groupSize[3] = {this->lwsSize, 1, 1};
    const uint32_t groupCount[3] = {this->gwsSize / this->lwsSize, 1, 1};
    this->appendLaunch(this->kernel, groupSize, groupCount);

    auto walker = this->parseWalker<FamilyType>();
    ASSERT_NE(nullptr, walker);

    auto l0Kernel = toKernel(this->kernel);
    const auto &kernelDescriptor = l0Kernel->getKernelDescriptor();
    EXPECT_EQ(getExpectedEmitLocalId(kernelDescriptor), walker->getEmitLocalId());
    EXPECT_EQ(1u, walker->getGenerateLocalId());

    constexpr bool isHeapless = FamilyType::template isHeaplessMode<WalkerType>();
    auto isaAllocation = l0Kernel->getIsaAllocation();
    auto kernelAllocationGpuAddr = (isHeapless ? isaAllocation->getGpuAddress() : isaAllocation->getGpuAddressToPatch()) + l0Kernel->getIsaOffsetInParentAllocation();
    uint64_t kernelStartPointer = kernelAllocationGpuAddr + kernelDescriptor.entryPoints.skipPerThreadDataLoad;

    auto &idd = walker->getInterfaceDescriptor();
    using KernelStartPointerType = decltype(idd.getKernelStartPointer());
    EXPECT_EQ(static_cast<KernelStartPointerType>(kernelStartPointer), idd.getKernelStartPointer());

    this->executeAndSynchronize();

    this->expectMemory<FamilyType>(this->getGpuPointer(this->destMemory), this->expectedMemory.data(), this->sizeUserMemory);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubHwLocalIdsTestL0, givenNonPowOf2LocalWorkSizeButCompatibleWorkOrderWhenLocalIdsAreUsedThenDataVerifiesCorrectly) {
    this->writeHostMemory<FamilyType>(this->destMemory);

    const uint32_t groupSize[3] = {200, 1, 1};
    const uint32_t groupCount[3] = {1, 1, 1};
    this->appendLaunch(this->kernel, groupSize, groupCount);

    auto walker = this->parseWalker<FamilyType>();
    ASSERT_NE(nullptr, walker);

    const auto &kernelAttributes = toKernel(this->kernel)->getKernelDescriptor().kernelAttributes;
    EXPECT_EQ(getExpectedEmitLocalId(toKernel(this->kernel)->getKernelDescriptor()), walker->getEmitLocalId());
    EXPECT_EQ(1u, walker->getGenerateLocalId());
    if (kernelAttributes.flags.requiresWorkgroupWalkOrder) {
        EXPECT_EQ(NEO::HwWalkOrderHelper::singleDimWalkIndex, walker->getWalkOrder());
    } else {
        EXPECT_EQ(0u, walker->getWalkOrder());
    }

    this->executeAndSynchronize();

    this->expectMemory<FamilyType>(this->getGpuPointer(this->destMemory), this->expectedMemory.data(), groupSize[0] * sizeof(uint32_t));
}

struct HwLocalIdsWithSubGroupsFixtureL0 : public AubDispatchThreadDataFixtureL0 {
    void setUp() {
        NEO::debugManager.flags.EnableHwGenerationLocalIds.set(1);
        AubDispatchThreadDataFixtureL0::setUp();

        this->kernel = this->createKernel("simple_kernels", "simple_kernel_2", true);
        ASSERT_NE(nullptr, this->kernel);

        this->destMemory = this->createDestinationBuffer(this->sizeUserMemory, 0);
        ASSERT_NE(nullptr, this->destMemory);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeKernelSetArgumentValue(this->kernel, 0, sizeof(void *), &this->destMemory));
    }

    ze_kernel_handle_t kernel = nullptr;
    void *destMemory = nullptr;
    const size_t sizeUserMemory = 16 * MemoryConstants::kiloByte;
};

using XeHPAndLaterAubHwLocalIdsWithSubgroupsTestL0 = Test<HwLocalIdsWithSubGroupsFixtureL0>;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterAubHwLocalIdsWithSubgroupsTestL0, givenKernelUsingSubgroupsWhenLocalIdsAreGeneratedByHwThenValuesAreCorrect) {
    this->writeHostMemory<FamilyType>(this->destMemory);

    const uint32_t groupSize[3] = {256, 1, 1};
    const uint32_t groupCount[3] = {1, 1, 1};
    this->appendLaunch(this->kernel, groupSize, groupCount);

    auto walker = this->parseWalker<FamilyType>();
    ASSERT_NE(nullptr, walker);

    const auto &kernelAttributes = toKernel(this->kernel)->getKernelDescriptor().kernelAttributes;
    EXPECT_EQ(getExpectedEmitLocalId(toKernel(this->kernel)->getKernelDescriptor()), walker->getEmitLocalId());
    EXPECT_EQ(1u, walker->getGenerateLocalId());
    EXPECT_EQ(NEO::HwWalkOrderHelper::linearWalkIndex, walker->getWalkOrder());
    if (kernelAttributes.numLocalIdChannels != 1) {
        for (size_t i = 0; i < 3; i++) {
            EXPECT_EQ(kernelAttributes.workgroupWalkOrder[i], NEO::HwWalkOrderHelper::compatibleDimensionOrders[walker->getWalkOrder()][i]);
        }
    }

    this->executeAndSynchronize();

    std::vector<uint32_t> expectedMemory(groupSize[0]);
    for (uint32_t workItem = 0; workItem < groupSize[0]; workItem++) {
        expectedMemory[workItem] = workItem;
    }
    this->expectMemory<FamilyType>(this->getGpuPointer(this->destMemory), expectedMemory.data(), expectedMemory.size() * sizeof(uint32_t));
}

} // namespace ult
} // namespace L0
