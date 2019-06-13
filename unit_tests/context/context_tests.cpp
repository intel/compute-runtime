/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_queue/command_queue.h"
#include "runtime/context/context.inl"
#include "runtime/device/device.h"
#include "runtime/device_queue/device_queue.h"
#include "runtime/helpers/options.h"
#include "runtime/sharings/sharing.h"
#include "unit_tests/fixtures/platform_fixture.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_deferred_deleter.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_memory_manager.h"

#include "gtest/gtest.h"

using namespace NEO;

class WhiteBoxContext : public Context {
  public:
    MemoryManager *getMM() {
        return this->memoryManager;
    }

    const cl_context_properties *getProperties() const {
        return properties;
    }

    size_t getNumProperties() const {
        return numProperties;
    }

    WhiteBoxContext(void(CL_CALLBACK *pfnNotify)(const char *, const void *, size_t, void *), void *userData) : Context(pfnNotify, userData){};
};

struct ContextTest : public PlatformFixture,
                     public ::testing::Test {

    using PlatformFixture::SetUp;

    void SetUp() override {
        PlatformFixture::SetUp();

        cl_platform_id platform = pPlatform;
        properties = new cl_context_properties[3];
        properties[0] = CL_CONTEXT_PLATFORM;
        properties[1] = (cl_context_properties)platform;
        properties[2] = 0;

        context = Context::create<WhiteBoxContext>(properties, DeviceVector(devices, num_devices), nullptr, nullptr, retVal);
        ASSERT_NE(nullptr, context);
    }

    void TearDown() override {
        delete[] properties;
        delete context;
        PlatformFixture::TearDown();
    }

    cl_int retVal = CL_SUCCESS;
    WhiteBoxContext *context = nullptr;
    cl_context_properties *properties = nullptr;
};

TEST_F(ContextTest, TestCtor) {
    EXPECT_EQ(numPlatformDevices, context->getNumDevices());
    for (size_t deviceOrdinal = 0; deviceOrdinal < context->getNumDevices(); ++deviceOrdinal) {
        EXPECT_NE(nullptr, context->getDevice(deviceOrdinal));
    }
}

TEST_F(ContextTest, MemoryManager) {
    EXPECT_NE(nullptr, context->getMM());
}

TEST_F(ContextTest, propertiesShouldBeCopied) {
    auto contextProperties = context->getProperties();
    EXPECT_NE(properties, contextProperties);
}

TEST_F(ContextTest, propertiesShouldBeValid) {
    auto contextProperties = context->getProperties();
    ASSERT_NE(nullptr, contextProperties);
    EXPECT_EQ(3u, context->getNumProperties());

    while (*contextProperties) {
        switch (*contextProperties) {
        case CL_CONTEXT_PLATFORM:
            ++contextProperties;
            break;
        default:
            ASSERT_FALSE(!"Unknown context property");
            break;
        }
        ++contextProperties;
    }
}

TEST_F(ContextTest, specialQueue) {
    auto specialQ = context->getSpecialQueue();
    EXPECT_NE(specialQ, nullptr);
}

TEST_F(ContextTest, setSpecialQueue) {
    MockContext context((Device *)devices[0], true);

    auto specialQ = context.getSpecialQueue();
    EXPECT_EQ(specialQ, nullptr);

    auto cmdQ = new CommandQueue(&context, (Device *)devices[0], 0);
    context.setSpecialQueue(cmdQ);
    specialQ = context.getSpecialQueue();
    EXPECT_NE(specialQ, nullptr);
}

TEST_F(ContextTest, defaultQueue) {
    EXPECT_EQ(nullptr, context->getDefaultDeviceQueue());
    auto dq = new DeviceQueue();
    context->setDefaultDeviceQueue(dq);
    EXPECT_EQ(dq, context->getDefaultDeviceQueue());
    delete dq;
}

TEST_F(ContextTest, givenCmdQueueWithoutContextWhenBeingCreatedNextDeletedThenContextRefCountShouldNeitherBeIncrementedNorNextDecremented) {
    MockContext context((Device *)devices[0]);
    EXPECT_EQ(1, context.getRefInternalCount());

    auto cmdQ1 = new CommandQueue();
    EXPECT_EQ(1, context.getRefInternalCount());

    delete cmdQ1;
    EXPECT_EQ(1, context.getRefInternalCount());

    auto cmdQ2 = new CommandQueue(nullptr, (Device *)devices[0], 0);
    EXPECT_EQ(1, context.getRefInternalCount());

    delete cmdQ2;
    EXPECT_EQ(1, context.getRefInternalCount());
}

TEST_F(ContextTest, givenDeviceQueueWithoutContextWhenBeingCreatedNextDeletedThenContextRefCountShouldNeitherBeIncrementedNorNextDecremented) {
    MockContext context((Device *)devices[0]);
    EXPECT_EQ(1, context.getRefInternalCount());

    auto cmdQ1 = new DeviceQueue();
    EXPECT_EQ(1, context.getRefInternalCount());

    delete cmdQ1;
    EXPECT_EQ(1, context.getRefInternalCount());

    cl_queue_properties properties = 0;
    auto cmdQ2 = new DeviceQueue(nullptr, (Device *)devices[0], properties);
    EXPECT_EQ(1, context.getRefInternalCount());

    delete cmdQ2;
    EXPECT_EQ(1, context.getRefInternalCount());
}

TEST_F(ContextTest, givenCmdQueueWithContextWhenBeingCreatedNextDeletedThenContextRefCountShouldBeIncrementedNextDecremented) {
    MockContext context((Device *)devices[0]);
    EXPECT_EQ(1, context.getRefInternalCount());

    auto cmdQ = new CommandQueue(&context, (Device *)devices[0], 0);
    EXPECT_EQ(2, context.getRefInternalCount());

    delete cmdQ;
    EXPECT_EQ(1, context.getRefInternalCount());
}

TEST_F(ContextTest, givenDeviceCmdQueueWithContextWhenBeingCreatedNextDeletedThenContextRefCountShouldBeIncrementedNextDecremented) {
    MockContext context((Device *)devices[0]);
    EXPECT_EQ(1, context.getRefInternalCount());

    cl_queue_properties properties = 0;
    auto cmdQ = new DeviceQueue(&context, (Device *)devices[0], properties);
    EXPECT_EQ(2, context.getRefInternalCount());

    delete cmdQ;
    EXPECT_EQ(1, context.getRefInternalCount());
}

TEST_F(ContextTest, givenDefaultDeviceCmdQueueWithContextWhenBeingCreatedNextDeletedThenContextRefCountShouldBeIncrementedNextDecremented) {
    MockContext context((Device *)devices[0]);
    EXPECT_EQ(1, context.getRefInternalCount());

    cl_queue_properties properties = 0;
    auto cmdQ = new DeviceQueue(&context, (Device *)devices[0], properties);
    context.setDefaultDeviceQueue(cmdQ);
    EXPECT_EQ(2, context.getRefInternalCount());

    delete cmdQ;
    EXPECT_EQ(1, context.getRefInternalCount());
}

TEST_F(ContextTest, givenContextWhenItIsCreatedFromDeviceThenItAddsRefCountToThisDevice) {
    auto device = castToObject<Device>(devices[0]);
    EXPECT_EQ(2, device->getRefInternalCount());
    cl_device_id deviceID = devices[0];
    std::unique_ptr<Context> context(Context::create<Context>(0, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(3, device->getRefInternalCount());
    context.reset(nullptr);
    EXPECT_EQ(2, device->getRefInternalCount());
}

TEST_F(ContextTest, givenContextWhenItIsCreatedFromMultipleDevicesThenItAddsRefCountToThoseDevices) {
    auto device = castToObject<Device>(devices[0]);
    EXPECT_EQ(2, device->getRefInternalCount());

    DeviceVector devicesVector;
    devicesVector.push_back(device);
    devicesVector.push_back(device);

    std::unique_ptr<Context> context(Context::create<Context>(0, devicesVector, nullptr, nullptr, retVal));
    EXPECT_EQ(4, device->getRefInternalCount());
    context.reset(nullptr);
    EXPECT_EQ(2, device->getRefInternalCount());
}

TEST_F(ContextTest, givenSpecialCmdQueueWithContextWhenBeingCreatedNextAutoDeletedThenContextRefCountShouldNeitherBeIncrementedNorNextDecremented) {
    MockContext context((Device *)devices[0], true);
    EXPECT_EQ(1, context.getRefInternalCount());

    auto cmdQ = new CommandQueue(&context, (Device *)devices[0], 0);
    context.overrideSpecialQueueAndDecrementRefCount(cmdQ);
    EXPECT_EQ(1, context.getRefInternalCount());

    //special queue is to be deleted implicitly by context
}

TEST_F(ContextTest, givenSpecialCmdQueueWithContextWhenBeingCreatedNextDeletedThenContextRefCountShouldNeitherBeIncrementedNorNextDecremented) {
    MockContext context((Device *)devices[0], true);
    EXPECT_EQ(1, context.getRefInternalCount());

    auto cmdQ = new CommandQueue(&context, (Device *)devices[0], 0);
    context.overrideSpecialQueueAndDecrementRefCount(cmdQ);
    EXPECT_EQ(1, context.getRefInternalCount());

    delete cmdQ;
    EXPECT_EQ(1, context.getRefInternalCount());

    context.setSpecialQueue(nullptr);
}

TEST_F(ContextTest, GivenInteropSyncParamWhenCreateContextThenSetContextParam) {
    cl_device_id deviceID = devices[0];
    auto pPlatform = NEO::platform();
    cl_platform_id pid[1];
    pid[0] = pPlatform;

    cl_context_properties validProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0],
                                                CL_CONTEXT_INTEROP_USER_SYNC, 1, 0};
    cl_int retVal = CL_SUCCESS;
    auto context = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_TRUE(context->getInteropUserSyncEnabled());
    delete context;

    validProperties[3] = 0; // false
    context = Context::create<Context>(validProperties, DeviceVector(&deviceID, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_FALSE(context->getInteropUserSyncEnabled());
    delete context;
}

class MockSharingFunctions : public SharingFunctions {
  public:
    uint32_t getId() const override {
        return sharingId;
    }
    static const uint32_t sharingId = 0;
};

TEST_F(ContextTest, givenContextWhenSharingTableEmptyThenReturnsNullptr) {
    MockContext context;
    context.clearSharingFunctions();
    auto *sharingF = context.getSharing<MockSharingFunctions>();
    EXPECT_EQ(sharingF, nullptr);
}

TEST_F(ContextTest, givenNullptrWhenRegisteringSharingToContextThenAbortExecution) {
    MockContext context;
    context.clearSharingFunctions();
    EXPECT_THROW(context.registerSharing<MockSharingFunctions>(nullptr), std::exception);
}

TEST_F(ContextTest, givenContextWhenSharingTableIsNotEmptyThenReturnsSharingFunctionPointer) {
    MockContext context;
    MockSharingFunctions *sharingFunctions = new MockSharingFunctions;
    context.registerSharing<MockSharingFunctions>(sharingFunctions);
    auto *sharingF = context.getSharing<MockSharingFunctions>();
    EXPECT_EQ(sharingF, sharingFunctions);
}

TEST(Context, givenFtrSvmFalseWhenContextIsCreatedThenSVMAllocsManagerIsNotCreated) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    auto hwInfo = executionEnvironment->getMutableHardwareInfo();
    hwInfo->capabilityTable.ftrSvm = false;

    std::unique_ptr<MockDevice> device(MockDevice::createWithExecutionEnvironment<MockDevice>(hwInfo, executionEnvironment, 0));

    cl_device_id clDevice = device.get();
    cl_int retVal = CL_SUCCESS;
    auto context = std::unique_ptr<MockContext>(Context::create<MockContext>(nullptr, DeviceVector(&clDevice, 1), nullptr, nullptr, retVal));
    ASSERT_NE(nullptr, context);
    auto svmManager = context->getSVMAllocsManager();
    EXPECT_EQ(nullptr, svmManager);
}

class ContextWithAsyncDeleterTest : public ::testing::WithParamInterface<bool>,
                                    public ::testing::Test {
  public:
    void SetUp() override {
        memoryManager = new MockMemoryManager();
        device = new MockDevice;
        deleter = new MockDeferredDeleter();
        device->injectMemoryManager(memoryManager);
        memoryManager->setDeferredDeleter(deleter);
    }
    void TearDown() override {
        delete device;
    }
    Context *context;
    MockMemoryManager *memoryManager;
    MockDeferredDeleter *deleter;
    MockDevice *device;
};

TEST_P(ContextWithAsyncDeleterTest, givenContextWithMemoryManagerWhenAsyncDeleterIsEnabledThenUsesDeletersMethods) {
    cl_device_id clDevice = static_cast<cl_device_id>(device);
    cl_int retVal;
    DeviceVector deviceVector((cl_device_id *)&clDevice, 1);
    bool asyncDeleterEnabled = GetParam();
    memoryManager->overrideAsyncDeleterFlag(asyncDeleterEnabled);

    EXPECT_EQ(0, deleter->getClientsNum());
    context = Context::create<Context>(0, deviceVector, nullptr, nullptr, retVal);

    if (asyncDeleterEnabled) {
        EXPECT_EQ(1, deleter->getClientsNum());
    } else {
        EXPECT_EQ(0, deleter->getClientsNum());
    }
    delete context;

    EXPECT_EQ(0, deleter->getClientsNum());
}

INSTANTIATE_TEST_CASE_P(ContextTests,
                        ContextWithAsyncDeleterTest,
                        ::testing::Bool());

TEST(DefaultContext, givenDefaultContextWhenItIsQueriedForTypeThenDefaultTypeIsReturned) {
    MockContext context;
    EXPECT_EQ(ContextType::CONTEXT_TYPE_DEFAULT, context.peekContextType());
}

TEST(Context, givenContextWhenCheckIfAllocationsAreMultiStorageThenReturnProperValueAccordingToContextType) {
    MockContext context;
    EXPECT_TRUE(context.areMultiStorageAllocationsPreffered());

    context.setContextType(ContextType::CONTEXT_TYPE_SPECIALIZED);
    EXPECT_FALSE(context.areMultiStorageAllocationsPreffered());

    context.setContextType(ContextType::CONTEXT_TYPE_UNRESTRICTIVE);
    EXPECT_TRUE(context.areMultiStorageAllocationsPreffered());
}
