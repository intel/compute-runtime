/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/sharings/unified/enable_unified.h"
#include "opencl/source/sharings/unified/unified_buffer.h"
#include "opencl/source/sharings/unified/unified_sharing.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/sharings/unified/unified_sharing_fixtures.h"

namespace NEO {
class MemoryManager;
} // namespace NEO

using namespace NEO;

TEST(UnifiedSharingTests, givenContextCreatedWithExternalDeviceHandlePropertyWhenGettingUnifiedSharingThenReturnIt) {
    MockClDevice device{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())};
    cl_device_id deviceId = &device;
    ClDeviceVector allDevs(&deviceId, 1);
    cl_int retVal{};

    const cl_context_properties contextProps[] = {
        static_cast<cl_context_properties>(UnifiedSharingContextType::deviceHandle), 0,
        CL_CONTEXT_INTEROP_USER_SYNC, 1,
        0};

    auto context = std::unique_ptr<MockContext>(Context::create<MockContext>(contextProps, allDevs,
                                                                             nullptr, nullptr, retVal));
    auto sharingFunctions = context->getSharing<UnifiedSharingFunctions>();
    EXPECT_NE(nullptr, sharingFunctions);
}

struct MockUnifiedSharingContextBuilder : UnifiedSharingContextBuilder {
    using UnifiedSharingContextBuilder::contextData;
};

TEST(UnifiedSharingTests, givenExternalDeviceHandleWhenProcessingBySharingContextBuilderThenResultIsTrue) {
    MockUnifiedSharingContextBuilder builder{};
    cl_context_properties propertyType = static_cast<cl_context_properties>(UnifiedSharingContextType::deviceHandle);
    cl_context_properties propertyValue = 0x1234;
    bool result = builder.processProperties(propertyType, propertyValue);
    EXPECT_TRUE(result);
    EXPECT_NE(nullptr, builder.contextData);
}

TEST(UnifiedSharingTests, givenExternalDeviceGroupHandleWhenProcessingBySharingContextBuilderThenResultIsTrue) {
    MockUnifiedSharingContextBuilder builder{};
    cl_context_properties propertyType = static_cast<cl_context_properties>(UnifiedSharingContextType::deviceGroup);
    cl_context_properties propertyValue = 0x1234;
    bool result = builder.processProperties(propertyType, propertyValue);
    EXPECT_TRUE(result);
    EXPECT_NE(nullptr, builder.contextData);
}

TEST(UnifiedSharingTests, givenExternalDeviceGroupHandleWhenProcessingBySharingContextBuilderThenReturnSuccess) {
    MockUnifiedSharingContextBuilder builder{};
    cl_context_properties propertyType = CL_CONTEXT_PLATFORM;
    cl_context_properties propertyValue = 0x1234;
    bool result = builder.processProperties(propertyType, propertyValue);
    EXPECT_FALSE(result);
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
    UnifiedSharing sharingHandler{&sharingFunctions, UnifiedSharingHandleType::linuxFd};
    EXPECT_EQ(&sharingFunctions, sharingHandler.peekFunctionsHandler());
    EXPECT_EQ(UnifiedSharingHandleType::linuxFd, sharingHandler.getExternalMemoryType());
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
        void releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) override {
            UnifiedSharing::releaseResource(memObject, rootDeviceIndex);
            releaseResourceCalled++;
        };
    };

    cl_mem_flags flags{};
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    desc.type = UnifiedSharingHandleType::win32Nt;
    cl_int retVal{};
    auto buffer = std::unique_ptr<Buffer>(UnifiedBuffer::createSharedUnifiedBuffer(context.get(), flags, desc, &retVal));
    ASSERT_EQ(CL_SUCCESS, retVal);

    UnifiedSharingFunctions sharingFunctions;
    MockSharingHandler *sharingHandler = new MockSharingHandler(&sharingFunctions, desc.type);
    buffer->setSharingHandler(sharingHandler);

    ASSERT_EQ(0u, sharingHandler->synchronizeObjectCalled);

    ASSERT_EQ(CL_SUCCESS, sharingHandler->acquire(buffer.get(), context->getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(1u, sharingHandler->synchronizeObjectCalled);

    ASSERT_EQ(CL_SUCCESS, sharingHandler->acquire(buffer.get(), context->getDevice(0)->getRootDeviceIndex()));
    EXPECT_EQ(1u, sharingHandler->synchronizeObjectCalled);

    ASSERT_EQ(0u, sharingHandler->releaseResourceCalled);
    sharingHandler->release(buffer.get(), context->getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(0u, sharingHandler->releaseResourceCalled);
    sharingHandler->release(buffer.get(), context->getDevice(0)->getRootDeviceIndex());
    EXPECT_EQ(1u, sharingHandler->releaseResourceCalled);
}

struct UnifiedSharingCreateAllocationTests : UnifiedSharingTestsWithMemoryManager {
    struct MemoryManagerCheckingAllocationMethod : MockMemoryManager {
        using MockMemoryManager::MockMemoryManager;

        GraphicsAllocation *createGraphicsAllocationFromSharedHandle(const OsHandleData &osHandleData, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation, void *mapPointer) override {
            this->createFromSharedHandleCalled = true;
            this->properties = std::make_unique<AllocationProperties>(properties);
            this->handle = osHandleData.handle;
            return nullptr;
        }

        bool createFromSharedHandleCalled = false;
        osHandle handle;
        std::unique_ptr<AllocationProperties> properties;
    };

    struct MockSharingHandler : UnifiedSharing {
        using UnifiedSharing::createMultiGraphicsAllocation;
    };

    void SetUp() override {
        UnifiedSharingTestsWithMemoryManager::SetUp();
        this->memoryManager = std::make_unique<MemoryManagerCheckingAllocationMethod>();
        this->memoryManagerBackup = std::make_unique<VariableBackup<MemoryManager *>>(&this->context->memoryManager, this->memoryManager.get());
    }

    std::unique_ptr<MemoryManagerCheckingAllocationMethod> memoryManager;
    std::unique_ptr<VariableBackup<MemoryManager *>> memoryManagerBackup;
};

TEST_F(UnifiedSharingCreateAllocationTests, givenWindowsNtHandleWhenCreateGraphicsAllocationIsCalledThenUseSharedHandleMethod) {
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    desc.type = UnifiedSharingHandleType::win32Nt;
    AllocationType allocationType = AllocationType::sharedImage;
    MockSharingHandler::createMultiGraphicsAllocation(this->context.get(), desc, nullptr, allocationType, nullptr);

    EXPECT_TRUE(memoryManager->createFromSharedHandleCalled);
    EXPECT_EQ(toOsHandle(desc.handle), memoryManager->handle);
}

TEST_F(UnifiedSharingCreateAllocationTests, givenWindowsSharedHandleWhenCreateGraphicsAllocationIsCalledThenUseSharedHandleMethod) {
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    desc.type = UnifiedSharingHandleType::win32Shared;
    AllocationType allocationType = AllocationType::sharedImage;
    MockSharingHandler::createMultiGraphicsAllocation(this->context.get(), desc, nullptr, allocationType, nullptr);

    EXPECT_TRUE(memoryManager->createFromSharedHandleCalled);
    EXPECT_EQ(toOsHandle(desc.handle), memoryManager->handle);
    const AllocationProperties expectedProperties{0u, false, 0u, allocationType, false, {}};
    EXPECT_EQ(expectedProperties.allFlags, memoryManager->properties->allFlags);
}

TEST_F(UnifiedSharingCreateAllocationTests, givenLinuxSharedHandleWhenCreateGraphicsAllocationIsCalledThenUseSharedHandleMethod) {
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    desc.type = UnifiedSharingHandleType::linuxFd;
    AllocationType allocationType = AllocationType::sharedImage;
    MockSharingHandler::createMultiGraphicsAllocation(this->context.get(), desc, nullptr, allocationType, nullptr);

    EXPECT_TRUE(memoryManager->createFromSharedHandleCalled);
    EXPECT_EQ(toOsHandle(desc.handle), memoryManager->handle);
    const AllocationProperties expectedProperties{0u, false, 0u, allocationType, false, {}};
    EXPECT_EQ(expectedProperties.allFlags, memoryManager->properties->allFlags);
}
