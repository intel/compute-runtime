/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"

namespace L0 {
namespace ult {

TEST(zeCommandListClose, whenCalledThenRedirectedToObject) {
    Mock<CommandList> commandList;
    EXPECT_CALL(commandList, close());

    auto result = zeCommandListClose(commandList.toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeCommandListAppendMemoryPrefetch, whenCalledThenRedirectedToObject) {
    Mock<CommandList> commandList;

    EXPECT_CALL(commandList, appendMemoryPrefetch(reinterpret_cast<const void *>(0x1000), 0x1000)).Times(1);

    auto res = zeCommandListAppendMemoryPrefetch(&commandList, reinterpret_cast<const void *>(0x1000), 0x1000);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
}

using zeCommandListAppendMemAdviseTest = Test<DeviceFixture>;
TEST_F(zeCommandListAppendMemAdviseTest, whenCalledThenRedirectedToObject) {
    Mock<CommandList> commandList;

    EXPECT_CALL(commandList, appendMemAdvise(device->toHandle(), reinterpret_cast<const void *>(0x1000), 0x1000, ::testing::_)).Times(1);

    auto res = zeCommandListAppendMemAdvise(&commandList, device->toHandle(), reinterpret_cast<const void *>(0x1000), 0x1000, ZE_MEMORY_ADVICE_BIAS_CACHED);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST(zeCommandListAppendMemoryCopy, whenCalledThenRedirectedToObject) {
    Mock<CommandList> commandList;

    EXPECT_CALL(commandList, appendMemoryCopy(::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_, ::testing::_)).Times(1);

    auto res = zeCommandListAppendMemoryCopy(&commandList, reinterpret_cast<void *>(0x2000), reinterpret_cast<const void *>(0x1000), 0x1000, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST(zeCommandListAppendMemoryFill, whenCalledThenRedirectedToObject) {
    Mock<CommandList> commandList;
    size_t bufferSize = 4096u;

    EXPECT_CALL(commandList, appendMemoryFill(::testing::_, ::testing::_, ::testing::_, bufferSize, ::testing::_)).Times(1);

    int value = 0;
    auto res = zeCommandListAppendMemoryFill(&commandList, reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(&value),
                                             sizeof(value), bufferSize, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST(zeCommandListAppendLaunchKernel, whenCalledThenRedirectedToObject) {
    Mock<CommandList> commandList;
    Mock<::L0::Kernel> kernel;
    ze_group_count_t dispatchFunctionArguments;

    EXPECT_CALL(commandList, appendLaunchKernel(kernel.toHandle(), &dispatchFunctionArguments,
                                                nullptr, 0, nullptr))
        .Times(1);

    auto result =
        zeCommandListAppendLaunchKernel(commandList.toHandle(), kernel.toHandle(),
                                        &dispatchFunctionArguments, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeCommandListAppendEventReset, whenCalledThenRedirectedToObject) {
    Mock<CommandList> commandList;
    ze_event_handle_t event = reinterpret_cast<ze_event_handle_t>(0x2000);

    EXPECT_CALL(commandList, appendEventReset(event)).Times(1);

    auto result = zeCommandListAppendEventReset(commandList.toHandle(), event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeCommandListAppendExecutionBarrier, whenCalledThenRedirectedToObject) {
    Mock<CommandList> commandList;

    EXPECT_CALL(commandList, appendBarrier(::testing::_, ::testing::_, ::testing::_)).Times(1);

    auto result = zeCommandListAppendBarrier(commandList.toHandle(), nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeCommandListAppendSignalEvent, redirectsToObject) {
    Mock<CommandList> commandList;
    ze_event_handle_t event = reinterpret_cast<ze_event_handle_t>(0x2000);

    EXPECT_CALL(commandList, appendSignalEvent(event)).Times(1);

    auto result = zeCommandListAppendSignalEvent(commandList.toHandle(), event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0