/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/sharings/unified/enable_unified.h"
#include "runtime/sharings/unified/unified_buffer.h"
#include "runtime/sharings/unified/unified_sharing.h"
#include "test.h"
#include "unit_tests/helpers/variable_backup.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_memory_manager.h"
#include "unit_tests/sharings/unified/unified_sharing_fixtures.h"

using namespace NEO;

TEST(UnifiedSharingTests, givenContextCreatedWithExternalDeviceHandlePropertyWhenGettingUnifiedSharingThenReturnIt) {
    MockDevice device{};
    cl_device_id deviceId = static_cast<cl_device_id>(&device);
    DeviceVector allDevs(&deviceId, 1);
    cl_int retVal{};

    const cl_context_properties context_props[] = {
        static_cast<cl_context_properties>(UnifiedSharingContextType::DeviceHandle), 0,
        CL_CONTEXT_INTEROP_USER_SYNC, 1,
        0};

    auto context = std::unique_ptr<MockContext>(Context::create<MockContext>(context_props, allDevs,
                                                                             nullptr, nullptr, retVal));
    auto sharingFunctions = context->getSharing<UnifiedSharingFunctions>();
    EXPECT_NE(nullptr, sharingFunctions);
}

struct MockUnifiedSharingContextBuilder : UnifiedSharingContextBuilder {
    using UnifiedSharingContextBuilder::contextData;
};

TEST(UnifiedSharingTests, givenExternalDeviceHandleWhenProcessingBySharingContextBuilderThenResultIsTrue) {
    MockUnifiedSharingContextBuilder builder{};
    cl_context_properties propertyType = static_cast<cl_context_properties>(UnifiedSharingContextType::DeviceHandle);
    cl_context_properties propertyValue = 0x1234;
    cl_int retVal{};
    bool result = builder.processProperties(propertyType, propertyValue, retVal);
    EXPECT_TRUE(result);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, builder.contextData);
}

TEST(UnifiedSharingTests, givenExternalDeviceGroupHandleWhenProcessingBySharingContextBuilderThenResultIsTrue) {
    MockUnifiedSharingContextBuilder builder{};
    cl_context_properties propertyType = static_cast<cl_context_properties>(UnifiedSharingContextType::DeviceGroup);
    cl_context_properties propertyValue = 0x1234;
    cl_int retVal{};
    bool result = builder.processProperties(propertyType, propertyValue, retVal);
    EXPECT_TRUE(result);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, builder.contextData);
}

TEST(UnifiedSharingTests, givenExternalDeviceGroupHandleWhenProcessingBySharingContextBuilderThenReturnSuccess) {
    MockUnifiedSharingContextBuilder builder{};
    cl_context_properties propertyType = CL_CONTEXT_PLATFORM;
    cl_context_properties propertyValue = 0x1234;
    cl_int retVal{};
    bool result = builder.processProperties(propertyType, propertyValue, retVal);
    EXPECT_FALSE(result);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, builder.contextData);
}

TEST(UnifiedSharingTests, givenContextWithUserSyncWhenFinalizingPropertiesBySharingContextBuilderThenRegisterSharingInContextAndClearContextData) {
    MockUnifiedSharingContextBuilder builder{};
    builder.contextData = std::make_unique<UnifiedCreateContextProperties>();

    MockContext context{};
    context.setInteropUserSyncEnabled(true);
    cl_int retVal{};
    bool result = builder.finalizeProperties(context, retVal);
    EXPECT_TRUE(result);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context.sharingFunctions[UnifiedSharingFunctions::sharingId]);
    EXPECT_EQ(nullptr, builder.contextData);
}

TEST(UnifiedSharingTests, givenContextWithoutUserSyncWhenFinalizingPropertiesBySharingContextBuilderThenDoNotRegisterSharingInContextAndClearContextData) {
    MockUnifiedSharingContextBuilder builder{};
    builder.contextData = std::make_unique<UnifiedCreateContextProperties>();

    MockContext context{};
    context.setInteropUserSyncEnabled(false);
    cl_int retVal{};
    bool result = builder.finalizeProperties(context, retVal);
    EXPECT_TRUE(result);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, context.sharingFunctions[UnifiedSharingFunctions::sharingId]);
    EXPECT_EQ(nullptr, builder.contextData);
}

TEST(UnifiedSharingTests, givenBuilderWithoutContextDataWhenFinalizingPropertiesBySharingContextBuilderThenDoNotRegisterSharingInContext) {
    MockUnifiedSharingContextBuilder builder{};

    MockContext context{};
    cl_int retVal{};
    bool result = builder.finalizeProperties(context, retVal);
    EXPECT_TRUE(result);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, context.sharingFunctions[UnifiedSharingFunctions::sharingId]);
    EXPECT_EQ(nullptr, builder.contextData);
}

TEST(UnifiedSharingTests, givenSharingHandlerThenItReturnsCorrectValues) {
    UnifiedSharingFunctions sharingFunctions;
    EXPECT_EQ(UnifiedSharingFunctions::sharingId, sharingFunctions.getId());
    UnifiedSharing sharingHandler{&sharingFunctions, UnifiedSharingHandleType::LinuxFd};
    EXPECT_EQ(&sharingFunctions, sharingHandler.peekFunctionsHandler());
    EXPECT_EQ(UnifiedSharingHandleType::LinuxFd, sharingHandler.getExternalMemoryType());
}

using UnifiedSharingTestsWithMemoryManager = UnifiedSharingFixture<true, true>;

TEST_F(UnifiedSharingTestsWithMemoryManager, givenUnifiedSharingHandlerWhenAcquiringAndReleasingThenMethodsAreCalledAppropriately) {
    struct MockSharingHandler : UnifiedSharing {
        using UnifiedSharing::UnifiedSharing;
        unsigned int synchronizeObjectCalled = 0u;
        unsigned int releaseResourceCalled = 0u;
        void synchronizeObject(UpdateData &updateData) override {
            UnifiedSharing::synchronizeObject(updateData);
            synchronizeObjectCalled++;
        }
        void releaseResource(MemObj *memObject) override {
            UnifiedSharing::releaseResource(memObject);
            releaseResourceCalled++;
        };
    };

    cl_mem_flags flags{};
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    desc.type = UnifiedSharingHandleType::Win32Nt;
    cl_int retVal{};
    auto buffer = std::unique_ptr<Buffer>(UnifiedBuffer::createSharedUnifiedBuffer(context.get(), flags, desc, &retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    UnifiedSharingFunctions sharingFunctions;
    MockSharingHandler *sharingHandler = new MockSharingHandler(&sharingFunctions, desc.type);
    buffer->setSharingHandler(sharingHandler);

    ASSERT_EQ(0u, sharingHandler->synchronizeObjectCalled);

    ASSERT_EQ(CL_SUCCESS, sharingHandler->acquire(buffer.get()));
    EXPECT_EQ(1u, sharingHandler->synchronizeObjectCalled);

    ASSERT_EQ(CL_SUCCESS, sharingHandler->acquire(buffer.get()));
    EXPECT_EQ(1u, sharingHandler->synchronizeObjectCalled);

    ASSERT_EQ(0u, sharingHandler->releaseResourceCalled);
    sharingHandler->release(buffer.get());
    EXPECT_EQ(0u, sharingHandler->releaseResourceCalled);
    sharingHandler->release(buffer.get());
    EXPECT_EQ(1u, sharingHandler->releaseResourceCalled);
}
