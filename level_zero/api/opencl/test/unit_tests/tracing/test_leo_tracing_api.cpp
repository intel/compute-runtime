/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/tracing/leo_tracing_api.h"
#include "level_zero/api/opencl/source/tracing/leo_tracing_handle.h"
#include "level_zero/api/opencl/source/tracing/leo_tracing_notify.h"

#include "CL/cl.h"

#include <array>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

using namespace HostSideTracing;

struct TracingRecord {
    ClFunctionId fid;
    ClCallbackSite site;
    cl_uint correlationId;
    const char *functionName;
    const cl_ulong *correlationDataPtr;
    cl_ulong correlationDataValue;
    void *userData;
    cl_sampler samplerParam;
    cl_int returnValue;
    bool hasSamplerParam;
    bool hasReturnValue;
};

struct TracingCallbackRecorder {
    static constexpr size_t capacity = 32u;
    static std::array<TracingRecord, capacity> records;
    static size_t count;

    static void reset() {
        records = {};
        count = 0u;
    }

    static void callback(ClFunctionId fid, cl_callback_data *callbackData, void *userData) {
        if (count >= capacity) {
            return;
        }

        TracingRecord record{};
        record.fid = fid;
        record.site = callbackData->site;
        record.correlationId = callbackData->correlationId;
        record.functionName = callbackData->functionName;
        record.userData = userData;
        record.correlationDataPtr = callbackData->correlationData;
        if (callbackData->correlationData != nullptr) {
            record.correlationDataValue = *callbackData->correlationData;
        }
        if (callbackData->functionReturnValue != nullptr) {
            record.hasReturnValue = true;
            record.returnValue = *static_cast<cl_int *>(callbackData->functionReturnValue);
        }
        if (fid == CL_FUNCTION_clRetainSampler && callbackData->functionParams != nullptr) {
            record.hasSamplerParam = true;
            record.samplerParam = *static_cast<const cl_params_clRetainSampler *>(callbackData->functionParams)->sampler;
        }

        records[count++] = record;
    }

    static void writingCallback(ClFunctionId fid, cl_callback_data *callbackData, void *userData) {
        if (callbackData->site == CL_CALLBACK_SITE_ENTER) {
            *callbackData->correlationData = 0xC0FFEEu;
        }
        callback(fid, callbackData, userData);
    }
};

std::array<TracingRecord, TracingCallbackRecorder::capacity> TracingCallbackRecorder::records{};
size_t TracingCallbackRecorder::count = 0u;

static void emptyTracingCallback(ClFunctionId, cl_callback_data *, void *) {}

struct TracingFixture : public ::testing::Test {
    void SetUp() override {
        savedState = tracingState.load();
        savedHandles = {};
        for (size_t i = 0; i < tracingMaxHandleCount; i++) {
            savedHandles[i] = tracingHandle[i];
            tracingHandle[i] = nullptr;
        }
        tracingState.store(0u);
        TracingCallbackRecorder::reset();
    }

    void TearDown() override {
        for (auto handle : createdHandles) {
            clDisableTracingINTEL(handle);
            clDestroyTracingHandleINTEL(handle);
        }
        for (size_t i = 0; i < tracingMaxHandleCount; i++) {
            tracingHandle[i] = savedHandles[i];
        }
        tracingState.store(savedState);
    }

    cl_tracing_handle createHandle(cl_tracing_callback callback = &emptyTracingCallback, void *userData = nullptr) {
        cl_tracing_handle handle = nullptr;
        EXPECT_EQ(CL_SUCCESS, clCreateTracingHandleINTEL(fakeDevice, callback, userData, &handle));
        EXPECT_NE(nullptr, handle);
        createdHandles.push_back(handle);
        return handle;
    }

    size_t countEnabledSlots() const {
        size_t enabled = 0u;
        while (enabled < tracingMaxHandleCount && tracingHandle[enabled] != nullptr) {
            ++enabled;
        }
        return enabled;
    }

    cl_device_id fakeDevice = reinterpret_cast<cl_device_id>(0x1234u);
    std::vector<cl_tracing_handle> createdHandles{};
    std::array<TracingHandle *, tracingMaxHandleCount> savedHandles{};
    uint32_t savedState = 0u;
};

TEST_F(TracingFixture, givenNullDeviceWhenCreateTracingHandleThenReturnsInvalidValue) {
    cl_tracing_handle handle = nullptr;
    EXPECT_EQ(CL_INVALID_VALUE, clCreateTracingHandleINTEL(nullptr, &emptyTracingCallback, nullptr, &handle));
    EXPECT_EQ(nullptr, handle);
}

TEST_F(TracingFixture, givenNullCallbackWhenCreateTracingHandleThenReturnsInvalidValue) {
    cl_tracing_handle handle = nullptr;
    EXPECT_EQ(CL_INVALID_VALUE, clCreateTracingHandleINTEL(fakeDevice, nullptr, nullptr, &handle));
    EXPECT_EQ(nullptr, handle);
}

TEST_F(TracingFixture, givenNullOutputHandleWhenCreateTracingHandleThenReturnsInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, clCreateTracingHandleINTEL(fakeDevice, &emptyTracingCallback, nullptr, nullptr));
}

TEST_F(TracingFixture, givenValidArgumentsWhenCreateTracingHandleThenDeviceAndInnerHandleAreStored) {
    auto handle = createHandle();
    EXPECT_EQ(fakeDevice, handle->device);
    EXPECT_NE(nullptr, handle->handle);
}

TEST_F(TracingFixture, givenCreatedHandleWhenCreatingAnotherThenInnerHandlesAreDistinct) {
    auto first = createHandle();
    auto second = createHandle();
    EXPECT_NE(first, second);
    EXPECT_NE(first->handle, second->handle);
}

TEST_F(TracingFixture, givenCreatedHandleThenNoTracingSlotIsOccupiedAndTracingStaysDisabled) {
    createHandle();
    EXPECT_EQ(0u, countEnabledSlots());
    EXPECT_EQ(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));
}

TEST_F(TracingFixture, givenNullHandleWhenDestroyTracingHandleThenReturnsInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, clDestroyTracingHandleINTEL(nullptr));
}

TEST_F(TracingFixture, givenValidHandleWhenDestroyTracingHandleThenReturnsSuccess) {
    cl_tracing_handle handle = nullptr;
    ASSERT_EQ(CL_SUCCESS, clCreateTracingHandleINTEL(fakeDevice, &emptyTracingCallback, nullptr, &handle));
    EXPECT_EQ(CL_SUCCESS, clDestroyTracingHandleINTEL(handle));
}

TEST_F(TracingFixture, givenNullHandleWhenSetTracingPointThenReturnsInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, clSetTracingPointINTEL(nullptr, CL_FUNCTION_clBuildProgram, CL_TRUE));
}

TEST_F(TracingFixture, givenOutOfRangeFunctionIdWhenSetTracingPointThenReturnsInvalidValue) {
    auto handle = createHandle();
    EXPECT_EQ(CL_INVALID_VALUE, clSetTracingPointINTEL(handle, static_cast<ClFunctionId>(CL_FUNCTION_COUNT), CL_TRUE));
    EXPECT_EQ(CL_INVALID_VALUE, clSetTracingPointINTEL(handle, static_cast<ClFunctionId>(CL_FUNCTION_COUNT + 1), CL_TRUE));
    EXPECT_EQ(CL_INVALID_VALUE, clSetTracingPointINTEL(handle, static_cast<ClFunctionId>(0xFF), CL_TRUE));
}

TEST_F(TracingFixture, givenNewHandleWhenQueryingEveryTracingPointThenAllAreDisabled) {
    auto handle = createHandle();
    for (uint32_t fid = 0; fid < static_cast<uint32_t>(CL_FUNCTION_COUNT); fid++) {
        EXPECT_FALSE(handle->handle->getTracingPoint(static_cast<ClFunctionId>(fid))) << "fid " << fid;
    }
}

TEST_F(TracingFixture, givenEnabledTracingPointWhenQueryingThenOnlyThatPointIsEnabled) {
    auto handle = createHandle();
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(handle, CL_FUNCTION_clRetainSampler, CL_TRUE));

    for (uint32_t fid = 0; fid < static_cast<uint32_t>(CL_FUNCTION_COUNT); fid++) {
        const auto expected = (fid == static_cast<uint32_t>(CL_FUNCTION_clRetainSampler));
        EXPECT_EQ(expected, handle->handle->getTracingPoint(static_cast<ClFunctionId>(fid))) << "fid " << fid;
    }
}

TEST_F(TracingFixture, givenEveryTracingPointEnabledWhenQueryingThenAllAreEnabled) {
    auto handle = createHandle();
    for (uint32_t fid = 0; fid < static_cast<uint32_t>(CL_FUNCTION_COUNT); fid++) {
        ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(handle, static_cast<ClFunctionId>(fid), CL_TRUE));
    }
    for (uint32_t fid = 0; fid < static_cast<uint32_t>(CL_FUNCTION_COUNT); fid++) {
        EXPECT_TRUE(handle->handle->getTracingPoint(static_cast<ClFunctionId>(fid))) << "fid " << fid;
    }
}

TEST_F(TracingFixture, givenEnabledTracingPointWhenDisablingItThenItBecomesDisabled) {
    auto handle = createHandle();
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(handle, CL_FUNCTION_clRetainSampler, CL_TRUE));
    ASSERT_TRUE(handle->handle->getTracingPoint(CL_FUNCTION_clRetainSampler));

    EXPECT_EQ(CL_SUCCESS, clSetTracingPointINTEL(handle, CL_FUNCTION_clRetainSampler, CL_FALSE));
    EXPECT_FALSE(handle->handle->getTracingPoint(CL_FUNCTION_clRetainSampler));
}

TEST_F(TracingFixture, givenTracingPointSetOnOneHandleWhenQueryingAnotherThenItIsNotAffected) {
    auto first = createHandle();
    auto second = createHandle();
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(first, CL_FUNCTION_clRetainSampler, CL_TRUE));

    EXPECT_TRUE(first->handle->getTracingPoint(CL_FUNCTION_clRetainSampler));
    EXPECT_FALSE(second->handle->getTracingPoint(CL_FUNCTION_clRetainSampler));
}

TEST_F(TracingFixture, givenNullHandleWhenEnableTracingThenReturnsInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, clEnableTracingINTEL(nullptr));
}

TEST_F(TracingFixture, givenNullHandleWhenDisableTracingThenReturnsInvalidValue) {
    EXPECT_EQ(CL_INVALID_VALUE, clDisableTracingINTEL(nullptr));
}

TEST_F(TracingFixture, givenHandleWhenEnableTracingThenFirstSlotIsTakenAndEnabledBitIsSet) {
    auto handle = createHandle();

    EXPECT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));
    EXPECT_EQ(handle->handle, tracingHandle[0]);
    EXPECT_EQ(1u, countEnabledSlots());
    EXPECT_NE(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));
    EXPECT_EQ(0u, TRACING_GET_LOCKED_BIT(tracingState.load()));
    EXPECT_EQ(0u, TRACING_GET_CLIENT_COUNTER(tracingState.load()));
}

TEST_F(TracingFixture, givenAlreadyEnabledHandleWhenEnableTracingAgainThenReturnsInvalidValueAndSlotsAreUnchanged) {
    auto handle = createHandle();
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));

    EXPECT_EQ(CL_INVALID_VALUE, clEnableTracingINTEL(handle));
    EXPECT_EQ(1u, countEnabledSlots());
    EXPECT_NE(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));
}

TEST_F(TracingFixture, givenTwoHandlesWhenEnabledThenBothOccupyConsecutiveSlots) {
    auto first = createHandle();
    auto second = createHandle();

    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(first));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(second));

    EXPECT_EQ(first->handle, tracingHandle[0]);
    EXPECT_EQ(second->handle, tracingHandle[1]);
    EXPECT_EQ(2u, countEnabledSlots());
}

TEST_F(TracingFixture, givenSingleEnabledHandleWhenDisabledThenSlotIsFreedAndEnabledBitIsCleared) {
    auto handle = createHandle();
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));

    EXPECT_EQ(CL_SUCCESS, clDisableTracingINTEL(handle));
    EXPECT_EQ(nullptr, tracingHandle[0]);
    EXPECT_EQ(0u, countEnabledSlots());
    EXPECT_EQ(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));
}

TEST_F(TracingFixture, givenTwoEnabledHandlesWhenDisablingTheFirstThenTheLastIsMovedIntoItsSlot) {
    auto first = createHandle();
    auto second = createHandle();
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(first));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(second));

    EXPECT_EQ(CL_SUCCESS, clDisableTracingINTEL(first));

    EXPECT_EQ(second->handle, tracingHandle[0]);
    EXPECT_EQ(nullptr, tracingHandle[1]);
    EXPECT_EQ(1u, countEnabledSlots());
    EXPECT_NE(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));
}

TEST_F(TracingFixture, givenTwoEnabledHandlesWhenDisablingTheLastThenTheFirstStaysInPlace) {
    auto first = createHandle();
    auto second = createHandle();
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(first));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(second));

    EXPECT_EQ(CL_SUCCESS, clDisableTracingINTEL(second));

    EXPECT_EQ(first->handle, tracingHandle[0]);
    EXPECT_EQ(nullptr, tracingHandle[1]);
    EXPECT_EQ(1u, countEnabledSlots());
    EXPECT_NE(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));
}

TEST_F(TracingFixture, givenTwoEnabledHandlesWhenBothAreDisabledThenEnabledBitIsCleared) {
    auto first = createHandle();
    auto second = createHandle();
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(first));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(second));

    ASSERT_EQ(CL_SUCCESS, clDisableTracingINTEL(first));
    EXPECT_NE(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));

    ASSERT_EQ(CL_SUCCESS, clDisableTracingINTEL(second));
    EXPECT_EQ(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));
    EXPECT_EQ(0u, countEnabledSlots());
}

TEST_F(TracingFixture, givenNeverEnabledHandleWhenDisableTracingThenReturnsInvalidValue) {
    auto handle = createHandle();
    EXPECT_EQ(CL_INVALID_VALUE, clDisableTracingINTEL(handle));
}

TEST_F(TracingFixture, givenOtherHandleEnabledWhenDisablingANotEnabledHandleThenReturnsInvalidValueAndSlotsAreUnchanged) {
    auto enabled = createHandle();
    auto notEnabled = createHandle();
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(enabled));

    EXPECT_EQ(CL_INVALID_VALUE, clDisableTracingINTEL(notEnabled));
    EXPECT_EQ(enabled->handle, tracingHandle[0]);
    EXPECT_EQ(1u, countEnabledSlots());
}

TEST_F(TracingFixture, givenDisabledHandleWhenDisablingItAgainThenReturnsInvalidValue) {
    auto handle = createHandle();
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));
    ASSERT_EQ(CL_SUCCESS, clDisableTracingINTEL(handle));

    EXPECT_EQ(CL_INVALID_VALUE, clDisableTracingINTEL(handle));
}

TEST_F(TracingFixture, givenDisabledHandleWhenEnablingItAgainThenItTakesASlotAgain) {
    auto handle = createHandle();
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));
    ASSERT_EQ(CL_SUCCESS, clDisableTracingINTEL(handle));

    EXPECT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));
    EXPECT_EQ(handle->handle, tracingHandle[0]);
    EXPECT_NE(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));
}

TEST_F(TracingFixture, givenMaximumNumberOfEnabledHandlesWhenEnablingOneMoreThenReturnsOutOfResources) {
    for (size_t i = 0; i < tracingMaxHandleCount; i++) {
        ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(createHandle())) << "handle " << i;
    }
    ASSERT_EQ(tracingMaxHandleCount, countEnabledSlots());

    auto overflowing = createHandle();
    EXPECT_EQ(CL_OUT_OF_RESOURCES, clEnableTracingINTEL(overflowing));
    EXPECT_EQ(tracingMaxHandleCount, countEnabledSlots());
}

TEST_F(TracingFixture, givenMaximumNumberOfEnabledHandlesWhenAllAreDisabledInOrderThenEnabledBitIsClearedOnlyAtTheEnd) {
    std::vector<cl_tracing_handle> handles;
    for (size_t i = 0; i < tracingMaxHandleCount; i++) {
        handles.push_back(createHandle());
        ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handles.back()));
    }

    for (size_t i = 0; i < tracingMaxHandleCount; i++) {
        EXPECT_NE(0u, TRACING_GET_ENABLED_BIT(tracingState.load())) << "before disabling " << i;
        ASSERT_EQ(CL_SUCCESS, clDisableTracingINTEL(handles[i]));
        EXPECT_EQ(tracingMaxHandleCount - i - 1u, countEnabledSlots());
    }
    EXPECT_EQ(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));
}

TEST_F(TracingFixture, givenNullHandleWhenGetTracingStateThenReturnsInvalidValue) {
    cl_bool enabled = CL_TRUE;
    EXPECT_EQ(CL_INVALID_VALUE, clGetTracingStateINTEL(nullptr, &enabled));
}

TEST_F(TracingFixture, givenNullOutputWhenGetTracingStateThenReturnsInvalidValue) {
    auto handle = createHandle();
    EXPECT_EQ(CL_INVALID_VALUE, clGetTracingStateINTEL(handle, nullptr));
}

TEST_F(TracingFixture, givenNotEnabledHandleWhenGetTracingStateThenReturnsFalse) {
    auto handle = createHandle();
    cl_bool enabled = CL_TRUE;
    EXPECT_EQ(CL_SUCCESS, clGetTracingStateINTEL(handle, &enabled));
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), enabled);
}

TEST_F(TracingFixture, givenEnabledHandleWhenGetTracingStateThenReturnsTrue) {
    auto handle = createHandle();
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));

    cl_bool enabled = CL_FALSE;
    EXPECT_EQ(CL_SUCCESS, clGetTracingStateINTEL(handle, &enabled));
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), enabled);
}

TEST_F(TracingFixture, givenHandleDisabledAfterEnableWhenGetTracingStateThenReturnsFalse) {
    auto handle = createHandle();
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));
    ASSERT_EQ(CL_SUCCESS, clDisableTracingINTEL(handle));

    cl_bool enabled = CL_TRUE;
    EXPECT_EQ(CL_SUCCESS, clGetTracingStateINTEL(handle, &enabled));
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), enabled);
}

TEST_F(TracingFixture, givenOneOfTwoHandlesEnabledWhenGetTracingStateThenOnlyTheEnabledOneReportsTrue) {
    auto enabledHandle = createHandle();
    auto disabledHandle = createHandle();
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(enabledHandle));

    cl_bool enabled = CL_FALSE;
    EXPECT_EQ(CL_SUCCESS, clGetTracingStateINTEL(enabledHandle, &enabled));
    EXPECT_EQ(static_cast<cl_bool>(CL_TRUE), enabled);

    EXPECT_EQ(CL_SUCCESS, clGetTracingStateINTEL(disabledHandle, &enabled));
    EXPECT_EQ(static_cast<cl_bool>(CL_FALSE), enabled);
}

TEST_F(TracingFixture, givenTracingDisabledWhenAddTracingClientThenReturnsFalseAndCounterStaysZero) {
    EXPECT_FALSE(addTracingClient());
    EXPECT_EQ(0u, TRACING_GET_CLIENT_COUNTER(tracingState.load()));
}

TEST_F(TracingFixture, givenTracingEnabledWhenAddTracingClientThenCounterIsIncremented) {
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(createHandle()));

    EXPECT_TRUE(addTracingClient());
    EXPECT_EQ(1u, TRACING_GET_CLIENT_COUNTER(tracingState.load()));

    removeTracingClient();
    EXPECT_EQ(0u, TRACING_GET_CLIENT_COUNTER(tracingState.load()));
}

TEST_F(TracingFixture, givenTracingEnabledWhenSeveralClientsAreAddedThenCounterTracksThemAll) {
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(createHandle()));

    constexpr uint32_t clients = 5u;
    for (uint32_t i = 0; i < clients; i++) {
        EXPECT_TRUE(addTracingClient());
        EXPECT_EQ(i + 1u, TRACING_GET_CLIENT_COUNTER(tracingState.load()));
    }
    for (uint32_t i = 0; i < clients; i++) {
        removeTracingClient();
        EXPECT_EQ(clients - i - 1u, TRACING_GET_CLIENT_COUNTER(tracingState.load()));
    }
    EXPECT_NE(0u, TRACING_GET_ENABLED_BIT(tracingState.load()));
}

TEST_F(TracingFixture, givenEnabledTracingPointWhenApiIsCalledThenCallbackIsInvokedOnEnterAndExit) {
    int userData = 0;
    auto handle = createHandle(&TracingCallbackRecorder::callback, &userData);
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(handle, CL_FUNCTION_clRetainSampler, CL_TRUE));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));

    EXPECT_EQ(CL_INVALID_SAMPLER, clRetainSampler(nullptr));

    ASSERT_EQ(2u, TracingCallbackRecorder::count);
    EXPECT_EQ(CL_FUNCTION_clRetainSampler, TracingCallbackRecorder::records[0].fid);
    EXPECT_EQ(CL_CALLBACK_SITE_ENTER, TracingCallbackRecorder::records[0].site);
    EXPECT_EQ(CL_FUNCTION_clRetainSampler, TracingCallbackRecorder::records[1].fid);
    EXPECT_EQ(CL_CALLBACK_SITE_EXIT, TracingCallbackRecorder::records[1].site);
    EXPECT_EQ(&userData, TracingCallbackRecorder::records[0].userData);
    EXPECT_EQ(&userData, TracingCallbackRecorder::records[1].userData);
}

TEST_F(TracingFixture, givenEnabledTracingPointWhenApiIsCalledThenCallbackSeesFunctionNameParamsAndReturnValue) {
    auto handle = createHandle(&TracingCallbackRecorder::callback);
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(handle, CL_FUNCTION_clRetainSampler, CL_TRUE));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));

    clRetainSampler(nullptr);

    ASSERT_EQ(2u, TracingCallbackRecorder::count);
    const auto &enterRecord = TracingCallbackRecorder::records[0];
    const auto &exitRecord = TracingCallbackRecorder::records[1];

    EXPECT_STREQ("clRetainSampler", enterRecord.functionName);
    EXPECT_STREQ("clRetainSampler", exitRecord.functionName);

    EXPECT_TRUE(enterRecord.hasSamplerParam);
    EXPECT_EQ(nullptr, enterRecord.samplerParam);
    EXPECT_TRUE(exitRecord.hasSamplerParam);

    EXPECT_FALSE(enterRecord.hasReturnValue);
    ASSERT_TRUE(exitRecord.hasReturnValue);
    EXPECT_EQ(CL_INVALID_SAMPLER, exitRecord.returnValue);
}

TEST_F(TracingFixture, givenEnabledTracingWhenApiIsCalledThenEnterAndExitShareCorrelationIdAndData) {
    auto handle = createHandle(&TracingCallbackRecorder::writingCallback);
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(handle, CL_FUNCTION_clRetainSampler, CL_TRUE));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));

    clRetainSampler(nullptr);

    ASSERT_EQ(2u, TracingCallbackRecorder::count);
    EXPECT_EQ(TracingCallbackRecorder::records[0].correlationId, TracingCallbackRecorder::records[1].correlationId);
    EXPECT_EQ(TracingCallbackRecorder::records[0].correlationDataPtr, TracingCallbackRecorder::records[1].correlationDataPtr);
    ASSERT_NE(nullptr, TracingCallbackRecorder::records[1].correlationDataPtr);
    EXPECT_EQ(0xC0FFEEu, TracingCallbackRecorder::records[1].correlationDataValue);
}

TEST_F(TracingFixture, givenTwoSubsequentApiCallsWhenTracedThenCorrelationIdsDiffer) {
    auto handle = createHandle(&TracingCallbackRecorder::callback);
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(handle, CL_FUNCTION_clRetainSampler, CL_TRUE));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));

    clRetainSampler(nullptr);
    clRetainSampler(nullptr);

    ASSERT_EQ(4u, TracingCallbackRecorder::count);
    EXPECT_NE(TracingCallbackRecorder::records[0].correlationId, TracingCallbackRecorder::records[2].correlationId);
}

TEST_F(TracingFixture, givenTracingEnabledButPointDisabledWhenApiIsCalledThenCallbackIsNotInvoked) {
    auto handle = createHandle(&TracingCallbackRecorder::callback);
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(handle, CL_FUNCTION_clReleaseSampler, CL_TRUE));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));

    clRetainSampler(nullptr);

    EXPECT_EQ(0u, TracingCallbackRecorder::count);
}

TEST_F(TracingFixture, givenTracingPointSetButTracingNotEnabledWhenApiIsCalledThenCallbackIsNotInvoked) {
    auto handle = createHandle(&TracingCallbackRecorder::callback);
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(handle, CL_FUNCTION_clRetainSampler, CL_TRUE));

    clRetainSampler(nullptr);

    EXPECT_EQ(0u, TracingCallbackRecorder::count);
}

TEST_F(TracingFixture, givenTwoEnabledHandlesTracingTheSamePointWhenApiIsCalledThenBothAreNotified) {
    int firstUserData = 0;
    int secondUserData = 0;
    auto first = createHandle(&TracingCallbackRecorder::callback, &firstUserData);
    auto second = createHandle(&TracingCallbackRecorder::callback, &secondUserData);
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(first, CL_FUNCTION_clRetainSampler, CL_TRUE));
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(second, CL_FUNCTION_clRetainSampler, CL_TRUE));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(first));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(second));

    clRetainSampler(nullptr);

    ASSERT_EQ(4u, TracingCallbackRecorder::count);
    EXPECT_EQ(&firstUserData, TracingCallbackRecorder::records[0].userData);
    EXPECT_EQ(&secondUserData, TracingCallbackRecorder::records[1].userData);
    EXPECT_EQ(CL_CALLBACK_SITE_ENTER, TracingCallbackRecorder::records[0].site);
    EXPECT_EQ(CL_CALLBACK_SITE_ENTER, TracingCallbackRecorder::records[1].site);
    EXPECT_EQ(CL_CALLBACK_SITE_EXIT, TracingCallbackRecorder::records[2].site);
    EXPECT_EQ(CL_CALLBACK_SITE_EXIT, TracingCallbackRecorder::records[3].site);
}

TEST_F(TracingFixture, givenTwoEnabledHandlesWhenOnlyOneTracesThePointThenOnlyItIsNotified) {
    int tracingUserData = 0;
    int idleUserData = 0;
    auto tracing = createHandle(&TracingCallbackRecorder::callback, &tracingUserData);
    auto idle = createHandle(&TracingCallbackRecorder::callback, &idleUserData);
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(tracing, CL_FUNCTION_clRetainSampler, CL_TRUE));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(tracing));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(idle));

    clRetainSampler(nullptr);

    ASSERT_EQ(2u, TracingCallbackRecorder::count);
    EXPECT_EQ(&tracingUserData, TracingCallbackRecorder::records[0].userData);
    EXPECT_EQ(&tracingUserData, TracingCallbackRecorder::records[1].userData);
}

TEST_F(TracingFixture, givenTwoEnabledHandlesWhenTheyTraceTheSamePointThenCorrelationDataSlotsAreDistinct) {
    auto first = createHandle(&TracingCallbackRecorder::callback);
    auto second = createHandle(&TracingCallbackRecorder::callback);
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(first, CL_FUNCTION_clRetainSampler, CL_TRUE));
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(second, CL_FUNCTION_clRetainSampler, CL_TRUE));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(first));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(second));

    clRetainSampler(nullptr);

    ASSERT_EQ(4u, TracingCallbackRecorder::count);
    EXPECT_NE(TracingCallbackRecorder::records[0].correlationDataPtr, TracingCallbackRecorder::records[1].correlationDataPtr);
    EXPECT_EQ(TracingCallbackRecorder::records[0].correlationDataPtr, TracingCallbackRecorder::records[2].correlationDataPtr);
    EXPECT_EQ(TracingCallbackRecorder::records[1].correlationDataPtr, TracingCallbackRecorder::records[3].correlationDataPtr);
}

TEST_F(TracingFixture, givenEnabledHandleWhenItIsDisabledBeforeTheApiCallThenCallbackIsNotInvoked) {
    auto handle = createHandle(&TracingCallbackRecorder::callback);
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(handle, CL_FUNCTION_clRetainSampler, CL_TRUE));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));
    ASSERT_EQ(CL_SUCCESS, clDisableTracingINTEL(handle));

    clRetainSampler(nullptr);

    EXPECT_EQ(0u, TracingCallbackRecorder::count);
}

TEST_F(TracingFixture, givenTracedApiCallWhenItCompletesThenClientCounterIsBackToZero) {
    auto handle = createHandle(&TracingCallbackRecorder::callback);
    ASSERT_EQ(CL_SUCCESS, clSetTracingPointINTEL(handle, CL_FUNCTION_clRetainSampler, CL_TRUE));
    ASSERT_EQ(CL_SUCCESS, clEnableTracingINTEL(handle));

    clRetainSampler(nullptr);

    EXPECT_EQ(0u, TRACING_GET_CLIENT_COUNTER(tracingState.load()));
    EXPECT_EQ(0u, TRACING_GET_LOCKED_BIT(tracingState.load()));
}

TEST(TracingHandleTests, givenCallbackAndUserDataWhenCallingHandleThenBothArePassedThrough) {
    TracingCallbackRecorder::reset();
    int userData = 42;
    TracingHandle handle{&TracingCallbackRecorder::callback, &userData};

    cl_callback_data data{};
    data.site = CL_CALLBACK_SITE_EXIT;
    data.functionName = "someFunction";
    handle.call(CL_FUNCTION_clFlush, &data);

    ASSERT_EQ(1u, TracingCallbackRecorder::count);
    EXPECT_EQ(CL_FUNCTION_clFlush, TracingCallbackRecorder::records[0].fid);
    EXPECT_EQ(CL_CALLBACK_SITE_EXIT, TracingCallbackRecorder::records[0].site);
    EXPECT_EQ(&userData, TracingCallbackRecorder::records[0].userData);
}

TEST(TracingHandleTests, givenFreshHandleWhenSettingAndClearingTracingPointsThenEachIsIndependent) {
    TracingHandle handle{&emptyTracingCallback, nullptr};

    handle.setTracingPoint(CL_FUNCTION_clFlush, true);
    handle.setTracingPoint(CL_FUNCTION_clFinish, true);
    EXPECT_TRUE(handle.getTracingPoint(CL_FUNCTION_clFlush));
    EXPECT_TRUE(handle.getTracingPoint(CL_FUNCTION_clFinish));

    handle.setTracingPoint(CL_FUNCTION_clFlush, false);
    EXPECT_FALSE(handle.getTracingPoint(CL_FUNCTION_clFlush));
    EXPECT_TRUE(handle.getTracingPoint(CL_FUNCTION_clFinish));
}

} // namespace ult
} // namespace LEO
} // namespace NEO
