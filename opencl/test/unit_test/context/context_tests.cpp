/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_deferred_deleter.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/mocks/ult_device_factory.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/context/context.inl"
#include "opencl/source/gtpin/gtpin_defs.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/sharings/sharing.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_buffer.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"
#include "opencl/test/unit_test/mocks/mock_context.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/test_macros/test_checks_ocl.h"

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
    using PlatformFixture::setUp;

    void SetUp() override {
        PlatformFixture::setUp();

        properties.push_back(CL_CONTEXT_PLATFORM);
        properties.push_back(reinterpret_cast<cl_context_properties>(pPlatform));
        properties.push_back(0);

        context = Context::create<WhiteBoxContext>(properties.data(), ClDeviceVector(devices, numDevices), nullptr, nullptr, retVal);
        ASSERT_NE(nullptr, context);
    }

    void TearDown() override {
        delete context;
        PlatformFixture::tearDown();
    }

    uint32_t getRootDeviceIndex() {
        return context->getDevice(0)->getRootDeviceIndex();
    }

    cl_int retVal = CL_SUCCESS;
    WhiteBoxContext *context = nullptr;
    std::vector<cl_context_properties> properties;
};

TEST_F(ContextTest, WhenCreatingContextThenDevicesAllDevicesExist) {
    for (size_t deviceOrdinal = 0; deviceOrdinal < context->getNumDevices(); ++deviceOrdinal) {
        EXPECT_NE(nullptr, context->getDevice(deviceOrdinal));
    }
}

TEST_F(ContextTest, WhenCreatingContextThenMemoryManagerForContextIsSet) {
    EXPECT_NE(nullptr, context->getMM());
}

TEST_F(ContextTest, WhenCreatingContextThenPropertiesAreCopied) {
    auto contextProperties = context->getProperties();
    EXPECT_NE(properties.data(), contextProperties);
}

TEST_F(ContextTest, WhenCreatingContextThenPropertiesAreValid) {
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

TEST_F(ContextTest, WhenCreatingContextThenSpecialQueueIsAvailable) {
    auto specialQ = context->getSpecialQueue(0u);
    EXPECT_NE(specialQ, nullptr);
}

TEST_F(ContextTest, WhenSettingSpecialQueueThenQueueIsAvailable) {
    MockContext context((ClDevice *)devices[0], true);

    auto specialQ = context.specialQueues[0];
    EXPECT_EQ(specialQ, nullptr);

    auto cmdQ = new MockCommandQueue(&context, (ClDevice *)devices[0], 0, false);
    context.setSpecialQueue(cmdQ, 0u);
    specialQ = context.getSpecialQueue(0u);
    EXPECT_NE(specialQ, nullptr);
}

TEST_F(ContextTest, givenCmdQueueWithoutContextWhenBeingCreatedNextDeletedThenContextRefCountShouldNeitherBeIncrementedNorNextDecremented) {
    MockContext context((ClDevice *)devices[0]);
    EXPECT_EQ(1, context.getRefInternalCount());

    auto cmdQ1 = new MockCommandQueue();
    EXPECT_EQ(1, context.getRefInternalCount());

    delete cmdQ1;
    EXPECT_EQ(1, context.getRefInternalCount());

    auto cmdQ2 = new MockCommandQueue(nullptr, (ClDevice *)devices[0], 0, false);
    EXPECT_EQ(1, context.getRefInternalCount());

    delete cmdQ2;
    EXPECT_EQ(1, context.getRefInternalCount());
}

TEST_F(ContextTest, givenCmdQueueWithContextWhenBeingCreatedNextDeletedThenContextRefCountShouldBeIncrementedNextDecremented) {
    MockContext context((ClDevice *)devices[0]);
    EXPECT_EQ(1, context.getRefInternalCount());

    auto cmdQ = new MockCommandQueue(&context, (ClDevice *)devices[0], 0, false);
    EXPECT_EQ(2, context.getRefInternalCount());

    delete cmdQ;
    EXPECT_EQ(1, context.getRefInternalCount());
}

TEST_F(ContextTest, givenContextWhenItIsCreatedFromDeviceThenItAddsRefCountToThisDevice) {
    auto device = castToObject<ClDevice>(devices[0]);
    EXPECT_EQ(2, device->getRefInternalCount());
    cl_device_id deviceID = devices[0];
    std::unique_ptr<Context> context(Context::create<Context>(0, ClDeviceVector(&deviceID, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(3, device->getRefInternalCount());
    context.reset(nullptr);
    EXPECT_EQ(2, device->getRefInternalCount());
}

TEST_F(ContextTest, givenContextWhenItIsCreatedFromMultipleDevicesThenItAddsRefCountToThoseDevices) {
    auto device = castToObject<ClDevice>(devices[0]);
    EXPECT_EQ(2, device->getRefInternalCount());

    ClDeviceVector devicesVector;
    devicesVector.push_back(device);
    devicesVector.push_back(device);

    std::unique_ptr<Context> context(Context::create<Context>(0, devicesVector, nullptr, nullptr, retVal));
    EXPECT_EQ(4, device->getRefInternalCount());
    context.reset(nullptr);
    EXPECT_EQ(2, device->getRefInternalCount());
}

TEST_F(ContextTest, givenSpecialCmdQueueWithContextWhenBeingCreatedNextAutoDeletedThenContextRefCountShouldNeitherBeIncrementedNorNextDecremented) {
    MockContext context((ClDevice *)devices[0], true);
    EXPECT_EQ(1, context.getRefInternalCount());

    auto cmdQ = new MockCommandQueue(&context, (ClDevice *)devices[0], 0, false);
    context.overrideSpecialQueueAndDecrementRefCount(cmdQ, 0u);
    EXPECT_EQ(1, context.getRefInternalCount());

    // special queue is to be deleted implicitly by context
}

TEST_F(ContextTest, givenSpecialCmdQueueWithContextWhenBeingCreatedNextDeletedThenContextRefCountShouldNeitherBeIncrementedNorNextDecremented) {
    MockContext context((ClDevice *)devices[0], true);
    EXPECT_EQ(1, context.getRefInternalCount());

    auto cmdQ = new MockCommandQueue(&context, (ClDevice *)devices[0], 0, false);
    context.overrideSpecialQueueAndDecrementRefCount(cmdQ, 0u);
    EXPECT_EQ(1, context.getRefInternalCount());

    delete cmdQ;
    EXPECT_EQ(1, context.getRefInternalCount());

    context.setSpecialQueue(nullptr, 0u);
}

TEST_F(ContextTest, GivenInteropSyncParamWhenCreateContextThenSetContextParam) {
    cl_device_id deviceID = devices[0];
    auto pPlatform = NEO::platform();
    cl_platform_id pid[1];
    pid[0] = pPlatform;

    cl_context_properties validProperties[5] = {CL_CONTEXT_PLATFORM, (cl_context_properties)pid[0],
                                                CL_CONTEXT_INTEROP_USER_SYNC, 1, 0};
    cl_int retVal = CL_SUCCESS;
    auto context = Context::create<Context>(validProperties, ClDeviceVector(&deviceID, 1), nullptr, nullptr, retVal);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_TRUE(context->getInteropUserSyncEnabled());
    delete context;

    validProperties[3] = 0; // false
    context = Context::create<Context>(validProperties, ClDeviceVector(&deviceID, 1), nullptr, nullptr, retVal);
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

TEST(Context, whenCreateContextThenSpecialQueueUsesInternalEngine) {
    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
    cl_device_id clDevice = device.get();
    cl_int retVal = CL_SUCCESS;

    auto context = std::unique_ptr<MockContext>(Context::create<MockContext>(nullptr, ClDeviceVector(&clDevice, 1), nullptr, nullptr, retVal));
    ASSERT_NE(nullptr, context);
    EXPECT_EQ(CL_SUCCESS, retVal);

    auto specialQueueEngine = context->getSpecialQueue(device->getRootDeviceIndex())->getGpgpuEngine();
    auto internalEngine = device->getInternalEngine();
    EXPECT_EQ(internalEngine.commandStreamReceiver, specialQueueEngine.commandStreamReceiver);
}

TEST(MultiDeviceContextTest, givenContextWithMultipleDevicesWhenGettingInfoAboutSubDevicesThenCorrectValueIsReturned) {
    MockSpecializedContext context1;
    MockUnrestrictiveContext context2;
    MockDefaultContext context3;

    EXPECT_EQ(2u, context1.getNumDevices());
    EXPECT_TRUE(context1.containsMultipleSubDevices(0));

    EXPECT_EQ(3u, context2.getNumDevices());
    EXPECT_TRUE(context2.containsMultipleSubDevices(0));

    EXPECT_EQ(3u, context3.getNumDevices());
    EXPECT_FALSE(context3.containsMultipleSubDevices(0));
    EXPECT_FALSE(context3.containsMultipleSubDevices(1));
    EXPECT_FALSE(context3.containsMultipleSubDevices(2));
}

class ContextWithAsyncDeleterTest : public ::testing::WithParamInterface<bool>,
                                    public ::testing::Test {
  public:
    void SetUp() override {
        memoryManager = new MockMemoryManager();
        device = new MockClDevice{MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get())};
        deleter = new MockDeferredDeleter();

        device->allEngines.clear();
        device->device.secondaryEngines.clear();
        device->device.regularEngineGroups.clear();
        device->injectMemoryManager(memoryManager);
        device->createEngines();
        memoryManager->setDeferredDeleter(deleter);
    }
    void TearDown() override {
        delete device;
    }
    Context *context;
    MockMemoryManager *memoryManager;
    MockDeferredDeleter *deleter;
    MockClDevice *device;
};

TEST_P(ContextWithAsyncDeleterTest, givenContextWithMemoryManagerWhenAsyncDeleterIsEnabledThenUsesDeletersMethods) {
    cl_device_id clDevice = device;
    cl_int retVal;
    ClDeviceVector deviceVector(&clDevice, 1);
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

INSTANTIATE_TEST_SUITE_P(ContextTests,
                         ContextWithAsyncDeleterTest,
                         ::testing::Bool());

TEST(DefaultContext, givenDefaultContextWhenItIsQueriedForTypeThenDefaultTypeIsReturned) {
    MockContext context;
    EXPECT_EQ(ContextType::CONTEXT_TYPE_DEFAULT, context.peekContextType());
}

TEST(Context, givenContextWhenCheckIfAllocationsAreMultiStorageThenReturnProperValueAccordingToContextType) {
    MockContext context;
    EXPECT_TRUE(context.areMultiStorageAllocationsPreferred());

    context.contextType = ContextType::CONTEXT_TYPE_SPECIALIZED;
    EXPECT_FALSE(context.areMultiStorageAllocationsPreferred());

    context.contextType = ContextType::CONTEXT_TYPE_UNRESTRICTIVE;
    EXPECT_TRUE(context.areMultiStorageAllocationsPreferred());
}

TEST(Context, givenContextWhenIsDeviceAssociatedIsCalledWithItsDeviceThenTrueIsReturned) {
    MockContext context;
    EXPECT_TRUE(context.isDeviceAssociated(*context.getDevice(0)));
}

TEST(Context, givenContextWhenIsDeviceAssociatedIsCalledWithNotAssociatedDeviceThenFalseIsReturned) {
    MockContext context0;
    MockContext context1;
    EXPECT_FALSE(context0.isDeviceAssociated(*context1.getDevice(0)));
    EXPECT_FALSE(context1.isDeviceAssociated(*context0.getDevice(0)));
}
TEST(Context, givenContextWithSingleDevicesWhenGettingDeviceBitfieldForAllocationThenDeviceBitfieldForDeviceIsReturned) {
    UltClDeviceFactory deviceFactory{1, 3};
    auto device = deviceFactory.subDevices[1];
    auto expectedDeviceBitfield = device->getDeviceBitfield();
    MockContext context(device);
    EXPECT_EQ(expectedDeviceBitfield.to_ulong(), context.getDeviceBitfieldForAllocation(device->getRootDeviceIndex()).to_ulong());
}
TEST(Context, givenContextWithMultipleSubDevicesWhenGettingDeviceBitfieldForAllocationThenMergedDeviceBitfieldIsReturned) {
    UltClDeviceFactory deviceFactory{1, 3};
    cl_int retVal;
    cl_device_id devices[]{deviceFactory.subDevices[0], deviceFactory.subDevices[2]};
    ClDeviceVector deviceVector(devices, 2);
    auto expectedDeviceBitfield = deviceFactory.subDevices[0]->getDeviceBitfield() | deviceFactory.subDevices[2]->getDeviceBitfield();
    auto context = Context::create<Context>(0, deviceVector, nullptr, nullptr, retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(expectedDeviceBitfield.to_ulong(), context->getDeviceBitfieldForAllocation(deviceFactory.rootDevices[0]->getRootDeviceIndex()).to_ulong());
    context->release();
}

TEST(MultiDeviceContextTest, givenContextWithTwoDifferentSubDevicesFromDifferentRootDevicesWhenGettingDeviceBitfieldForAllocationThenSeparatedDeviceBitfieldsAreReturned) {
    DebugManagerStateRestore restorer;

    debugManager.flags.EnableMultiRootDeviceContexts.set(true);
    UltClDeviceFactory deviceFactory{2, 2};
    cl_int retVal;
    cl_device_id devices[]{deviceFactory.subDevices[1], deviceFactory.subDevices[2]};
    ClDeviceVector deviceVector(devices, 2);

    auto expectedDeviceBitfieldForRootDevice0 = deviceFactory.subDevices[1]->getDeviceBitfield();
    auto expectedDeviceBitfieldForRootDevice1 = deviceFactory.subDevices[2]->getDeviceBitfield();

    auto context = Context::create<Context>(0, deviceVector, nullptr, nullptr, retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_EQ(expectedDeviceBitfieldForRootDevice0.to_ulong(), context->getDeviceBitfieldForAllocation(deviceFactory.rootDevices[0]->getRootDeviceIndex()).to_ulong());
    EXPECT_EQ(expectedDeviceBitfieldForRootDevice1.to_ulong(), context->getDeviceBitfieldForAllocation(deviceFactory.rootDevices[1]->getRootDeviceIndex()).to_ulong());

    context->release();
}

TEST(MultiDeviceContextTest, givenMultipleRootDevicesWhenCreatingMultiRootDeviceContextCrossDeviceTagAllocationsAreCreated) {
    DebugManagerStateRestore restorer;

    UltClDeviceFactory deviceFactory{3, 0};
    cl_int retVal;

    for (auto &csr : deviceFactory.pUltDeviceFactory->rootDevices[0]->commandStreamReceivers) {
        auto tagsMultiAllocation = csr->getTagsMultiAllocation();
        EXPECT_NE(nullptr, tagsMultiAllocation->getGraphicsAllocation(0));
        EXPECT_EQ(nullptr, tagsMultiAllocation->getGraphicsAllocation(1));
        EXPECT_EQ(nullptr, tagsMultiAllocation->getGraphicsAllocation(2));
    }

    for (auto &csr : deviceFactory.pUltDeviceFactory->rootDevices[1]->commandStreamReceivers) {
        auto tagsMultiAllocation = csr->getTagsMultiAllocation();
        EXPECT_EQ(nullptr, tagsMultiAllocation->getGraphicsAllocation(0));
        EXPECT_NE(nullptr, tagsMultiAllocation->getGraphicsAllocation(1));
        EXPECT_EQ(nullptr, tagsMultiAllocation->getGraphicsAllocation(2));
    }

    for (auto &csr : deviceFactory.pUltDeviceFactory->rootDevices[2]->commandStreamReceivers) {
        auto tagsMultiAllocation = csr->getTagsMultiAllocation();
        EXPECT_EQ(nullptr, tagsMultiAllocation->getGraphicsAllocation(0));
        EXPECT_EQ(nullptr, tagsMultiAllocation->getGraphicsAllocation(1));
        EXPECT_NE(nullptr, tagsMultiAllocation->getGraphicsAllocation(2));
    }
    cl_device_id devices[]{deviceFactory.rootDevices[0], deviceFactory.rootDevices[2]};
    ClDeviceVector deviceVector(devices, 2);

    auto context = Context::create<Context>(0, deviceVector, nullptr, nullptr, retVal);
    EXPECT_NE(nullptr, context);
    EXPECT_EQ(CL_SUCCESS, retVal);
    for (auto &csr : deviceFactory.pUltDeviceFactory->rootDevices[0]->commandStreamReceivers) {
        auto tagsMultiAllocation = csr->getTagsMultiAllocation();
        EXPECT_NE(nullptr, tagsMultiAllocation->getGraphicsAllocation(0));
        EXPECT_EQ(nullptr, tagsMultiAllocation->getGraphicsAllocation(1));
        EXPECT_NE(nullptr, tagsMultiAllocation->getGraphicsAllocation(2));
    }
    for (auto &csr : deviceFactory.pUltDeviceFactory->rootDevices[1]->commandStreamReceivers) {
        auto tagsMultiAllocation = csr->getTagsMultiAllocation();
        EXPECT_EQ(nullptr, tagsMultiAllocation->getGraphicsAllocation(0));
        EXPECT_NE(nullptr, tagsMultiAllocation->getGraphicsAllocation(1));
        EXPECT_EQ(nullptr, tagsMultiAllocation->getGraphicsAllocation(2));
    }
    for (auto &csr : deviceFactory.pUltDeviceFactory->rootDevices[2]->commandStreamReceivers) {
        auto tagsMultiAllocation = csr->getTagsMultiAllocation();
        EXPECT_NE(nullptr, tagsMultiAllocation->getGraphicsAllocation(0));
        EXPECT_EQ(nullptr, tagsMultiAllocation->getGraphicsAllocation(1));
        EXPECT_NE(nullptr, tagsMultiAllocation->getGraphicsAllocation(2));
    }

    context->release();
}

TEST(Context, WhenSettingContextDestructorCallbackThenCallOrderIsPreserved) {
    struct UserDataType {
        cl_context expectedContext;
        std::vector<size_t> &vectorToModify;
        size_t valueToAdd;
    };
    auto callback = [](cl_context context, void *userData) -> void {
        auto pUserData = reinterpret_cast<UserDataType *>(userData);
        EXPECT_EQ(pUserData->expectedContext, context);
        pUserData->vectorToModify.push_back(pUserData->valueToAdd);
    };

    auto pContext = new MockContext{};
    std::vector<size_t> callbacksReturnValues;
    UserDataType userDataArray[]{
        {pContext, callbacksReturnValues, 1},
        {pContext, callbacksReturnValues, 2},
        {pContext, callbacksReturnValues, 3}};

    for (auto &userData : userDataArray) {
        cl_int retVal = clSetContextDestructorCallback(pContext, callback, &userData);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }
    delete pContext;

    ASSERT_EQ(3u, callbacksReturnValues.size());
    EXPECT_EQ(3u, callbacksReturnValues[0]);
    EXPECT_EQ(2u, callbacksReturnValues[1]);
    EXPECT_EQ(1u, callbacksReturnValues[2]);
}

TEST(Context, givenContextAndDevicesWhenIsTileOnlyThenProperValueReturned) {
    UltClDeviceFactory deviceFactoryWithSubDevices{1, 2};
    UltClDeviceFactory deviceFactoryWithMultipleDevices{2, 0};
    cl_device_id devices[] = {deviceFactoryWithMultipleDevices.rootDevices[0], deviceFactoryWithMultipleDevices.rootDevices[1]};

    MockContext tileOnlyContext(deviceFactoryWithMultipleDevices.rootDevices[0]);
    MockContext subDevicesContext(deviceFactoryWithSubDevices.rootDevices[0]);
    MockContext multipleDevicesContext(ClDeviceVector(devices, 2));

    EXPECT_TRUE(tileOnlyContext.isSingleDeviceContext());
    EXPECT_FALSE(subDevicesContext.isSingleDeviceContext());
    EXPECT_FALSE(multipleDevicesContext.isSingleDeviceContext());
}

TEST(InvalidExtraPropertiesTests, givenInvalidExtraPropertiesWhenCreatingContextThenContextIsNotCreated) {
    constexpr cl_context_properties invalidPropertyType = (1 << 31);
    constexpr cl_context_properties invalidContextFlag = (1 << 31);

    auto device = std::make_unique<MockClDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    cl_device_id deviceID = device.get();
    cl_int retVal = 0;
    std::unique_ptr<Context> context;

    {
        cl_context_properties properties[] = {invalidPropertyType, invalidContextFlag, 0};
        context.reset(Context::create<Context>(properties, ClDeviceVector(&deviceID, 1), nullptr, nullptr, retVal));
        EXPECT_EQ(CL_INVALID_PROPERTY, retVal);
        EXPECT_EQ(nullptr, context.get());
    }
}

using ContextCreateTests = ::testing::Test;

HWCMDTEST_F(IGFX_XE_HP_CORE, ContextCreateTests, givenLocalMemoryAllocationWhenBlitMemoryToAllocationIsCalledThenSuccessIsReturned) {
    if (is32bit) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restore;
    debugManager.flags.EnableLocalMemory.set(true);
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));

    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    {
        auto productHelper = ProductHelper::create(defaultHwInfo->platform.eProductFamily);
        auto defaultBcsIndex = EngineHelpers::getBcsIndex(productHelper->getDefaultCopyEngine());
        if (0u != defaultBcsIndex) {
            defaultHwInfo->featureTable.ftrBcsInfo.set(defaultBcsIndex, true);
            defaultHwInfo->featureTable.ftrBcsInfo.set(EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS3), true); // enable BCS3 for internal operations
        }
    }
    UltClDeviceFactory deviceFactory{1, 2};

    ClDevice *devicesToTest[] = {deviceFactory.rootDevices[0], deviceFactory.subDevices[0], deviceFactory.subDevices[1]};

    for (const auto &testedDevice : devicesToTest) {

        MockContext context(testedDevice);
        cl_int retVal;
        auto buffer = std::unique_ptr<Buffer>(Buffer::create(&context, {}, 1, nullptr, retVal));
        auto memory = buffer->getGraphicsAllocation(testedDevice->getRootDeviceIndex());
        uint8_t hostMemory[1];
        auto executionEnv = testedDevice->getExecutionEnvironment();
        executionEnv->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = false;

        EXPECT_EQ(BlitOperationResult::unsupported, BlitHelper::blitMemoryToAllocation(buffer->getContext()->getDevice(0)->getDevice(), memory, buffer->getOffset(), hostMemory, {1, 1, 1}));

        executionEnv->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
        EXPECT_EQ(BlitOperationResult::success, BlitHelper::blitMemoryToAllocation(buffer->getContext()->getDevice(0)->getDevice(), memory, buffer->getOffset(), hostMemory, {1, 1, 1}));
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ContextCreateTests, givenGpuHangOnFlushBcsTaskAndLocalMemoryAllocationWhenBlitMemoryToAllocationIsCalledThenGpuHangIsReturned) {
    if (is32bit) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore restore;
    debugManager.flags.EnableLocalMemory.set(true);
    debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(LocalMemoryAccessMode::defaultMode));

    VariableBackup<HardwareInfo> backupHwInfo(defaultHwInfo.get());
    defaultHwInfo->capabilityTable.blitterOperationsSupported = true;
    {
        auto productHelper = ProductHelper::create(defaultHwInfo->platform.eProductFamily);
        auto defaultBcsIndex = EngineHelpers::getBcsIndex(productHelper->getDefaultCopyEngine());
        if (0u != defaultBcsIndex) {
            defaultHwInfo->featureTable.ftrBcsInfo.set(defaultBcsIndex, true);
            defaultHwInfo->featureTable.ftrBcsInfo.set(EngineHelpers::getBcsIndex(aub_stream::ENGINE_BCS3), true); // enable BCS3 for internal operations
        }
    }
    UltClDeviceFactory deviceFactory{1, 2};

    auto testedDevice = deviceFactory.rootDevices[0];

    MockContext context(testedDevice);
    cl_int retVal;
    auto buffer = std::unique_ptr<Buffer>(Buffer::create(&context, {}, 1, nullptr, retVal));
    auto memory = buffer->getGraphicsAllocation(testedDevice->getRootDeviceIndex());

    uint8_t hostMemory[1];
    auto executionEnv = testedDevice->getExecutionEnvironment();
    executionEnv->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = false;

    EXPECT_EQ(BlitOperationResult::unsupported, BlitHelper::blitMemoryToAllocation(buffer->getContext()->getDevice(0)->getDevice(), memory, buffer->getOffset(), hostMemory, {1, 1, 1}));

    executionEnv->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;

    const auto rootDevice = testedDevice->getDevice().getRootDevice();
    const auto leastOccupiedBankDevice = rootDevice->getRTMemoryBackedBuffer() ? 1u : 0u;
    const auto blitDevice = rootDevice->getNearestGenericSubDevice(leastOccupiedBankDevice);
    auto &selectorCopyEngine = blitDevice->getSelectorCopyEngine();
    auto deviceBitfield = blitDevice->getDeviceBitfield();

    auto &rootDeviceEnvironment = testedDevice->getRootDeviceEnvironment();
    auto &gfxCoreHelper = rootDeviceEnvironment.getHelper<GfxCoreHelper>();

    auto internalUsage = true;
    auto bcsEngineType = EngineHelpers::getBcsEngineType(rootDeviceEnvironment, deviceBitfield, selectorCopyEngine, internalUsage);
    auto bcsEngineUsage = gfxCoreHelper.preferInternalBcsEngine() ? EngineUsage::internal : EngineUsage::regular;
    auto bcsEngine = blitDevice->tryGetEngine(bcsEngineType, bcsEngineUsage);
    ASSERT_NE(nullptr, bcsEngine);

    auto ultBcsCsr = static_cast<UltCommandStreamReceiver<FamilyType> *>(bcsEngine->commandStreamReceiver);
    ultBcsCsr->callBaseFlushBcsTask = false;
    ultBcsCsr->flushBcsTaskReturnValue = CompletionStamp::gpuHang;

    EXPECT_EQ(BlitOperationResult::gpuHang, BlitHelper::blitMemoryToAllocation(buffer->getContext()->getDevice(0)->getDevice(), memory, buffer->getOffset(), hostMemory, {1, 1, 1}));
}

struct AllocationReuseContextTest : ContextTest {
    void addMappedPtr(Buffer &buffer, void *ptr, size_t ptrLength) {
        auto &handler = context->getMapOperationsStorage().getHandler(&buffer);
        MemObjSizeArray size{};
        MemObjSizeArray offset{};
        cl_map_flags mapFlag = CL_MAP_READ;
        EXPECT_TRUE(handler.add(ptr, ptrLength, mapFlag, size, offset, 0, buffer.getMultiGraphicsAllocation().getDefaultGraphicsAllocation()));
    }

    void addSvmPtr(InternalMemoryType type, GraphicsAllocation &allocation) {
        SvmAllocationData svmEntry{getRootDeviceIndex()};
        svmEntry.memoryType = type;
        svmEntry.size = allocation.getUnderlyingBufferSize();
        svmEntry.gpuAllocations.addAllocation(&allocation);
        if (type != InternalMemoryType::deviceUnifiedMemory) {
            svmEntry.cpuAllocation = &allocation;
        }
        context->getSVMAllocsManager()->insertSVMAlloc(svmEntry);
    }
};

TEST_F(AllocationReuseContextTest, givenSharedSvmAllocPresentWhenGettingExistingHostPtrAllocThenRetrieveTheAllocation) {
    REQUIRE_SVM_OR_SKIP(context->getDevice(0));

    uint64_t svmPtrGpu = 0x1234;
    void *svmPtr = reinterpret_cast<void *>(svmPtrGpu);
    MockGraphicsAllocation allocation{svmPtr, svmPtrGpu, 400};
    addSvmPtr(InternalMemoryType::sharedUnifiedMemory, allocation);

    GraphicsAllocation *retrievedAllocation{};
    InternalMemoryType retrievedMemoryType{};
    bool retrievedCpuCopyStatus = true;
    retVal = context->tryGetExistingHostPtrAllocation(svmPtr, allocation.getUnderlyingBufferSize(), getRootDeviceIndex(),
                                                      retrievedAllocation, retrievedMemoryType, retrievedCpuCopyStatus);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(&allocation, retrievedAllocation);
    EXPECT_EQ(InternalMemoryType::sharedUnifiedMemory, retrievedMemoryType);
    EXPECT_TRUE(retrievedCpuCopyStatus);
}

TEST_F(AllocationReuseContextTest, givenHostSvmAllocPresentWhenGettingExistingHostPtrAllocThenRetrieveTheAllocation) {
    REQUIRE_SVM_OR_SKIP(context->getDevice(0));

    uint64_t svmPtrGpu = 0x1234;
    void *svmPtr = reinterpret_cast<void *>(svmPtrGpu);
    MockGraphicsAllocation allocation{svmPtr, svmPtrGpu, 400};
    addSvmPtr(InternalMemoryType::hostUnifiedMemory, allocation);

    GraphicsAllocation *retrievedAllocation{};
    InternalMemoryType retrievedMemoryType{};
    bool retrievedCpuCopyStatus = true;
    retVal = context->tryGetExistingHostPtrAllocation(svmPtr, allocation.getUnderlyingBufferSize(), getRootDeviceIndex(),
                                                      retrievedAllocation, retrievedMemoryType, retrievedCpuCopyStatus);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(&allocation, retrievedAllocation);
    EXPECT_EQ(InternalMemoryType::hostUnifiedMemory, retrievedMemoryType);
    EXPECT_TRUE(retrievedCpuCopyStatus);
}

TEST_F(AllocationReuseContextTest, givenDeviceSvmAllocPresentWhenGettingExistingHostPtrAllocThenRetrieveTheAllocationAndDisallowCpuCopy) {
    REQUIRE_SVM_OR_SKIP(context->getDevice(0));

    uint64_t svmPtrGpu = 0x1234;
    void *svmPtr = reinterpret_cast<void *>(svmPtrGpu);
    MockGraphicsAllocation allocation{svmPtr, svmPtrGpu, 400};
    addSvmPtr(InternalMemoryType::deviceUnifiedMemory, allocation);

    GraphicsAllocation *retrievedAllocation{};
    InternalMemoryType retrievedMemoryType{};
    bool retrievedCpuCopyStatus = true;
    retVal = context->tryGetExistingHostPtrAllocation(svmPtr, allocation.getUnderlyingBufferSize(), getRootDeviceIndex(),
                                                      retrievedAllocation, retrievedMemoryType, retrievedCpuCopyStatus);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(&allocation, retrievedAllocation);
    EXPECT_EQ(InternalMemoryType::deviceUnifiedMemory, retrievedMemoryType);
    EXPECT_FALSE(retrievedCpuCopyStatus);
}

TEST_F(AllocationReuseContextTest, givenHostSvmAllocPresentButRequestingTooBigSizeWhenGettingExistingHostPtrAllocThenReturnError) {
    REQUIRE_SVM_OR_SKIP(context->getDevice(0));

    uint64_t svmPtrGpu = 0x1234;
    void *svmPtr = reinterpret_cast<void *>(svmPtrGpu);
    MockGraphicsAllocation allocation{svmPtr, svmPtrGpu, 400};
    addSvmPtr(InternalMemoryType::hostUnifiedMemory, allocation);

    size_t ptrSizeToRetrieve = allocation.getUnderlyingBufferSize() + 1;
    GraphicsAllocation *retrievedAllocation{};
    InternalMemoryType retrievedMemoryType{};
    bool retrievedCpuCopyStatus = true;
    retVal = context->tryGetExistingHostPtrAllocation(svmPtr, ptrSizeToRetrieve, getRootDeviceIndex(),
                                                      retrievedAllocation, retrievedMemoryType, retrievedCpuCopyStatus);
    EXPECT_EQ(CL_INVALID_OPERATION, retVal);
}

TEST_F(AllocationReuseContextTest, givenHostPtrStoredInMapOperationsStorageWhenGettingExistingHostPtrAllocThenRetrieveTheAllocation) {
    MockGraphicsAllocation allocation{};
    MockBuffer buffer{context, allocation};
    void *mappedPtr = reinterpret_cast<void *>(0x1234);
    size_t mappedPtrSize = 10u;
    addMappedPtr(buffer, mappedPtr, mappedPtrSize);

    GraphicsAllocation *retrievedAllocation{};
    InternalMemoryType retrievedMemoryType{};
    bool retrievedCpuCopyStatus = true;
    retVal = context->tryGetExistingHostPtrAllocation(mappedPtr, mappedPtrSize, getRootDeviceIndex(),
                                                      retrievedAllocation, retrievedMemoryType, retrievedCpuCopyStatus);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(&allocation, retrievedAllocation);
    EXPECT_EQ(InternalMemoryType::notSpecified, retrievedMemoryType);
    EXPECT_TRUE(retrievedCpuCopyStatus);
}

TEST_F(AllocationReuseContextTest, givenHostPtrNotStoredInMapOperationsStorageWhenGettingExistingHostPtrAllocThenFailToRetrieveTheAllocation) {
    MockGraphicsAllocation allocation{};
    MockBuffer buffer{context, allocation};
    void *mappedPtr = reinterpret_cast<void *>(0x1234);
    size_t mappedPtrSize = 10u;
    addMappedPtr(buffer, mappedPtr, mappedPtrSize);

    void *differentPtr = reinterpret_cast<void *>(0x12345);
    GraphicsAllocation *retrievedAllocation{};
    InternalMemoryType retrievedMemoryType{};
    bool retrievedCpuCopyStatus = true;
    retVal = context->tryGetExistingHostPtrAllocation(differentPtr, mappedPtrSize, getRootDeviceIndex(),
                                                      retrievedAllocation, retrievedMemoryType, retrievedCpuCopyStatus);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, retrievedAllocation);
    EXPECT_EQ(InternalMemoryType::notSpecified, retrievedMemoryType);
    EXPECT_TRUE(retrievedCpuCopyStatus);
}

TEST_F(AllocationReuseContextTest, givenHostPtrStoredInMapOperationsStorageAndRequestedPtrToBigWhenGettingExistingHostPtrAllocThenFailRetrieveTheAllocation) {
    MockGraphicsAllocation allocation{};
    MockBuffer buffer{context, allocation};
    void *mappedPtr = reinterpret_cast<void *>(0x1234);
    size_t mappedPtrSize = 10u;
    addMappedPtr(buffer, mappedPtr, mappedPtrSize);

    size_t ptrSizeToRetrieve = mappedPtrSize + 1;
    GraphicsAllocation *retrievedAllocation{};
    InternalMemoryType retrievedMemoryType{};
    bool retrievedCpuCopyStatus = true;
    retVal = context->tryGetExistingHostPtrAllocation(mappedPtr, ptrSizeToRetrieve, getRootDeviceIndex(),
                                                      retrievedAllocation, retrievedMemoryType, retrievedCpuCopyStatus);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(nullptr, retrievedAllocation);
    EXPECT_EQ(InternalMemoryType::notSpecified, retrievedMemoryType);
    EXPECT_TRUE(retrievedCpuCopyStatus);
}

struct MockGTPinTestContext : Context {
    using Context::svmAllocsManager;
};

struct MockSVMAllocManager : SVMAllocsManager {
    MockSVMAllocManager() : SVMAllocsManager(nullptr) {}
    ~MockSVMAllocManager() override {
        svmAllocManagerDeleted = true;
    }

    inline static bool svmAllocManagerDeleted = false;
};

struct GTPinContextDestroyTest : ContextTest {
    void SetUp() override {
        ContextTest::SetUp();
    }

    void TearDown() override {
        PlatformFixture::tearDown();
    }
};

void onContextDestroy(gtpin::context_handle_t context) {
    EXPECT_FALSE(MockSVMAllocManager::svmAllocManagerDeleted);
}

namespace NEO {
extern gtpin::ocl::gtpin_events_t gtpinCallbacks;
TEST_F(GTPinContextDestroyTest, whenCallingConxtextDestructorThenGTPinIsNotifiedBeforeSVMAllocManagerGetsDestroyed) {
    auto mockContext = reinterpret_cast<MockGTPinTestContext *>(context);
    if (mockContext->svmAllocsManager) {
        mockContext->getDeviceMemAllocPool().cleanup();
        mockContext->getHostMemAllocPool().cleanup();
        mockContext->svmAllocsManager->cleanupUSMAllocCaches();
        delete mockContext->svmAllocsManager;
    }
    mockContext->svmAllocsManager = new MockSVMAllocManager();

    gtpinCallbacks.onContextDestroy = onContextDestroy;
    delete context;
    EXPECT_TRUE(MockSVMAllocManager::svmAllocManagerDeleted);
}
} // namespace NEO

struct ContextUsmPoolParamsTest : public ::testing::Test {
    void SetUp() override {
        deviceFactory = std::make_unique<UltClDeviceFactory>(2, 0);
        device = deviceFactory->rootDevices[rootDeviceIndex];
        mockNeoDevice = static_cast<MockDevice *>(&device->getDevice());
        mockProductHelper = new MockProductHelper;
        mockNeoDevice->getRootDeviceEnvironmentRef().productHelper.reset(mockProductHelper);
    }

    bool compareUsmPoolParams(const MockContext::UsmPoolParams &first, const MockContext::UsmPoolParams &second) {
        return first.poolSize == second.poolSize &&
               first.minServicedSize == second.minServicedSize &&
               first.maxServicedSize == second.maxServicedSize;
    }

    const size_t rootDeviceIndex = 1u;
    std::unique_ptr<UltClDeviceFactory> deviceFactory;
    MockClDevice *device;
    MockDevice *mockNeoDevice;
    MockProductHelper *mockProductHelper;
    std::unique_ptr<MockContext> context;
    cl_int retVal = CL_SUCCESS;
    DebugManagerStateRestore restore;
};

TEST_F(ContextUsmPoolParamsTest, GivenDisabled2MBLocalMemAlignmentWhenGettingUsmPoolParamsThenReturnCorrectValues) {
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = false;

    cl_device_id devices[] = {device};
    context.reset(Context::create<MockContext>(nullptr, ClDeviceVector(devices, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    const MockContext::UsmPoolParams expectedUsmHostPoolParams{
        .poolSize = 2 * MemoryConstants::megaByte,
        .minServicedSize = 0u,
        .maxServicedSize = 1 * MemoryConstants::megaByte};

    const MockContext::UsmPoolParams expectedUsmDevicePoolParams{
        .poolSize = 2 * MemoryConstants::megaByte,
        .minServicedSize = 0u,
        .maxServicedSize = 1 * MemoryConstants::megaByte};

    EXPECT_TRUE(compareUsmPoolParams(expectedUsmHostPoolParams, context->getUsmHostPoolParams()));
    EXPECT_TRUE(compareUsmPoolParams(expectedUsmDevicePoolParams, context->getUsmDevicePoolParams()));
}

TEST_F(ContextUsmPoolParamsTest, GivenEnabled2MBLocalMemAlignmentWhenGettingUsmPoolParamsThenReturnCorrectValues) {
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

    cl_device_id devices[] = {device};
    context.reset(Context::create<MockContext>(nullptr, ClDeviceVector(devices, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    const MockContext::UsmPoolParams expectedUsmHostPoolParams{
        .poolSize = 2 * MemoryConstants::megaByte,
        .minServicedSize = 0u,
        .maxServicedSize = 1 * MemoryConstants::megaByte};

    const MockContext::UsmPoolParams expectedUsmDevicePoolParams{
        .poolSize = 16 * MemoryConstants::megaByte,
        .minServicedSize = 0u,
        .maxServicedSize = 2 * MemoryConstants::megaByte};

    EXPECT_TRUE(compareUsmPoolParams(expectedUsmHostPoolParams, context->getUsmHostPoolParams()));
    EXPECT_TRUE(compareUsmPoolParams(expectedUsmDevicePoolParams, context->getUsmDevicePoolParams()));
}

TEST_F(ContextUsmPoolParamsTest, GivenUsmPoolAllocatorSupportedWhenInitializingUsmPoolsThenPoolsAreInitializedWithCorrectParams) {
    mockProductHelper->isHostUsmPoolAllocatorSupportedResult = true;
    mockProductHelper->isDeviceUsmPoolAllocatorSupportedResult = true;

    cl_device_id devices[] = {device};
    context.reset(Context::create<MockContext>(nullptr, ClDeviceVector(devices, 1), nullptr, nullptr, retVal));
    EXPECT_EQ(CL_SUCCESS, retVal);

    context->initializeUsmAllocationPools();

    EXPECT_TRUE(context->getHostMemAllocPool().isInitialized());
    EXPECT_TRUE(context->getDeviceMemAllocPool().isInitialized());

    {
        auto mockHostUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&context->getHostMemAllocPool());
        const MockContext::UsmPoolParams givenUsmHostPoolParams{
            .poolSize = mockHostUsmMemAllocPool->poolSize,
            .minServicedSize = mockHostUsmMemAllocPool->minServicedSize,
            .maxServicedSize = mockHostUsmMemAllocPool->maxServicedSize};
        const MockContext::UsmPoolParams expectedUsmHostPoolParams = context->getUsmHostPoolParams();

        EXPECT_TRUE(compareUsmPoolParams(expectedUsmHostPoolParams, givenUsmHostPoolParams));
    }

    {
        auto mockDeviceUsmMemAllocPool = static_cast<MockUsmMemAllocPool *>(&context->getDeviceMemAllocPool());
        const MockContext::UsmPoolParams givenUsmDevicePoolParams{
            .poolSize = mockDeviceUsmMemAllocPool->poolSize,
            .minServicedSize = mockDeviceUsmMemAllocPool->minServicedSize,
            .maxServicedSize = mockDeviceUsmMemAllocPool->maxServicedSize};
        const MockContext::UsmPoolParams expectedUsmDevicePoolParams = context->getUsmDevicePoolParams();

        EXPECT_TRUE(compareUsmPoolParams(expectedUsmDevicePoolParams, givenUsmDevicePoolParams));
    }
}
