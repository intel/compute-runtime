/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/unit_test_helper.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/test/aub_tests/memory/memory_aub_fixture.h"

#include <algorithm>
#include <limits>
#include <tuple>

namespace L0 {
namespace ult {

using AUBMemoryFillL0 = MemoryAubParamTest<size_t>;

HWTEST_P(AUBMemoryFillL0, givenOffsetWhenFillingDeviceAllocationThenPatternIsWrittenAndNeighbourMemoryIsUntouched) {
    constexpr size_t elementCount = 8;
    const float pattern = 1.0f;
    const size_t patternSize = sizeof(pattern);
    const size_t bufferSize = elementCount * patternSize;
    const size_t patternCount = 2;
    const size_t offset = GetParam();

    std::vector<float> reference(elementCount, 0.0f);
    auto dstBuffer = this->allocate(AubMemoryType::deviceUsm, bufferSize);
    this->initialize(AubMemoryType::deviceUsm, dstBuffer, reference.data(), bufferSize);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryFill(this->commandList->toHandle(), ptrOffset(dstBuffer, offset), &pattern, patternSize, patternCount * patternSize, nullptr, 0, nullptr));
    this->executeAndSynchronize();

    std::fill_n(reference.begin() + offset / patternSize, patternCount, pattern);
    this->template expectMemory<FamilyType>(dstBuffer, reference.data(), bufferSize);
}

INSTANTIATE_TEST_SUITE_P(AUBMemoryFillL0Offsets,
                         AUBMemoryFillL0,
                         ::testing::Values(0 * sizeof(float), 1 * sizeof(float), 2 * sizeof(float), 3 * sizeof(float)));

using AUBMemoryFillConcurrentL0 = Test<MemoryAubFixture>;

HWTEST_F(AUBMemoryFillConcurrentL0, givenFillsOnSeveralCommandListsWhenExecutedWithoutSynchronizationBetweenThenAllPatternsAreWritten) {
    constexpr uint32_t numWrites = 4;
    const float pattern = 1.0f;
    const size_t patternSize = sizeof(pattern);
    const size_t size = 2 * patternSize;
    const size_t bufferSize = numWrites * size;
    const std::vector<uint8_t> zeroMemory(bufferSize, 0);

    auto dstBuffer = this->allocate(AubMemoryType::deviceUsm, bufferSize);
    this->initialize(AubMemoryType::deviceUsm, dstBuffer, zeroMemory.data(), bufferSize);
    this->executeAndSynchronize();

    ze_command_list_desc_t commandListDesc = {ZE_STRUCTURE_TYPE_COMMAND_LIST_DESC};
    std::vector<ze_command_list_handle_t> commandLists(numWrites, nullptr);
    for (uint32_t id = 0; id < numWrites; id++) {
        ASSERT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreate(this->context->toHandle(), this->device->toHandle(), &commandListDesc, &commandLists[id]));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryFill(commandLists[id], ptrOffset(dstBuffer, id * size), &pattern, patternSize, size, nullptr, 0, nullptr));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListClose(commandLists[id]));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandQueueExecuteCommandLists(this->pCmdq->toHandle(), 1, &commandLists[id], nullptr));
    }

    this->csr->pollForCompletion();

    const std::vector<float> reference(bufferSize / patternSize, pattern);
    this->template expectMemory<FamilyType>(dstBuffer, reference.data(), bufferSize);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandQueueSynchronize(this->pCmdq->toHandle(), std::numeric_limits<uint64_t>::max()));
    for (auto commandListHandle : commandLists) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListDestroy(commandListHandle));
    }
}

using AUBVerifyMemoryL0 = MemoryAubParamTest<std::tuple<size_t, bool>>;

HWTEST_P(AUBVerifyMemoryL0, givenDeviceAllocationCopiedToHostWhenComparingWithValidAndInvalidContentThenExpectationsMatch) {
    const uint32_t testItem = 5;
    const uint32_t testItemWrong1 = 4;
    const uint32_t testItemWrong2 = 6;
    const size_t testItemSize = sizeof(testItem);
    const auto [testDataSize, initializeByCopy] = GetParam();

    std::vector<uint32_t> validContent(testDataSize / testItemSize, testItem);
    std::vector<uint32_t> invalidContent1(validContent);
    std::vector<uint32_t> invalidContent2(validContent);
    invalidContent1.back() = testItemWrong1;
    invalidContent2.back() = testItemWrong2;

    auto buffer = this->allocate(AubMemoryType::deviceUsm, testDataSize);
    if (initializeByCopy) {
        this->initialize(AubMemoryType::deviceUsm, buffer, validContent.data(), testDataSize);
    } else {
        EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListAppendMemoryFill(this->commandList->toHandle(), buffer, &testItem, testItemSize, testDataSize, nullptr, 0, nullptr));
        this->appendBarrier();
    }

    auto hostBuffer = this->allocate(AubMemoryType::hostUsm, testDataSize);
    this->appendCopy(hostBuffer, buffer, testDataSize);
    this->executeAndSynchronize();

    this->template expectMemory<FamilyType>(hostBuffer, validContent.data(), testDataSize);
    if (NEO::UnitTestHelper<FamilyType>::isExpectMemoryNotEqualSupported()) {
        this->template expectNotEqualMemory<FamilyType>(hostBuffer, invalidContent1.data(), testDataSize);
        this->template expectNotEqualMemory<FamilyType>(hostBuffer, invalidContent2.data(), testDataSize);
    }
}

INSTANTIATE_TEST_SUITE_P(SizesAndInit,
                         AUBVerifyMemoryL0,
                         ::testing::Combine(
                             ::testing::Values(size_t{16}, MemoryConstants::megaByte),
                             ::testing::Bool()));

} // namespace ult
} // namespace L0
