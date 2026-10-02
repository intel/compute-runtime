/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/internal/l0_cmdlist.h"
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

TEST(zeCommandListAppendMemoryFill, whenSizeNotMultipleOfPatternSizeThenReturnError) {
    MockCommandList commandList;
    size_t bufferSize = 4095u;

    int value = 0;
    auto res = zeCommandListAppendMemoryFill(&commandList, reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(&value),
                                             4u, bufferSize, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, res);
}

TEST(zeCommandListAppendMemoryFill, whenPatternSizeIsZeroThenReturnError) {
    MockCommandList commandList;
    size_t bufferSize = 4096u;

    int value = 0;
    auto res = zeCommandListAppendMemoryFill(&commandList, reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(&value),
                                             0u, bufferSize, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, res);
}

TEST(zeCommandListAppendMemoryFillWithParameters, whenCalledThenRedirectedToObject) {
    MockCommandList commandList;
    size_t bufferSize = 4096u;

    int value = 0;
    auto res = zeCommandListAppendMemoryFillWithParameters(&commandList, reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(&value),
                                                           sizeof(value), bufferSize, nullptr, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_SUCCESS, res);
}

TEST(zeCommandListAppendMemoryFillWithParameters, whenPatternSizeNotPowerOf2ThenReturnError) {
    MockCommandList commandList;
    size_t bufferSize = 4096u;

    int value = 0;
    auto res = zeCommandListAppendMemoryFillWithParameters(&commandList, reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(&value),
                                                           3u, bufferSize, nullptr, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, res);
}

TEST(zeCommandListAppendMemoryFillWithParameters, whenSizeNotMultipleOfPatternSizeThenReturnError) {
    MockCommandList commandList;
    size_t bufferSize = 4095u;

    int value = 0;
    auto res = zeCommandListAppendMemoryFillWithParameters(&commandList, reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(&value),
                                                           4u, bufferSize, nullptr, nullptr, 0, nullptr);
    ASSERT_EQ(ZE_RESULT_ERROR_INVALID_SIZE, res);
}

TEST(zeCommandListAppendMemoryFillWithParameters, whenPatternSizeIsZeroThenReturnError) {
    MockCommandList commandList;
    size_t bufferSize = 4096u;

    int value = 0;
    auto res = zeCommandListAppendMemoryFillWithParameters(&commandList, reinterpret_cast<void *>(0x1000), reinterpret_cast<void *>(&value),
                                                           0u, bufferSize, nullptr, nullptr, 0, nullptr);
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

struct MockCommandListCaptureSignalWaitEventParams : public MockCommandList {
    ze_result_t appendSignalEvent(ze_event_handle_t hEvent, CmdListSignalEventParameters &signalEventParameters) override {
        capturedSignalEventParameters = signalEventParameters;
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t appendWaitOnEvents(uint32_t numEvents, ze_event_handle_t *phEvent, CmdListWaitEventParameters &waitEventParams) override {
        capturedWaitEventParameters = waitEventParams;
        return ZE_RESULT_SUCCESS;
    }

    CmdListSignalEventParameters capturedSignalEventParameters;
    CmdListWaitEventParameters capturedWaitEventParameters;
};

TEST(zeCommandListAppendSignalEventWithParameters, WhenAppendingSignalEventWithFlagsExtensionThenParamIsCorrectlySet) {
    MockCommandListCaptureSignalWaitEventParams commandList;
    Mock<Event> eventObj;
    ze_event_handle_t event = eventObj.toHandle();

    ze_event_flags_exp_desc_t eventFlagsDesc = {ZE_STRUCTURE_TYPE_EVENT_FLAGS_EXP_DESC, nullptr, ZE_EVENT_FLAG_EXP_MODE_GRAPH_EXTERNAL};

    auto result = zeCommandListAppendSignalEventWithParameters(commandList.toHandle(), &eventFlagsDesc, event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(commandList.capturedSignalEventParameters.apiRequestForGraphExternal);

    eventFlagsDesc.flags = 0;
    result = zeCommandListAppendSignalEventWithParameters(commandList.toHandle(), &eventFlagsDesc, event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(commandList.capturedSignalEventParameters.apiRequestForGraphExternal);
}

TEST(zeCommandListAppendSignalEventWithParameters, WhenAppendingSignalEventWithInvalidExtensionThenErrorIsReturned) {
    MockCommandListCaptureSignalWaitEventParams commandList;
    Mock<Event> eventObj;
    ze_event_handle_t event = eventObj.toHandle();

    ze_base_desc_t unknownDesc = {ZE_STRUCTURE_TYPE_FORCE_UINT32, nullptr};

    auto result = zeCommandListAppendSignalEventWithParameters(commandList.toHandle(), &unknownDesc, event);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST(zeCommandListAppendSignalEventWithParameters, WhenAppendingSignalEventWithNullptrExtensionThenNoParamIsSet) {
    MockCommandListCaptureSignalWaitEventParams commandList;
    Mock<Event> eventObj;
    ze_event_handle_t event = eventObj.toHandle();

    auto result = zeCommandListAppendSignalEventWithParameters(commandList.toHandle(), nullptr, event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(commandList.capturedSignalEventParameters.apiRequestForGraphExternal);
}

TEST(zeCommandListAppendWaitOnEventsWithParameters, WhenAppendingWaitOnEventsWithFlagsExtensionThenParamIsCorrectlySet) {
    MockCommandListCaptureSignalWaitEventParams commandList;
    Mock<Event> eventObj;
    ze_event_handle_t event = eventObj.toHandle();

    ze_event_flags_exp_desc_t eventFlagsDesc = {ZE_STRUCTURE_TYPE_EVENT_FLAGS_EXP_DESC, nullptr, ZE_EVENT_FLAG_EXP_MODE_GRAPH_EXTERNAL};

    auto result = zeCommandListAppendWaitOnEventsWithParameters(commandList.toHandle(), &eventFlagsDesc, 1, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_TRUE(commandList.capturedWaitEventParameters.apiRequestForGraphExternal);

    eventFlagsDesc.flags = 0;
    result = zeCommandListAppendWaitOnEventsWithParameters(commandList.toHandle(), &eventFlagsDesc, 1, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(commandList.capturedWaitEventParameters.apiRequestForGraphExternal);
}

TEST(zeCommandListAppendWaitOnEventsWithParameters, WhenAppendingWaitOnEventsWithInvalidExtensionThenErrorIsReturned) {
    MockCommandListCaptureSignalWaitEventParams commandList;
    Mock<Event> eventObj;
    ze_event_handle_t event = eventObj.toHandle();

    ze_base_desc_t unknownDesc = {ZE_STRUCTURE_TYPE_FORCE_UINT32, nullptr};

    auto result = zeCommandListAppendWaitOnEventsWithParameters(commandList.toHandle(), &unknownDesc, 1, &event);
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, result);
}

TEST(zeCommandListAppendWaitOnEventsWithParameters, WhenAppendingWaitOnEventsWithNullptrExtensionThenNoParamIsSet) {
    MockCommandListCaptureSignalWaitEventParams commandList;
    Mock<Event> eventObj;
    ze_event_handle_t event = eventObj.toHandle();

    auto result = zeCommandListAppendWaitOnEventsWithParameters(commandList.toHandle(), nullptr, 1, &event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_FALSE(commandList.capturedWaitEventParameters.apiRequestForGraphExternal);
}

} // namespace ult
} // namespace L0
