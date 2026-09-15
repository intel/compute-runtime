/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/extensions/public/cl_ext_private.h"
#include "level_zero/api/opencl/source/cl_device/leo_cl_device.h"
#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"

#include "CL/cl.h"
#include "CL/cl_ext.h"

#include <array>
#include <memory>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct ContextImplFixture : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        clDevice = platform->getDevices()[0].get();
        clDeviceId = clDevice;
    }

    std::unique_ptr<Context> createContext(const cl_context_properties *properties = nullptr) {
        return std::make_unique<Context>(properties, this->L0::ult::DeviceFixture::context->toHandle(), 1, &clDeviceId, true);
    }

    ClDevice *clDevice = nullptr;
    cl_device_id clDeviceId = nullptr;
};

TEST_F(ContextImplFixture, givenNoPropertiesWhenInitializingThenSucceeds) {
    auto context = createContext();
    EXPECT_EQ(CL_SUCCESS, context->initialize());
}

TEST_F(ContextImplFixture, givenInitializedContextThenInternalCommandListsAreCreated) {
    auto context = createContext();
    ASSERT_EQ(CL_SUCCESS, context->initialize());

    EXPECT_NE(nullptr, context->getInternalCopyCmdList());
    EXPECT_NE(nullptr, context->getInternalComputeCmdList());
    EXPECT_NE(context->getInternalCopyCmdList(), context->getInternalComputeCmdList());
}

TEST_F(ContextImplFixture, givenInitializedContextThenInternalCommandListsAreKeyedByRootDeviceIndex) {
    auto context = createContext();
    ASSERT_EQ(CL_SUCCESS, context->initialize());

    const auto rootDeviceIndex = clDevice->getRootDeviceIndex();
    EXPECT_EQ(context->getInternalCopyCmdList(), context->getInternalCopyCmdList(rootDeviceIndex));
    EXPECT_EQ(context->getInternalComputeCmdList(), context->getInternalComputeCmdList(rootDeviceIndex));
    EXPECT_EQ(rootDeviceIndex, context->getDefaultRootDeviceIndex());
}

TEST_F(ContextImplFixture, givenValidPlatformPropertyWhenInitializingThenSucceeds) {
    cl_context_properties properties[] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(static_cast<cl_platform_id>(platform)), 0};
    auto context = createContext(properties);
    EXPECT_EQ(CL_SUCCESS, context->initialize());
}

TEST_F(ContextImplFixture, givenForeignObjectAsPlatformPropertyWhenInitializingThenReturnsInvalidPlatform) {
    cl_context_properties properties[] = {CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(clDeviceId), 0};
    auto context = createContext(properties);
    EXPECT_EQ(CL_INVALID_PLATFORM, context->initialize());
}

TEST_F(ContextImplFixture, givenNullPlatformPropertyWhenInitializingThenReturnsInvalidPlatform) {
    cl_context_properties properties[] = {CL_CONTEXT_PLATFORM, 0x0, 0};
    auto context = createContext(properties);
    EXPECT_EQ(CL_INVALID_PLATFORM, context->initialize());
}

TEST_F(ContextImplFixture, givenDuplicatedPropertyWhenInitializingThenReturnsInvalidProperty) {
    const auto platformValue = reinterpret_cast<cl_context_properties>(static_cast<cl_platform_id>(platform));
    cl_context_properties properties[] = {CL_CONTEXT_PLATFORM, platformValue, CL_CONTEXT_PLATFORM, platformValue, 0};
    auto context = createContext(properties);
    EXPECT_EQ(CL_INVALID_PROPERTY, context->initialize());
}

TEST_F(ContextImplFixture, givenUnknownPropertyWhenInitializingThenReturnsInvalidProperty) {
    cl_context_properties properties[] = {0x4242, 1, 0};
    auto context = createContext(properties);
    EXPECT_EQ(CL_INVALID_PROPERTY, context->initialize());
}

TEST_F(ContextImplFixture, givenInteropUserSyncTrueWhenInitializingThenFlagIsEnabled) {
    cl_context_properties properties[] = {CL_CONTEXT_INTEROP_USER_SYNC, CL_TRUE, 0};
    auto context = createContext(properties);

    ASSERT_EQ(CL_SUCCESS, context->initialize());
    EXPECT_TRUE(context->getInteropUserSyncEnabled());
}

TEST_F(ContextImplFixture, givenInteropUserSyncFalseWhenInitializingThenFlagStaysDisabled) {
    cl_context_properties properties[] = {CL_CONTEXT_INTEROP_USER_SYNC, CL_FALSE, 0};
    auto context = createContext(properties);

    ASSERT_EQ(CL_SUCCESS, context->initialize());
    EXPECT_FALSE(context->getInteropUserSyncEnabled());
}

TEST_F(ContextImplFixture, givenOutOfRangeInteropUserSyncWhenInitializingThenReturnsInvalidProperty) {
    cl_context_properties properties[] = {CL_CONTEXT_INTEROP_USER_SYNC, 2, 0};
    auto context = createContext(properties);
    EXPECT_EQ(CL_INVALID_PROPERTY, context->initialize());
}

TEST_F(ContextImplFixture, givenNegativeInteropUserSyncWhenInitializingThenReturnsInvalidProperty) {
    cl_context_properties properties[] = {CL_CONTEXT_INTEROP_USER_SYNC, -1, 0};
    auto context = createContext(properties);
    EXPECT_EQ(CL_INVALID_PROPERTY, context->initialize());
}

TEST_F(ContextImplFixture, givenAcceptedPassThroughPropertiesWhenInitializingThenSucceeds) {
    cl_context_properties properties[] = {CL_CONTEXT_SHOW_DIAGNOSTICS_INTEL, 0, CL_L0_CONTEXT_HANDLE, 0, 0};
    auto context = createContext(properties);
    EXPECT_EQ(CL_SUCCESS, context->initialize());
}

TEST_F(ContextImplFixture, givenPropertiesWhenContextIsCreatedThenTheyAreStoredNullTerminated) {
    const auto platformValue = reinterpret_cast<cl_context_properties>(static_cast<cl_platform_id>(platform));
    cl_context_properties properties[] = {CL_CONTEXT_PLATFORM, platformValue, 0};
    auto context = createContext(properties);

    size_t retSize = 0;
    ASSERT_EQ(CL_SUCCESS, context->getInfo(CL_CONTEXT_PROPERTIES, 0, nullptr, &retSize));
    ASSERT_EQ(3u * sizeof(cl_context_properties), retSize);

    std::vector<cl_context_properties> stored(retSize / sizeof(cl_context_properties));
    ASSERT_EQ(CL_SUCCESS, context->getInfo(CL_CONTEXT_PROPERTIES, retSize, stored.data(), nullptr));
    EXPECT_EQ(static_cast<cl_context_properties>(CL_CONTEXT_PLATFORM), stored[0]);
    EXPECT_EQ(platformValue, stored[1]);
    EXPECT_EQ(0, stored[2]);
}

TEST_F(ContextImplFixture, givenNoPropertiesWhenQueryingStoredPropertiesThenNothingIsStored) {
    auto context = createContext();

    size_t retSize = 123u;
    EXPECT_EQ(CL_SUCCESS, context->getInfo(CL_CONTEXT_PROPERTIES, 0, nullptr, &retSize));
    EXPECT_EQ(0u, retSize);
}

TEST_F(ContextImplFixture, givenContextWhenQueryingNumDevicesThenMatchesDeviceList) {
    auto context = createContext();

    cl_uint numDevices = 0;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, context->getInfo(CL_CONTEXT_NUM_DEVICES, sizeof(numDevices), &numDevices, &retSize));
    EXPECT_EQ(sizeof(cl_uint), retSize);
    EXPECT_EQ(1u, numDevices);
    EXPECT_EQ(numDevices, static_cast<cl_uint>(context->getClDevices().size()));
}

TEST_F(ContextImplFixture, givenContextWhenQueryingDevicesThenReturnsOwningDevice) {
    auto context = createContext();

    cl_device_id queried = nullptr;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, context->getInfo(CL_CONTEXT_DEVICES, sizeof(queried), &queried, &retSize));
    EXPECT_EQ(sizeof(cl_device_id), retSize);
    EXPECT_EQ(clDeviceId, queried);
}

TEST_F(ContextImplFixture, givenContextWhenQueryingReferenceCountThenReturnsOne) {
    auto context = createContext();

    cl_uint refCount = 0;
    EXPECT_EQ(CL_SUCCESS, context->getInfo(CL_CONTEXT_REFERENCE_COUNT, sizeof(refCount), &refCount, nullptr));
    EXPECT_EQ(1u, refCount);
}

TEST_F(ContextImplFixture, givenContextWhenQueryingL0HandleThenReturnsUnderlyingContext) {
    auto context = createContext();

    ze_context_handle_t queried = nullptr;
    size_t retSize = 0;
    EXPECT_EQ(CL_SUCCESS, context->getInfo(CL_L0_CONTEXT_HANDLE, sizeof(queried), &queried, &retSize));
    EXPECT_EQ(sizeof(ze_context_handle_t), retSize);
    EXPECT_EQ(context->getL0ContextHandle(), queried);
}

TEST_F(ContextImplFixture, givenUnknownParamWhenQueryingContextInfoThenReturnsInvalidValue) {
    auto context = createContext();

    size_t retSize = 77u;
    EXPECT_EQ(CL_INVALID_VALUE, context->getInfo(0xDEAD0000u, 0, nullptr, &retSize));
    EXPECT_EQ(77u, retSize);
}

TEST_F(ContextImplFixture, givenTooSmallBufferWhenQueryingContextInfoThenReturnsInvalidValue) {
    auto context = createContext();

    cl_uint numDevices = 0;
    EXPECT_EQ(CL_INVALID_VALUE, context->getInfo(CL_CONTEXT_NUM_DEVICES, sizeof(cl_uint) - 1, &numDevices, nullptr));
}

TEST_F(ContextImplFixture, givenMatchingL0DeviceWhenFindClDeviceThenReturnsIt) {
    auto context = createContext();
    EXPECT_EQ(clDevice, context->findClDevice(clDevice->getL0Handle()));
}

TEST_F(ContextImplFixture, givenForeignL0DeviceWhenFindClDeviceThenReturnsNull) {
    auto context = createContext();
    EXPECT_EQ(nullptr, context->findClDevice(reinterpret_cast<ze_device_handle_t>(0x1234)));
}

TEST_F(ContextImplFixture, givenMatchingRootDeviceIndexWhenLookingUpDeviceThenReturnsIt) {
    auto context = createContext();
    EXPECT_EQ(clDevice, context->getClDeviceByRootDeviceIndex(clDevice->getRootDeviceIndex()));
}

TEST_F(ContextImplFixture, givenUnknownRootDeviceIndexWhenLookingUpDeviceThenReturnsNull) {
    auto context = createContext();
    EXPECT_EQ(nullptr, context->getClDeviceByRootDeviceIndex(clDevice->getRootDeviceIndex() + 100u));
}

TEST_F(ContextImplFixture, givenSingleDeviceContextThenItIsReportedAsSingleDevice) {
    auto context = createContext();

    EXPECT_TRUE(context->isSingleDeviceContext());
    EXPECT_EQ(1u, context->getRootDeviceIndices().size());
    EXPECT_EQ(clDevice, context->getClDevice());
}

TEST_F(ContextImplFixture, givenContextWhenCreatedThenDeviceBitfieldMatchesTheDevice) {
    auto context = createContext();

    const auto rootDeviceIndex = clDevice->getRootDeviceIndex();
    const auto &bitfields = context->getDeviceBitfields();
    ASSERT_EQ(1u, bitfields.count(rootDeviceIndex));
    EXPECT_EQ(clDevice->getDevice().getDeviceBitfield(), bitfields.at(rootDeviceIndex));
}

TEST_F(ContextImplFixture, givenNewContextThenExecutionIsNotTerminated) {
    auto context = createContext();
    EXPECT_FALSE(context->isTerminated());
}

TEST_F(ContextImplFixture, givenTerminatedExecutionThenContextReportsIt) {
    auto context = createContext();
    context->terminateExecution();
    EXPECT_TRUE(context->isTerminated());
}

TEST_F(ContextImplFixture, givenContextWhenCreatedThenDeviceReferenceIsHeldThroughInternalCount) {
    auto context = createContext();
    EXPECT_EQ(1, context->getReference());
}

struct ContextCallbackRecorder {
    static constexpr size_t capacity = 8u;
    static std::array<int, capacity> invocations;
    static size_t count;

    static void reset() {
        invocations.fill(0);
        count = 0u;
    }

    template <int id>
    static void CL_CALLBACK callback(cl_context, void *userData) {
        if (count < capacity) {
            invocations[count++] = id;
        }
        if (userData != nullptr) {
            *static_cast<int *>(userData) = id;
        }
    }
};

std::array<int, ContextCallbackRecorder::capacity> ContextCallbackRecorder::invocations{};
size_t ContextCallbackRecorder::count = 0u;

TEST_F(ContextImplFixture, givenRegisteredCallbacksWhenContextIsDestroyedThenTheyRunInReverseOrder) {
    ContextCallbackRecorder::reset();

    auto context = createContext();
    context->registerCallback(&ContextCallbackRecorder::callback<1>, nullptr);
    context->registerCallback(&ContextCallbackRecorder::callback<2>, nullptr);
    context->registerCallback(&ContextCallbackRecorder::callback<3>, nullptr);

    EXPECT_EQ(0u, ContextCallbackRecorder::count);

    context.reset();

    ASSERT_EQ(3u, ContextCallbackRecorder::count);
    EXPECT_EQ(3, ContextCallbackRecorder::invocations[0]);
    EXPECT_EQ(2, ContextCallbackRecorder::invocations[1]);
    EXPECT_EQ(1, ContextCallbackRecorder::invocations[2]);
}

TEST_F(ContextImplFixture, givenRegisteredCallbackWithUserDataWhenContextIsDestroyedThenUserDataIsForwarded) {
    ContextCallbackRecorder::reset();
    int userData = 0;

    auto context = createContext();
    context->registerCallback(&ContextCallbackRecorder::callback<7>, &userData);
    context.reset();

    EXPECT_EQ(7, userData);
}

TEST_F(ContextImplFixture, givenNoRegisteredCallbacksWhenContextIsDestroyedThenNothingIsInvoked) {
    ContextCallbackRecorder::reset();

    auto context = createContext();
    context.reset();

    EXPECT_EQ(0u, ContextCallbackRecorder::count);
}

TEST_F(ContextImplFixture, givenContextWhenObtainingRegularEventThenHandleIsProvidedAndCanBeReturned) {
    auto context = createContext();

    auto event = context->obtainRegularEvent(false);
    EXPECT_NE(nullptr, event);
    context->returnRegularEvent(event, false);
}

TEST_F(ContextImplFixture, givenContextWhenObtainingSeveralEventsThenEachHandleIsDistinct) {
    auto context = createContext();

    auto first = context->obtainRegularEvent(false);
    auto second = context->obtainRegularEvent(false);

    EXPECT_NE(nullptr, first);
    EXPECT_NE(nullptr, second);
    EXPECT_NE(first, second);

    context->returnRegularEvent(first, false);
    context->returnRegularEvent(second, false);
}

TEST_F(ContextImplFixture, givenContextWhenObtainingTimestampEventThenItIsDistinctFromRegularPool) {
    auto context = createContext();

    auto regularEvent = context->obtainRegularEvent(false);
    auto timestampEvent = context->obtainRegularEvent(true);

    EXPECT_NE(nullptr, regularEvent);
    EXPECT_NE(nullptr, timestampEvent);
    EXPECT_NE(regularEvent, timestampEvent);

    context->returnRegularEvent(regularEvent, false);
    context->returnRegularEvent(timestampEvent, true);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
