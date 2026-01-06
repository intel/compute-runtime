/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include <level_zero/ze_api.h>

namespace L0 {
namespace ult {

TEST(zeCommandListClose, whenCalledThenRedirectedToObject) {
    MockCommandList commandList;

    auto result = zeCommandListClose(commandList.toHandle());
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeCommandListAppendMemoryPrefetch, whenCalledThenRedirectedToObject) {
    MockCommandList commandList;

    auto res = zeCommandListAppendMemoryPrefetch(&commandList, reinterpret_cast<const void *>(0x1000), 0x1000);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
}

using zeCommandListAppendMemAdviseTest = Test<DeviceFixture>;
TEST_F(zeCommandListAppendMemAdviseTest, whenCalledThenRedirectedToObject) {
    MockCommandList commandList;

    auto res = zeCommandListAppendMemAdvise(&commandList, device->toHandle(), reinterpret_cast<const void *>(0x1000), 0x1000, ZE_MEMORY_ADVICE_BIAS_CACHED);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST(zeCommandListAppendMemoryCopy, whenCalledThenRedirectedToObject) {
    MockCommandList commandList;

    auto res = zeCommandListAppendMemoryCopy(&commandList, reinterpret_cast<void *>(0x2000), reinterpret_cast<const void *>(0x1000), 0x1000, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST(zeCommandListAppendMemoryFill, whenCalledThenRedirectedToObject) {
    MockCommandList commandList;
    size_t bufferSize = 4096u;

    int value = 0;
    auto res = zeCommandListAppendMemoryFill(&commandList, reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(&value),
                                             sizeof(value), bufferSize, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST(zeCommandListAppendMemoryFill, whenPatternSizeNotPowerOf2ThenReturnError) {
    MockCommandList commandList;
    size_t bufferSize = 4096u;

    int value = 0;
    auto res = zeCommandListAppendMemoryFill(&commandList, reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(&value),
                                             3u, bufferSize, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, res);
}

TEST(zeCommandListAppendWaitOnEvent, whenCalledThenRedirectedToObject) {
    MockCommandList commandList;
    Mock<Event> eventObj;
    ze_event_handle_t event = eventObj.toHandle();

    auto result = zeCommandListAppendWaitOnEvents(commandList.toHandle(), 1, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeCommandListAppendWriteGlobalTimestamp, whenCalledThenRedirectedToObject) {
    MockCommandList commandList;

    auto result = zeCommandListAppendWriteGlobalTimestamp(commandList.toHandle(), nullptr, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeCommandListAppendLaunchKernel, whenCalledThenRedirectedToObject) {
    MockCommandList commandList;
    Mock<::L0::KernelImp> kernel;
    ze_group_count_t dispatchKernelArguments;

    auto result =
        zeCommandListAppendLaunchKernel(commandList.toHandle(), kernel.toHandle(),
                                        &dispatchKernelArguments, nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}
TEST(zeCommandListAppendEventReset, whenCalledThenRedirectedToObject) {
    MockCommandList commandList;
    Mock<Event> eventObj;
    ze_event_handle_t event = eventObj.toHandle();

    auto result = zeCommandListAppendEventReset(commandList.toHandle(), event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeCommandListAppendExecutionBarrier, whenCalledThenRedirectedToObject) {
    MockCommandList commandList;

    auto result = zeCommandListAppendBarrier(commandList.toHandle(), nullptr, 0, nullptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST(zeCommandListAppendSignalEvent, WhenAppendingSignalEventThenSuccessIsReturned) {
    MockCommandList commandList;
    Mock<Event> eventObj;
    ze_event_handle_t event = eventObj.toHandle();

    auto result = zeCommandListAppendSignalEvent(commandList.toHandle(), event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

} // namespace ult
} // namespace L0
