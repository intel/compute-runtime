/*
 * Copyright (C) 2019-2022 Intel Corporation
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

using namespace NEO;

TEST(UnifiedSharingTests, givenContextCreatedWithExternalDeviceHandlePropertyWhenGettingUnifiedSharingThenReturnIt) {
    MockClDevice device{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())};
    cl_device_id deviceId = &device;
    ClDeviceVector allDevs(&deviceId, 1);
    cl_int retVal{};

    const cl_context_properties contextProps[] = {
        static_cast<cl_context_properties>(UnifiedSharingContextType::DeviceHandle), 0,
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
    cl_context_properties propertyType = static_cast<cl_context_properties>(UnifiedSharingContextType::DeviceHandle);
    cl_context_properties propertyValue = 0x1234;
    bool result = builder.processProperties(propertyType, propertyValue);
    EXPECT_TRUE(result);
    EXPECT_NE(nullptr, builder.contextData);
}

TEST(UnifiedSharingTests, givenExternalDeviceGroupHandleWhenProcessingBySharingContextBuilderThenResultIsTrue) {
    MockUnifiedSharingContextBuilder builder{};
    cl_context_properties propertyType = static_cast<cl_context_properties>(UnifiedSharingContextType::DeviceGroup);
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
        void releaseResource(MemObj *memObject, uint32_t rootDeviceIndex) override {
            UnifiedSharing::releaseResource(memObject, rootDeviceIndex);
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

        GraphicsAllocation *createGraphicsAllocationFromNTHandle(void *handle, uint32_t rootDeviceIndex, AllocationType allocType) override {
            this->createFromNTHandleCalled = true;
            this->handle = toOsHandle(handle);
            return nullptr;
        }
        GraphicsAllocation *createGraphicsAllocationFromSharedHandle(osHandle handle, const AllocationProperties &properties, bool requireSpecificBitness, bool isHostIpcAllocation, bool reuseSharedAllocation) override {
            this->createFromSharedHandleCalled = true;
            this->handle = handle;
            this->properties = std::make_unique<AllocationProperties>(properties);
            return nullptr;
        }

        bool createFromNTHandleCalled = false;
        bool createFromSharedHandleCalled = false;
        osHandle handle;
        std::unique_ptr<AllocationProperties> properties;
    };

    struct MockSharingHandler : UnifiedSharing {
        using UnifiedSharing::createGraphicsAllocation;
    };

    void SetUp() override {
        UnifiedSharingTestsWithMemoryManager::SetUp();
        this->memoryManager = std::make_unique<MemoryManagerCheckingAllocationMethod>();
        this->memoryManagerBackup = std::make_unique<VariableBackup<MemoryManager *>>(&this->context->memoryManager, this->memoryManager.get());
    }

    std::unique_ptr<MemoryManagerCheckingAllocationMethod> memoryManager;
    std::unique_ptr<VariableBackup<MemoryManager *>> memoryManagerBackup;
};

TEST_F(UnifiedSharingCreateAllocationTests, givenWindowsNtHandleWhenCreateGraphicsAllocationIsCalledThenUseNtHandleMethod) {
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    desc.type = UnifiedSharingHandleType::Win32Nt;
    AllocationType allocationType = AllocationType::SHARED_IMAGE;
    MockSharingHandler::createGraphicsAllocation(this->context.get(), desc, allocationType);

    EXPECT_TRUE(memoryManager->createFromNTHandleCalled);
    EXPECT_FALSE(memoryManager->createFromSharedHandleCalled);
    EXPECT_EQ(toOsHandle(desc.handle), memoryManager->handle);
}

TEST_F(UnifiedSharingCreateAllocationTests, givenWindowsSharedHandleWhenCreateGraphicsAllocationIsCalledThenUseSharedHandleMethod) {
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    desc.type = UnifiedSharingHandleType::Win32Shared;
    AllocationType allocationType = AllocationType::SHARED_IMAGE;
    MockSharingHandler::createGraphicsAllocation(this->context.get(), desc, allocationType);

    EXPECT_FALSE(memoryManager->createFromNTHandleCalled);
    EXPECT_TRUE(memoryManager->createFromSharedHandleCalled);
    EXPECT_EQ(toOsHandle(desc.handle), memoryManager->handle);
    const AllocationProperties expectedProperties{0u, false, 0u, allocationType, false, {}};
    EXPECT_EQ(expectedProperties.allFlags, memoryManager->properties->allFlags);
}

TEST_F(UnifiedSharingCreateAllocationTests, givenLinuxSharedHandleWhenCreateGraphicsAllocationIsCalledThenUseSharedHandleMethod) {
    UnifiedSharingMemoryDescription desc{};
    desc.handle = reinterpret_cast<void *>(0x1234);
    desc.type = UnifiedSharingHandleType::LinuxFd;
    AllocationType allocationType = AllocationType::SHARED_IMAGE;
    MockSharingHandler::createGraphicsAllocation(this->context.get(), desc, allocationType);

    EXPECT_FALSE(memoryManager->createFromNTHandleCalled);
    EXPECT_TRUE(memoryManager->createFromSharedHandleCalled);
    EXPECT_EQ(toOsHandle(desc.handle), memoryManager->handle);
    const AllocationProperties expectedProperties{0u, false, 0u, allocationType, false, {}};
    EXPECT_EQ(expectedProperties.allFlags, memoryManager->properties->allFlags);
}
