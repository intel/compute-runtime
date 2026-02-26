/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/os_inc.h"
#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface_upstream.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/debug_mock_drm_xe.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/linux/mock_drm_memory_manager.h"
#include "shared/test/common/mocks/linux/mock_ioctl_helper.h"
#include "shared/test/common/mocks/linux/mock_os_time_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/os_interface/linux/xe/xe_config_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using IoctlHelperXeTest = Test<XeConfigFixture>;
TEST_F(IoctlHelperXeTest, whenCallingDebuggerOpenIoctlThenProperValueIsReturned) {
    int ret;
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto mockXeIoctlHelper = drm->ioctlHelper.get();

    drm->reset();
    EuDebugConnect test = {};

    ret = mockXeIoctlHelper->ioctl(DrmIoctl::debuggerOpen, &test);
    EXPECT_EQ(ret, drm->debuggerOpenRetval);
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingGetIoctForDebuggerThenCorrectValueReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = drm->getIoctlHelper();
    auto verifyIoctlRequestValue = [&xeIoctlHelper](auto value, DrmIoctl drmIoctl) {
        EXPECT_EQ(xeIoctlHelper->getIoctlRequestValue(drmIoctl), static_cast<unsigned int>(value));
    };
    auto verifyIoctlString = [&xeIoctlHelper](DrmIoctl drmIoctl, const char *string) {
        EXPECT_STREQ(string, xeIoctlHelper->getIoctlString(drmIoctl).c_str());
    };

    verifyIoctlString(DrmIoctl::debuggerOpen, "DRM_IOCTL_XE_EUDEBUG_CONNECT");

    verifyIoctlRequestValue(EuDebugParam::connect, DrmIoctl::debuggerOpen);
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingGetEudebugExtPropertyThenCorrectValueReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());
    EXPECT_EQ(xeIoctlHelper->getEudebugExtProperty(), static_cast<int>(EuDebugParam::execQueueSetPropertyEuDebug));
}

TEST_F(IoctlHelperXeTest, givenIoctlHelperXeWhenCallingGetEudebugExtPropertyValueThenCorrectValueReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(DebuggingMode::offline);
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());
    uint64_t expectedValue = 0;
    auto mockEuDebugInterface = static_cast<NEO::MockEuDebugInterface *>(xeIoctlHelper->euDebugInterface.get());
    mockEuDebugInterface->pageFaultEnableSupported = true;
    expectedValue = static_cast<uint64_t>(EuDebugParam::execQueueSetPropertyValueEnable) | static_cast<uint64_t>(EuDebugParam::execQueueSetPropertyValuePageFaultEnable);
    EXPECT_EQ(xeIoctlHelper->getEudebugExtPropertyValue(), expectedValue);
    mockEuDebugInterface->pageFaultEnableSupported = false;
    expectedValue = static_cast<uint64_t>(EuDebugParam::execQueueSetPropertyValueEnable);
    EXPECT_EQ(xeIoctlHelper->getEudebugExtPropertyValue(), expectedValue);
}

using IoctlHelperXeTestFixture = ::testing::Test;
HWTEST_F(IoctlHelperXeTestFixture, GivenDebuggingDisabledWhenCreateDrmContextThenEuDebuggableContextIsNotRequested) {
    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(DebuggingMode::disabled);
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());

    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);
    drm->engineInfo = std::move(engineInfo);

    OsContextLinux osContext(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular}));
    uint16_t deviceIndex = 1;
    xeIoctlHelper->createDrmContext(*drm, osContext, 0, deviceIndex, false);

    auto ext = drm->receivedContextCreateSetParam;
    EXPECT_NE(ext.property, static_cast<uint32_t>(EuDebugParam::execQueueSetPropertyEuDebug));
}

HWTEST_F(IoctlHelperXeTestFixture, givenDeviceIndexWhenCreatingContextThenSetCorrectGtId) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];

    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());
    xeIoctlHelper->initialize();

    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);
    drm->engineInfo = std::move(engineInfo);

    auto engineDescriptor = EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular});
    OsContextLinux osContext(*drm, 0, 0u, engineDescriptor);

    uint16_t tileId = 1u;
    uint16_t expectedGtId = xeIoctlHelper->tileIdToGtId[tileId];

    xeIoctlHelper->createDrmContext(*drm, osContext, 0, tileId, false);

    EXPECT_EQ(4u, drm->execQueueCreateParams.num_placements);
    ASSERT_EQ(4u, drm->execQueueEngineInstances.size());

    EXPECT_EQ(expectedGtId, drm->execQueueEngineInstances[0].gt_id);
}

HWTEST_F(IoctlHelperXeTestFixture, GivenDebuggingEnabledWhenCreateDrmContextThenEuDebuggableContextIsRequested) {
    DebugManagerStateRestore restorer;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(DebuggingMode::online);
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());

    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);
    drm->engineInfo = std::move(engineInfo);

    OsContextLinux osContext(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular}));
    xeIoctlHelper->createDrmContext(*drm, osContext, 0, 1, false);

    auto ext = drm->receivedContextCreateSetParam;
    EXPECT_EQ(ext.base.name, static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_EXTENSION_SET_PROPERTY));
    EXPECT_EQ(ext.base.next_extension, 0ULL);
    EXPECT_EQ(ext.property, static_cast<uint32_t>(EuDebugParam::execQueueSetPropertyEuDebug));
    EXPECT_EQ(ext.value, static_cast<uint32_t>(EuDebugParam::execQueueSetPropertyValueEnable));
}

HWTEST_F(IoctlHelperXeTestFixture, GivenDebuggingEnabledWhenCreateDrmContextFromGroupThenEuDebuggableContextIsSetCorrectly) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(DebuggingMode::online);
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());

    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);
    drm->engineInfo = std::move(engineInfo);
    executionEnvironment->rootDeviceEnvironments[0]->getMutableHardwareInfo()->gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    OsContextLinux osContext(*drm, 0, 4u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular}));
    OsContextLinux osContext2(*drm, 0, 5u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_CCS, EngineUsage::regular}));

    osContext.setContextGroupCount(8);
    // primary context should create exec_queue with EUDEBUG enable
    osContext.ensureContextInitialized(false);
    auto ext = drm->receivedContextCreateSetParam;
    EXPECT_EQ(ext.base.name, static_cast<uint32_t>(DRM_XE_EXEC_QUEUE_EXTENSION_SET_PROPERTY));
    EXPECT_EQ(ext.property, static_cast<uint32_t>(EuDebugParam::execQueueSetPropertyEuDebug));
    EXPECT_EQ(ext.value, static_cast<uint32_t>(EuDebugParam::execQueueSetPropertyValueEnable));

    EXPECT_NE(ext.base.next_extension, 0ULL);

    osContext2.setPrimaryContext(&osContext);
    drm->receivedContextCreateSetParam = {};
    // secondary context should not create exec_queue with EUDEBUG enable
    osContext2.ensureContextInitialized(false);
    ext = drm->receivedContextCreateSetParam;
    EXPECT_NE(ext.property, static_cast<uint32_t>(EuDebugParam::execQueueSetPropertyEuDebug));
}

HWTEST_F(IoctlHelperXeTestFixture, GivenContextCreatedForCopyEngineWhenCreateDrmContextThenEuDebuggableContextIsNotRequested) {
    DebugManagerStateRestore restorer;

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(DebuggingMode::online);
    auto &rootDeviceEnvironment = *executionEnvironment->rootDeviceEnvironments[0];
    rootDeviceEnvironment.osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment.osInterface->setDriverModel(std::make_unique<DrmMockTime>(mockFd, rootDeviceEnvironment));
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());

    auto engineInfo = xeIoctlHelper->createEngineInfo(false);
    ASSERT_NE(nullptr, engineInfo);
    drm->engineInfo = std::move(engineInfo);

    OsContextLinux osContext(*drm, 0, 0u, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::EngineType::ENGINE_BCS1, EngineUsage::regular}));
    xeIoctlHelper->createDrmContext(*drm, osContext, 0, 0, false);

    auto ext = drm->receivedContextCreateSetParam;
    EXPECT_NE(ext.property, static_cast<uint32_t>(EuDebugParam::execQueueSetPropertyEuDebug));
}

TEST_F(IoctlHelperXeTest, GivenXeDriverThenDebugAttachReturnsTrue) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());
    EXPECT_TRUE(xeIoctlHelper->isDebugAttachAvailable());
}

TEST_F(IoctlHelperXeTest, givenEuDebugSysFsContentWhenItIsZeroThenEuDebugInterfaceIsNotCreated) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);

    VariableBackup<char> euDebugAvailabilityBackup(&MockEuDebugInterface::sysFsContent);

    MockEuDebugInterface::sysFsContent = '1';
    auto euDebugInterface = EuDebugInterface::create(drm->getSysFsPciPath());
    EXPECT_NE(nullptr, euDebugInterface);

    MockEuDebugInterface::sysFsContent = '0';
    euDebugInterface = EuDebugInterface::create(drm->getSysFsPciPath());
    EXPECT_EQ(nullptr, euDebugInterface);
}

TEST_F(IoctlHelperXeTest, givenInvalidPathWhenCreateEuDebugInterfaceThenReturnNullptr) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);

    VariableBackup<size_t> mockFreadReturnBackup(&IoFunctions::mockFreadReturn, 0);
    VariableBackup<const char *> eudebugSysFsEntryBackup(&eudebugSysfsEntry[static_cast<uint32_t>(EuDebugInterfaceType::upstream)], "invalidEntry");

    auto euDebugInterface = EuDebugInterface::create(drm->getSysFsPciPath());
    EXPECT_EQ(nullptr, euDebugInterface);
}

TEST_F(IoctlHelperXeTest, whenEuDebugInterfaceIsCreatedThenEuDebugIsAvailable) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());

    xeIoctlHelper->euDebugInterface.reset();
    EXPECT_EQ(0, xeIoctlHelper->getEuDebugSysFsEnable());
    xeIoctlHelper->euDebugInterface = std::make_unique<MockEuDebugInterface>();
    EXPECT_EQ(1, xeIoctlHelper->getEuDebugSysFsEnable());
}

TEST_F(IoctlHelperXeTest, givenXeRegisterResourceThenCorrectIoctlCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());
    auto &eudebugInterface = xeIoctlHelper->euDebugInterface;
    static_cast<MockEuDebugInterface *>(eudebugInterface.get())->setCurrentInterfaceType(EuDebugInterfaceType::prelim);
    constexpr size_t bufferSize = 20;
    uint8_t buffer[bufferSize];

    auto id = xeIoctlHelper->registerResource(DrmResourceClass::elf, buffer, bufferSize);
    EXPECT_EQ(drm->metadataID, id);
    EXPECT_EQ(drm->metadataAddr, buffer);
    EXPECT_EQ(drm->metadataSize, bufferSize);
    EXPECT_EQ(drm->metadataType, static_cast<uint64_t>(EuDebugParam::metadataElfBinary));

    drm->metadataID = 0;
    drm->metadataAddr = nullptr;
    drm->metadataSize = 0;
    id = xeIoctlHelper->registerResource(DrmResourceClass::l0ZebinModule, buffer, bufferSize);
    EXPECT_EQ(drm->metadataID, id);
    EXPECT_EQ(drm->metadataAddr, buffer);
    EXPECT_EQ(drm->metadataSize, bufferSize);
    EXPECT_EQ(drm->metadataType, static_cast<uint64_t>(EuDebugParam::metadataProgramModule));

    drm->metadataID = 0;
    drm->metadataAddr = nullptr;
    drm->metadataSize = 0;
    id = xeIoctlHelper->registerResource(DrmResourceClass::contextSaveArea, buffer, bufferSize);
    EXPECT_EQ(drm->metadataID, id);
    EXPECT_EQ(drm->metadataAddr, buffer);
    EXPECT_EQ(drm->metadataSize, bufferSize);
    EXPECT_EQ(drm->metadataType, static_cast<uint64_t>(EuDebugParam::metadataSipArea));

    drm->metadataID = 0;
    drm->metadataAddr = nullptr;
    drm->metadataSize = 0;
    id = xeIoctlHelper->registerResource(DrmResourceClass::sbaTrackingBuffer, buffer, bufferSize);
    EXPECT_EQ(drm->metadataID, id);
    EXPECT_EQ(drm->metadataAddr, buffer);
    EXPECT_EQ(drm->metadataSize, bufferSize);
    EXPECT_EQ(drm->metadataType, static_cast<uint64_t>(EuDebugParam::metadataSbaArea));
    drm->metadataID = 0;
    drm->metadataAddr = nullptr;
    drm->metadataSize = 0;
    id = xeIoctlHelper->registerResource(DrmResourceClass::moduleHeapDebugArea, buffer, bufferSize);
    EXPECT_EQ(drm->metadataID, id);
    EXPECT_EQ(drm->metadataAddr, buffer);
    EXPECT_EQ(drm->metadataSize, bufferSize);
    EXPECT_EQ(drm->metadataType, static_cast<uint64_t>(EuDebugParam::metadataModuleArea));
}

TEST_F(IoctlHelperXeTest, givenXeunregisterResourceThenCorrectIoctlCalled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());
    auto &eudebugInterface = xeIoctlHelper->euDebugInterface;
    static_cast<MockEuDebugInterface *>(eudebugInterface.get())->setCurrentInterfaceType(EuDebugInterfaceType::prelim);

    xeIoctlHelper->unregisterResource(0x1234);
    EXPECT_EQ(drm->metadataID, 0x1234u);
}

TEST_F(IoctlHelperXeTest, whenGettingVmBindExtFromHandlesThenProperStructsAreReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());
    auto &eudebugInterface = xeIoctlHelper->euDebugInterface;
    static_cast<MockEuDebugInterface *>(eudebugInterface.get())->setCurrentInterfaceType(EuDebugInterfaceType::prelim);

    StackVec<uint32_t, 2> bindExtHandles;
    bindExtHandles.push_back(1u);
    bindExtHandles.push_back(2u);
    bindExtHandles.push_back(3u);
    auto retVal = xeIoctlHelper->prepareVmBindExt(bindExtHandles, 1);
    auto vmBindExt = reinterpret_cast<VmBindOpExtAttachDebug *>(retVal.get());

    for (size_t i = 0; i < bindExtHandles.size(); i++) {

        EXPECT_EQ(bindExtHandles[i], vmBindExt[i].metadataId);
        EXPECT_EQ(static_cast<uint32_t>(EuDebugParam::vmBindOpExtensionsAttachDebug), vmBindExt[i].base.name);
        EXPECT_EQ(1u, vmBindExt[i].cookie);
    }

    EXPECT_EQ(reinterpret_cast<uintptr_t>(&vmBindExt[1]), vmBindExt[0].base.nextExtension);
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&vmBindExt[2]), vmBindExt[1].base.nextExtension);
}

TEST_F(IoctlHelperXeTest, givenUpstreamEuDebugInterfaceThenRegisterAndUnregisterResourceAndPrepareVmBindExtReturnEarly) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());
    auto &eudebugInterface = xeIoctlHelper->euDebugInterface;
    static_cast<MockEuDebugInterface *>(eudebugInterface.get())->setCurrentInterfaceType(EuDebugInterfaceType::upstream);

    constexpr size_t bufferSize = 20;
    uint8_t buffer[bufferSize];

    auto id = xeIoctlHelper->registerResource(DrmResourceClass::elf, buffer, bufferSize);
    EXPECT_EQ(id, 0u);

    drm->metadataID = 0x6789u;
    xeIoctlHelper->unregisterResource(0x1234);
    EXPECT_EQ(drm->metadataID, 0x6789u);

    StackVec<uint32_t, 2> bindExtHandles;
    auto retVal = xeIoctlHelper->prepareVmBindExt(bindExtHandles, 1);
    EXPECT_EQ(retVal, nullptr);
}

TEST_F(IoctlHelperXeTest, givenRegisterIsaHandleWhenIsaIsTileInstancedThenBOCookieSet) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    xeIoctlHelper->euDebugInterface = std::make_unique<MockEuDebugInterface>();
    auto &eudebugInterface = xeIoctlHelper->euDebugInterface;
    static_cast<MockEuDebugInterface *>(eudebugInterface.get())->setCurrentInterfaceType(EuDebugInterfaceType::prelim);

    drm.ioctlHelper.reset(xeIoctlHelper.release());

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.tileInstanced = true;
    allocation.storageInfo.subDeviceBitfield.set();
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(bo.getRegisteredBindHandleCookie(), allocation.storageInfo.subDeviceBitfield.to_ulong());
}

class IoctlHelperXeDebugDataTest : public Test<XeConfigFixture> {
  public:
    void SetUp() override {
        auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
        drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
        xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());
    }
    const uint32_t rootDeviceIndex = 0u;
    std::unique_ptr<DrmMockXeDebug> drm;
    MockIoctlHelperXeDebug *xeIoctlHelper = nullptr;
};

TEST_F(IoctlHelperXeDebugDataTest, givenUpstreamDebuggerWhenRegisterBOBindHandleCalledWithEmptyBOTheValidBOIsRegistered) {
    xeIoctlHelper->euDebugInterface = std::make_unique<MockEuDebugInterface>();
    auto &eudebugInterface = xeIoctlHelper->euDebugInterface;
    static_cast<MockEuDebugInterface *>(eudebugInterface.get())->setCurrentInterfaceType(EuDebugInterfaceType::upstream);

    MockBufferObject bo(rootDeviceIndex, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::localMemory);
    allocation.bufferObjects[0] = nullptr;
    allocation.bufferObjects[1] = &bo;
    allocation.registerBOBindExtHandle(drm.get());

    EXPECT_EQ(bo.getDrmResourceClass(), DrmResourceClass::contextSaveArea);
}

TEST_F(IoctlHelperXeDebugDataTest, givenPseudoClassesWhenCallingConvertDrmResourceClassToXeDebugPseudoPathThenCorrectPseudoPathReturned) {
    xeIoctlHelper->euDebugInterface = std::make_unique<EuDebugInterfaceUpstream>();

    EXPECT_EQ(0x1u, xeIoctlHelper->convertDrmResourceClassToXeDebugPseudoPath(DrmResourceClass::moduleHeapDebugArea));
    EXPECT_EQ(0x2u, xeIoctlHelper->convertDrmResourceClassToXeDebugPseudoPath(DrmResourceClass::sbaTrackingBuffer));
    EXPECT_EQ(0x3u, xeIoctlHelper->convertDrmResourceClassToXeDebugPseudoPath(DrmResourceClass::contextSaveArea));
}

TEST_F(IoctlHelperXeDebugDataTest, givenNonPseudoClassesWhenCallingConvertDrmResourceClassToXeDebugPseudoPathThenZeroPseudoPathReturned) {
    xeIoctlHelper->euDebugInterface = std::make_unique<MockEuDebugInterface>();
    auto &eudebugInterface = xeIoctlHelper->euDebugInterface;
    static_cast<MockEuDebugInterface *>(eudebugInterface.get())->setCurrentInterfaceType(EuDebugInterfaceType::upstream);

    EXPECT_EQ(0x0u, xeIoctlHelper->convertDrmResourceClassToXeDebugPseudoPath(DrmResourceClass::elf));
    EXPECT_EQ(0x0u, xeIoctlHelper->convertDrmResourceClassToXeDebugPseudoPath(DrmResourceClass::isa));
    EXPECT_EQ(0x0u, xeIoctlHelper->convertDrmResourceClassToXeDebugPseudoPath(DrmResourceClass::contextID));
    EXPECT_EQ(0x0u, xeIoctlHelper->convertDrmResourceClassToXeDebugPseudoPath(DrmResourceClass::l0ZebinModule));
    EXPECT_EQ(0x0u, xeIoctlHelper->convertDrmResourceClassToXeDebugPseudoPath(DrmResourceClass::cookie));
}

TEST_F(IoctlHelperXeDebugDataTest, givenUpstreamDebuggerWhenAddDebugDataAndCreateBindOpVecCalledThenVectorWithDebugDataReturned) {
    xeIoctlHelper->euDebugInterface = std::make_unique<EuDebugInterfaceUpstream>();

    uint32_t vmId = 1;
    auto isAdd = false;
    {
        MockBufferObject bo(rootDeviceIndex, drm.get(), 3, 0, 0, 1);
        bo.gpuAddress = 0x1234;
        bo.size = 0x1000;
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::localMemory);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(drm.get());

        auto result = drm->ioctlHelper->addDebugDataAndCreateBindOpVec(&bo, vmId, isAdd);
        EXPECT_NE(std::nullopt, result);
        auto debugData = result.value()[0];

        EXPECT_EQ(debugData.base.name, 0u);
        EXPECT_EQ(debugData.base.nextExtension, 0u);
        EXPECT_EQ(debugData.flags, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataPseudoFlag));
        EXPECT_EQ(debugData.pseudopath, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataSipArea));
        EXPECT_EQ(debugData.addr, 0x1234u);
        EXPECT_EQ(debugData.range, 0x1000u);
        EXPECT_EQ(debugData.offset, 0u);
    }

    {
        MockBufferObject bo(rootDeviceIndex, drm.get(), 3, 0, 0, 1);
        bo.gpuAddress = 0x2345;
        bo.size = 0x1001;

        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::localMemory);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(drm.get());

        auto result = drm->ioctlHelper->addDebugDataAndCreateBindOpVec(&bo, vmId, isAdd);
        EXPECT_NE(std::nullopt, result);
        auto debugData = result.value()[0];

        EXPECT_EQ(debugData.base.name, 0u);
        EXPECT_EQ(debugData.base.nextExtension, 0u);
        EXPECT_EQ(debugData.flags, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataPseudoFlag));
        EXPECT_EQ(debugData.pseudopath, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataSbaArea));
        EXPECT_EQ(debugData.addr, 0x2345u);
        EXPECT_EQ(debugData.range, 0x1001u);
        EXPECT_EQ(debugData.offset, 0u);
    }

    {
        MockBufferObject bo(rootDeviceIndex, drm.get(), 3, 0, 0, 1);
        bo.gpuAddress = 0x3456;
        bo.size = 0x1002;
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugModuleArea, MemoryPool::localMemory);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(drm.get());

        auto result = drm->ioctlHelper->addDebugDataAndCreateBindOpVec(&bo, vmId, isAdd);
        EXPECT_NE(std::nullopt, result);
        auto debugData = result.value()[0];

        EXPECT_EQ(debugData.base.name, 0u);
        EXPECT_EQ(debugData.base.nextExtension, 0u);
        EXPECT_EQ(debugData.flags, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataPseudoFlag));
        EXPECT_EQ(debugData.pseudopath, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataModuleArea));
        EXPECT_EQ(debugData.addr, 0x3456u);
        EXPECT_EQ(debugData.range, 0x1002u);
        EXPECT_EQ(debugData.offset, 0u);
    }
}

TEST_F(IoctlHelperXeDebugDataTest, givenIsaClassWhenAddDebugDataAndCreateBindOpVecCalledThenVectorWithDebugDataReturnedAndEntryAddedToBindInfoMap) {
    xeIoctlHelper->euDebugInterface = std::make_unique<EuDebugInterfaceUpstream>();

    uint32_t vmId = 1;
    auto isAdd = true;
    uint32_t isaDebugDataHandle = 0x5678;

    MockBufferObject bo(rootDeviceIndex, drm.get(), 3, 0, 0, 1);
    bo.gpuAddress = 0x1234;
    bo.size = 0x1000;
    MockBufferObject bo2(rootDeviceIndex, drm.get(), 3, 0, 0, 1);
    bo2.gpuAddress = 0x5678;
    bo2.size = 0x1000;

    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(drm.get());
    allocation.setDrmResourceClass(DrmResourceClass::isa);

    MockDrmAllocation allocation2(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::localMemory);
    allocation2.bufferObjects[0] = &bo2;
    allocation2.registerBOBindExtHandle(drm.get());
    allocation2.setDrmResourceClass(DrmResourceClass::isa);

    bo.isaDebugDataHandle = isaDebugDataHandle;
    bo2.isaDebugDataHandle = isaDebugDataHandle;

    struct IsaDebugData isaDebugData {};
    isaDebugData.totalSegments = 2;
    strcpy_s(isaDebugData.elfPath, sizeof(isaDebugData.elfPath), "mockElfPath");

    drm->isaDebugDataMap[isaDebugDataHandle] = isaDebugData;

    auto result = drm->ioctlHelper->addDebugDataAndCreateBindOpVec(&bo, vmId, isAdd);
    EXPECT_EQ(1u, drm->isaDebugDataMap[isaDebugDataHandle].bindInfoMap[vmId].size());
    EXPECT_EQ(std::nullopt, result);

    result = drm->ioctlHelper->addDebugDataAndCreateBindOpVec(&bo2, vmId, isAdd);
    EXPECT_EQ(2u, drm->isaDebugDataMap[isaDebugDataHandle].bindInfoMap[vmId].size());
    EXPECT_NE(std::nullopt, result);

    EXPECT_EQ(2u, result.value().size());
    auto data1 = result.value()[0];
    auto data2 = result.value()[1];

    EXPECT_EQ(data1.base.name, 0u);
    EXPECT_EQ(data1.base.nextExtension, 0u);
    EXPECT_EQ(data1.flags, 0u);
    EXPECT_STREQ(data1.pathname, "mockElfPath");
    EXPECT_EQ(data1.addr, bo.gpuAddress);
    EXPECT_EQ(data1.range, bo.size);
    EXPECT_EQ(data1.offset, 0u);

    EXPECT_EQ(data2.base.name, 0u);
    EXPECT_EQ(data2.base.nextExtension, 0u);
    EXPECT_EQ(data2.flags, 0u);
    EXPECT_STREQ(data2.pathname, "mockElfPath");
    EXPECT_EQ(data2.addr, bo2.gpuAddress);
    EXPECT_EQ(data2.range, bo2.size);
    EXPECT_EQ(data2.offset, 0u);
}

TEST_F(IoctlHelperXeDebugDataTest, givenSingleIsaWhenAddDebugDataAndCreateBindOpVecCalledWithNotAddThenNulloptReturned) {
    xeIoctlHelper->euDebugInterface = std::make_unique<EuDebugInterfaceUpstream>();

    uint32_t vmId = 1;
    auto isAdd = false;
    uint32_t isaDebugDataHandle = 0x5678;

    MockBufferObject bo(rootDeviceIndex, drm.get(), 3, 0, 0, 1);
    bo.gpuAddress = 0x1234;
    bo.size = 0x1000;
    MockBufferObject bo2(rootDeviceIndex, drm.get(), 3, 0, 0, 1);
    bo2.gpuAddress = 0x1234;
    bo2.size = 0x100;
    MockBufferObject bo3(rootDeviceIndex, drm.get(), 3, 0, 0, 1);
    bo3.gpuAddress = 0x5678;
    bo3.size = 0x1000;
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.bufferObjects[1] = &bo2;
    allocation.bufferObjects[2] = &bo3;
    allocation.registerBOBindExtHandle(drm.get());
    allocation.setDrmResourceClass(DrmResourceClass::isa);

    bo.isaDebugDataHandle = isaDebugDataHandle;
    bo2.isaDebugDataHandle = isaDebugDataHandle;
    bo3.isaDebugDataHandle = isaDebugDataHandle;

    struct IsaDebugData isaDebugData {};
    isaDebugData.totalSegments = 3;
    strcpy_s(isaDebugData.elfPath, sizeof(isaDebugData.elfPath), "mockElfPath");
    std::vector<IsaDebugData::DebugDataBindInfo> bindInfoVec({{bo.gpuAddress, bo.size}, {bo2.gpuAddress, bo2.size}, {bo3.gpuAddress, bo3.size}});
    isaDebugData.bindInfoMap[vmId] = bindInfoVec;

    drm->isaDebugDataMap[isaDebugDataHandle] = isaDebugData;

    auto result = drm->ioctlHelper->addDebugDataAndCreateBindOpVec(&bo, vmId, isAdd);
    EXPECT_NE(std::nullopt, result);
    EXPECT_EQ(3u, result.value().size());
    auto data1 = result.value()[0];
    auto data2 = result.value()[1];
    auto data3 = result.value()[2];

    EXPECT_EQ(data1.base.name, 0u);
    EXPECT_EQ(data1.base.nextExtension, 0u);
    EXPECT_EQ(data1.flags, 0u);
    EXPECT_STREQ(data1.pathname, "mockElfPath");
    EXPECT_EQ(data1.addr, bo.gpuAddress);
    EXPECT_EQ(data1.range, bo.size);
    EXPECT_EQ(data1.offset, 0u);

    EXPECT_EQ(data2.base.name, 0u);
    EXPECT_EQ(data2.base.nextExtension, 0u);
    EXPECT_EQ(data2.flags, 0u);
    EXPECT_STREQ(data2.pathname, "mockElfPath");
    EXPECT_EQ(data2.addr, bo2.gpuAddress);
    EXPECT_EQ(data2.range, bo2.size);
    EXPECT_EQ(data2.offset, 0u);

    EXPECT_EQ(data3.base.name, 0u);
    EXPECT_EQ(data3.base.nextExtension, 0u);
    EXPECT_EQ(data3.flags, 0u);
    EXPECT_STREQ(data3.pathname, "mockElfPath");
    EXPECT_EQ(data3.addr, bo3.gpuAddress);
    EXPECT_EQ(data3.range, bo3.size);
    EXPECT_EQ(data3.offset, 0u);

    // 1 less entry expected since bo is removed
    EXPECT_EQ(2u, drm->isaDebugDataMap[isaDebugDataHandle].bindInfoMap[vmId].size());

    result = drm->ioctlHelper->addDebugDataAndCreateBindOpVec(&bo2, vmId, isAdd);
    EXPECT_EQ(std::nullopt, result);
    EXPECT_EQ(1u, drm->isaDebugDataMap[isaDebugDataHandle].bindInfoMap[vmId].size());

    result = drm->ioctlHelper->addDebugDataAndCreateBindOpVec(&bo3, vmId, isAdd);
    EXPECT_EQ(std::nullopt, result);
    EXPECT_EQ(0u, drm->isaDebugDataMap[isaDebugDataHandle].bindInfoMap[vmId].size());
}

TEST_F(IoctlHelperXeDebugDataTest, givenInvalidResourceClassWhenAddDebugDataAndCreateBindOpVecThenNulloptReturnedAndNoEntryAddedToBindInfoMap) {
    xeIoctlHelper->euDebugInterface = std::make_unique<EuDebugInterfaceUpstream>();

    uint32_t vmId = 1;
    MockBufferObject bo(rootDeviceIndex, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::buffer, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(drm.get());

    EXPECT_EQ(std::nullopt, xeIoctlHelper->addDebugDataAndCreateBindOpVec(&bo, vmId, false));
    EXPECT_EQ(0u, drm->isaDebugDataMap.size());
}

TEST_F(IoctlHelperXeDebugDataTest, givenPseudoDebugDataWhenCallbindAddDebugDataThenBindSucceedsAndIoctlCalledWithProperArguments) {
    xeIoctlHelper->euDebugInterface = std::make_unique<EuDebugInterfaceUpstream>();

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    uint32_t vmId = 1;
    VmBindExtUserFenceT userFence{};
    xeIoctlHelper->fillVmBindExtUserFence(userFence, fenceAddress, fenceValue, 0);
    std::vector<VmBindOpExtDebugData> debugDataVec;
    VmBindOpExtDebugData debugData{};
    debugData.base.name = 0;
    debugData.base.nextExtension = 0;
    debugData.base.pad = 0;
    debugData.addr = 0x1234;
    debugData.range = 0x1000;
    debugData.flags = xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataPseudoFlag);
    debugData.pseudopath = xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataModuleArea);
    debugDataVec.push_back(debugData);
    auto retVal = xeIoctlHelper->bindAddDebugData(debugDataVec, vmId, &userFence, true);

    EXPECT_EQ(retVal, 0);

    auto bind = drm->gemVmBindInput;

    EXPECT_EQ(bind.vm_id, 1u);
    EXPECT_EQ(bind.num_binds, 1u);
    EXPECT_EQ(bind.num_syncs, 1u);

    auto &drmSyncs = drm->gemVmBindSyncs;
    EXPECT_EQ(drmSyncs[0].type, static_cast<uint32_t>(DRM_XE_SYNC_TYPE_USER_FENCE));
    EXPECT_EQ(drmSyncs[0].addr, 0x4321u);
    EXPECT_EQ(drmSyncs[0].flags, static_cast<uint32_t>(DRM_XE_SYNC_FLAG_SIGNAL));
    EXPECT_EQ(drmSyncs[0].timeline_value, 0x789u);

    EXPECT_EQ(bind.pad, 0u);
    EXPECT_EQ(bind.pad2, 0u);

    // drm_xe_vm_bind_op
    EXPECT_EQ(bind.bind.pad, 0u);
    EXPECT_EQ(bind.bind.addr, 0u);
    EXPECT_EQ(bind.bind.range, 0u);
    EXPECT_EQ(bind.bind.flags, 0u);

    EXPECT_EQ(bind.bind.op, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsAddDebugData));

    auto bindDebugData = drm->gemVmBindDebugData.get();
    EXPECT_EQ(bindDebugData->base.name, 0u);
    EXPECT_EQ(bindDebugData->base.nextExtension, 0u);
    EXPECT_EQ(bindDebugData->base.pad, 0u);
    EXPECT_EQ(bindDebugData->addr, 0x1234u);
    EXPECT_EQ(bindDebugData->range, 0x1000u);
    EXPECT_EQ(bindDebugData->flags, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataPseudoFlag));
    EXPECT_EQ(bindDebugData->pseudopath, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataModuleArea));

    EXPECT_EQ(drm->waitUserFenceInputs.size(), 1u);
    auto fenceData = drm->waitUserFenceInputs[0];
    EXPECT_EQ(fenceData.addr, 0x4321u);
    EXPECT_EQ(fenceData.value, 0x789u);
}

TEST_F(IoctlHelperXeDebugDataTest, givenNonPseudoDebugDataWhenCallbindAddDebugDataThenBindSucceedsAndDebugDataSetCorrectlyAndFenceSet) {
    xeIoctlHelper->euDebugInterface = std::make_unique<EuDebugInterfaceUpstream>();

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    uint32_t vmId = 1;
    VmBindExtUserFenceT userFence{};
    xeIoctlHelper->fillVmBindExtUserFence(userFence, fenceAddress, fenceValue, 0);
    std::vector<VmBindOpExtDebugData> debugDataVec;
    VmBindOpExtDebugData debugData{};
    debugData.base.name = 0;
    debugData.base.nextExtension = 0;
    debugData.base.pad = 0;
    debugData.addr = 0x1234;
    debugData.range = 0x1000;
    debugData.flags = 0;
    debugData.pseudopath = 0;
    debugDataVec.push_back(debugData);
    auto retVal = xeIoctlHelper->bindAddDebugData(debugDataVec, vmId, &userFence, true);
    EXPECT_EQ(retVal, 0);

    EXPECT_EQ(drm->waitUserFenceInputs.size(), 1u);
    auto fenceData = drm->waitUserFenceInputs[0];
    EXPECT_EQ(fenceData.addr, 0x4321u);
    EXPECT_EQ(fenceData.value, 0x789u);
}

TEST_F(IoctlHelperXeDebugDataTest, givenNonPseudoDebugDataWhenCallbindAddDebugDataWithNotAddThenBindSucceedsAndDebugDataSetCorrectlyAndFenceNotSet) {
    xeIoctlHelper->euDebugInterface = std::make_unique<EuDebugInterfaceUpstream>();

    drm->waitUserFenceInputs.clear();
    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    uint32_t vmId = 1;
    VmBindExtUserFenceT userFence{};
    xeIoctlHelper->fillVmBindExtUserFence(userFence, fenceAddress, fenceValue, 0);
    std::vector<VmBindOpExtDebugData> debugDataVec;
    VmBindOpExtDebugData debugData{};
    debugData.base.name = 0;
    debugData.base.nextExtension = 0;
    debugData.base.pad = 0;
    debugData.addr = 0x1234;
    debugData.range = 0x1000;
    debugData.flags = 0;
    debugData.pseudopath = 0;
    debugDataVec.push_back(debugData);
    auto retVal = xeIoctlHelper->bindAddDebugData(debugDataVec, vmId, &userFence, false);
    EXPECT_EQ(retVal, 0);

    EXPECT_EQ(drm->waitUserFenceInputs.size(), 0u);
}

TEST_F(IoctlHelperXeDebugDataTest, givenPseudoDebugDataWhenCallbindAddDebugDataWithNotAddThenBindSucceedsAndFenceNotSet) {
    xeIoctlHelper->euDebugInterface = std::make_unique<EuDebugInterfaceUpstream>();

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    uint32_t vmId = 1;
    VmBindExtUserFenceT userFence{};
    xeIoctlHelper->fillVmBindExtUserFence(userFence, fenceAddress, fenceValue, 0);
    std::vector<VmBindOpExtDebugData> debugDataVec;
    VmBindOpExtDebugData debugData{};
    debugData.base.name = 0;
    debugData.base.nextExtension = 0;
    debugData.base.pad = 0;
    debugData.addr = 0x1234;
    debugData.range = 0x1000;
    debugData.flags = xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataPseudoFlag);
    debugData.pseudopath = xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataModuleArea);
    debugDataVec.push_back(debugData);
    auto retVal = xeIoctlHelper->bindAddDebugData(debugDataVec, vmId, &userFence, false);
    EXPECT_EQ(retVal, 0);

    auto bind = drm->gemVmBindInput;
    EXPECT_EQ(bind.vm_id, 1u);
    EXPECT_EQ(bind.num_binds, 1u);
    EXPECT_EQ(bind.num_syncs, 0u);

    EXPECT_EQ(bind.pad, 0u);
    EXPECT_EQ(bind.pad2, 0u);

    // drm_xe_vm_bind_op
    EXPECT_EQ(bind.bind.pad, 0u);
    EXPECT_EQ(bind.bind.addr, 0u);
    EXPECT_EQ(bind.bind.range, 0u);
    EXPECT_EQ(bind.bind.flags, 0u);

    EXPECT_EQ(bind.bind.op, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsRemoveDebugData));

    auto bindDebugData = drm->gemVmBindDebugData.get();
    EXPECT_EQ(bindDebugData->base.name, 0u);
    EXPECT_EQ(bindDebugData->base.nextExtension, 0u);
    EXPECT_EQ(bindDebugData->base.pad, 0u);
    EXPECT_EQ(bindDebugData->addr, 0x1234u);
    EXPECT_EQ(bindDebugData->range, 0x1000u);
    EXPECT_EQ(bindDebugData->flags, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataPseudoFlag));
    EXPECT_EQ(bindDebugData->pseudopath, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataModuleArea));

    EXPECT_EQ(drm->waitUserFenceInputs.size(), 0u);
}

TEST_F(IoctlHelperXeDebugDataTest, givenMultiplePseudoDebugDataWhenCallbindAddDebugDataThenVectorOfBindsSetAndDataCorrectForEachBindAndFenceSet) {
    xeIoctlHelper->euDebugInterface = std::make_unique<EuDebugInterfaceUpstream>();

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    uint32_t vmId = 1;
    VmBindExtUserFenceT userFence{};
    xeIoctlHelper->fillVmBindExtUserFence(userFence, fenceAddress, fenceValue, 0);
    std::vector<VmBindOpExtDebugData> debugDataVec;

    VmBindOpExtDebugData debugData{};
    debugData.base.name = 0;
    debugData.base.nextExtension = 0;
    debugData.base.pad = 0;
    debugData.addr = 0x1234;
    debugData.range = 0x1000;
    debugData.flags = xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataPseudoFlag);
    debugData.pseudopath = xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataModuleArea);

    VmBindOpExtDebugData debugData2{};
    debugData2.base.name = 0;
    debugData2.base.nextExtension = 0;
    debugData2.base.pad = 0;
    debugData2.addr = 0x5678;
    debugData2.range = 0x1000;
    debugData2.flags = xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataPseudoFlag);
    debugData2.pseudopath = xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataSbaArea);

    debugDataVec.push_back(debugData);
    debugDataVec.push_back(debugData2);

    auto retVal = xeIoctlHelper->bindAddDebugData(debugDataVec, vmId, &userFence, true);
    EXPECT_EQ(retVal, 0);

    auto bind = drm->gemVmBindInput;
    EXPECT_EQ(bind.vm_id, 1u);
    EXPECT_EQ(bind.num_binds, 2u);
    EXPECT_EQ(bind.num_syncs, 1u);

    auto &drmSyncs = drm->gemVmBindSyncs;
    EXPECT_EQ(drmSyncs[0].type, static_cast<uint32_t>(DRM_XE_SYNC_TYPE_USER_FENCE));
    EXPECT_EQ(drmSyncs[0].addr, 0x4321u);
    EXPECT_EQ(drmSyncs[0].flags, static_cast<uint32_t>(DRM_XE_SYNC_FLAG_SIGNAL));
    EXPECT_EQ(drmSyncs[0].timeline_value, 0x789u);

    EXPECT_EQ(bind.pad, 0u);
    EXPECT_EQ(bind.pad2, 0u);

    // drm_xe_vm_bind_op
    auto binds = reinterpret_cast<drm_xe_vm_bind_op *>(bind.vector_of_binds);
    EXPECT_EQ(binds[0].op, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsAddDebugData));
    EXPECT_EQ(binds[1].op, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsAddDebugData));

    auto bindDebugData = drm->gemVmBindDebugData.get();
    EXPECT_EQ(bindDebugData->base.name, 0u);
    EXPECT_EQ(bindDebugData->base.nextExtension, 0u);
    EXPECT_EQ(bindDebugData->base.pad, 0u);
    EXPECT_EQ(bindDebugData->addr, 0x1234u);
    EXPECT_EQ(bindDebugData->range, 0x1000u);
    EXPECT_EQ(bindDebugData->flags, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataPseudoFlag));
    EXPECT_EQ(bindDebugData->pseudopath, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataModuleArea));

    auto bindDebugData2 = bindDebugData + 1;
    EXPECT_EQ(bindDebugData2->base.name, 0u);
    EXPECT_EQ(bindDebugData2->base.nextExtension, 0u);
    EXPECT_EQ(bindDebugData2->base.pad, 0u);
    EXPECT_EQ(bindDebugData2->addr, 0x5678u);
    EXPECT_EQ(bindDebugData2->range, 0x1000u);
    EXPECT_EQ(bindDebugData2->flags, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataPseudoFlag));
    EXPECT_EQ(bindDebugData2->pseudopath, xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataSbaArea));

    EXPECT_EQ(drm->waitUserFenceInputs.size(), 1u);
    auto fenceData = drm->waitUserFenceInputs[0];
    EXPECT_EQ(fenceData.addr, 0x4321u);
    EXPECT_EQ(fenceData.value, 0x789u);
}

TEST_F(IoctlHelperXeDebugDataTest, givenIoctlFailsWhenCallbindAddDebugDataThenBindFailsAndFenceNotSet) {
    xeIoctlHelper->euDebugInterface = std::make_unique<EuDebugInterfaceUpstream>();

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    drm->waitUserFenceInputs.clear();
    uint32_t vmId = 1;
    VmBindExtUserFenceT userFence{};
    xeIoctlHelper->fillVmBindExtUserFence(userFence, fenceAddress, fenceValue, 0);
    std::vector<VmBindOpExtDebugData> debugDataVec;
    VmBindOpExtDebugData debugData{};
    debugData.base.name = 0;
    debugData.base.nextExtension = 0;
    debugData.base.pad = 0;
    debugData.addr = 0x1234;
    debugData.range = 0x1000;
    debugData.flags = xeIoctlHelper->euDebugInterface->getParamValue(EuDebugParam::vmBindOpExtensionsDebugDataPseudoFlag);
    debugData.pseudopath = 0x1;
    debugDataVec.push_back(debugData);

    drm->gemVmBindReturn = -1;
    auto retVal = xeIoctlHelper->bindAddDebugData(debugDataVec, vmId, &userFence, true);
    EXPECT_NE(retVal, 0);

    EXPECT_TRUE(drm->gemVmBindCalled);
    EXPECT_EQ(drm->waitUserFenceInputs.size(), 0u);
}

TEST_F(IoctlHelperXeTest, givenRegisterIsaHandleWhenIsaIsNotTileInstancedThenBOCookieNotSet) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    xeIoctlHelper->euDebugInterface = std::make_unique<MockEuDebugInterface>();
    auto &eudebugInterface = xeIoctlHelper->euDebugInterface;
    static_cast<MockEuDebugInterface *>(eudebugInterface.get())->setCurrentInterfaceType(EuDebugInterfaceType::prelim);

    drm.ioctlHelper.reset(xeIoctlHelper.release());

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.tileInstanced = false;
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(bo.getRegisteredBindHandleCookie(), 0u);
}

TEST_F(IoctlHelperXeTest, givenResourceRegistrationEnabledWhenAllocationTypeShouldBeRegisteredThenBoHasBindExtHandleAdded) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);
    xeIoctlHelper->euDebugInterface = std::make_unique<MockEuDebugInterface>();
    auto &eudebugInterface = xeIoctlHelper->euDebugInterface;
    static_cast<MockEuDebugInterface *>(eudebugInterface.get())->setCurrentInterfaceType(EuDebugInterfaceType::prelim);

    drm.ioctlHelper.reset(xeIoctlHelper.release());

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::contextSaveArea, drm.registeredClass);
    }
    drm.registeredClass = DrmResourceClass::maxSize;

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::sbaTrackingBuffer, drm.registeredClass);
    }
    drm.registeredClass = DrmResourceClass::maxSize;

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugModuleArea, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(DrmMockResources::registerResourceReturnHandle, bo.bindExtHandles[0]);
        EXPECT_EQ(DrmResourceClass::moduleHeapDebugArea, drm.registeredClass);
    }

    drm.registeredClass = DrmResourceClass::maxSize;

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::bufferHostMemory, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(0u, bo.bindExtHandles.size());
        EXPECT_EQ(DrmResourceClass::maxSize, drm.registeredClass);
    }
}

TEST_F(IoctlHelperXeTest, givenResourceRegistrationEnabledWhenAllocationTypeShouldNotBeRegisteredThenNoBindHandleCreated) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);

    drm.registeredClass = DrmResourceClass::maxSize;
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);

    drm.ioctlHelper.reset(xeIoctlHelper.release());

    {
        MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsaInternal, MemoryPool::system4KBPages);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(&drm);
        EXPECT_EQ(0u, bo.bindExtHandles.size());
    }
    EXPECT_EQ(DrmResourceClass::maxSize, drm.registeredClass);
}

TEST_F(IoctlHelperXeTest, givenDebuggingEnabledWhenCallingVmBindThenWaitUserFenceIsCalledWithCorrectTimeout) {

    DebugManagerStateRestore restorer;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(DebuggingMode::online);
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(*drm);

    auto expectedTimeout = -1;

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    auto handle = 0x1234u;

    VmBindExtUserFenceT vmBindExtUserFence{};
    xeIoctlHelper->fillVmBindExtUserFence(vmBindExtUserFence, fenceAddress, fenceValue, 0u);

    VmBindParams vmBindParams{};
    vmBindParams.handle = handle;
    xeIoctlHelper->setVmBindUserFence(vmBindParams, vmBindExtUserFence);

    EXPECT_EQ(0, xeIoctlHelper->vmBind(vmBindParams));
    auto &waitUserFence = drm->waitUserFenceInputs[0];
    EXPECT_EQ(expectedTimeout, waitUserFence.timeout);
    drm->waitUserFenceInputs.clear();
    EXPECT_EQ(0, xeIoctlHelper->vmUnbind(vmBindParams));
    waitUserFence = drm->waitUserFenceInputs[0];
    EXPECT_EQ(expectedTimeout, waitUserFence.timeout);
}

TEST_F(IoctlHelperXeTest, givenDebuggingEnabledWhenCallinggetFlagsForVmCreateThenFaultModeIsEnabled) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->setDebuggingMode(DebuggingMode::online);
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(*drm);
    uint32_t flags = xeIoctlHelper->getFlagsForVmCreate(false, false, false);
    EXPECT_EQ(flags & DRM_XE_VM_CREATE_FLAG_FAULT_MODE, static_cast<uint32_t>(DRM_XE_VM_CREATE_FLAG_FAULT_MODE));
    flags = xeIoctlHelper->getFlagsForVmCreate(false, true, false);
    EXPECT_EQ(flags & DRM_XE_VM_CREATE_FLAG_FAULT_MODE, static_cast<uint32_t>(DRM_XE_VM_CREATE_FLAG_FAULT_MODE));
}

TEST_F(IoctlHelperXeTest, whenGettingEuDEbugInterfaceTypeThenCorrectValueReturned) {

    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());
    auto &eudebugInterface = xeIoctlHelper->euDebugInterface;
    static_cast<MockEuDebugInterface *>(eudebugInterface.get())->setCurrentInterfaceType(EuDebugInterfaceType::prelim);
    EXPECT_EQ(xeIoctlHelper->getEuDebugInterfaceType(), EuDebugInterfaceType::prelim);
    static_cast<MockEuDebugInterface *>(eudebugInterface.get())->setCurrentInterfaceType(EuDebugInterfaceType::upstream);
    EXPECT_EQ(xeIoctlHelper->getEuDebugInterfaceType(), EuDebugInterfaceType::upstream);
}

TEST_F(IoctlHelperXeDebugDataTest, givenPrelimIoctlHelperWhenBindAddDebugDataCalledThenReturnZeroAndIoctlNotCalledNorFenceSet) {
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);

    uint64_t fenceAddress = 0x4321;
    uint64_t fenceValue = 0x789;

    uint32_t vmId = 1;
    VmBindExtUserFenceT userFence{};
    ioctlHelper->fillVmBindExtUserFence(userFence, fenceAddress, fenceValue, 0);
    std::vector<VmBindOpExtDebugData> debugDataVec;
    VmBindOpExtDebugData debugData{};
    debugData.base.name = 0;
    debugData.base.nextExtension = 0;
    debugData.base.pad = 0;
    debugData.addr = 0x1234;
    debugData.range = 0x1000;
    debugData.flags = 0;
    debugData.pseudopath = 0;
    debugDataVec.push_back(debugData);
    auto retVal = ioctlHelper->bindAddDebugData(debugDataVec, vmId, &userFence, true);
    EXPECT_EQ(retVal, 0);

    EXPECT_FALSE(drm->gemVmBindCalled);
    EXPECT_EQ(drm->waitUserFenceInputs.size(), 0u);
}

TEST_F(IoctlHelperXeDebugDataTest, givenPrelimIoctlHelperWhenAddDebugDataAndCreateBindOpVecThenNulloptReturned) {
    auto ioctlHelper = std::make_unique<MockIoctlHelper>(*drm);

    uint32_t vmId = 1;
    auto isAdd = false;
    MockBufferObject bo(rootDeviceIndex, drm.get(), 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.registerBOBindExtHandle(drm.get());

    auto result = ioctlHelper->addDebugDataAndCreateBindOpVec(&bo, vmId, isAdd);
    EXPECT_EQ(std::nullopt, result);
}

TEST_F(IoctlHelperXeDebugDataTest, givenUpstreamDebuggerWhenRegisterBOBindHandleCalledThenResourceClassIsSet) {
    xeIoctlHelper->euDebugInterface = std::make_unique<EuDebugInterfaceUpstream>();

    {
        MockBufferObject bo(rootDeviceIndex, drm.get(), 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugContextSaveArea, MemoryPool::localMemory);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(drm.get());

        EXPECT_EQ(bo.getDrmResourceClass(), DrmResourceClass::contextSaveArea);
    }

    {
        MockBufferObject bo(rootDeviceIndex, drm.get(), 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugSbaTrackingBuffer, MemoryPool::localMemory);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(drm.get());

        EXPECT_EQ(bo.getDrmResourceClass(), DrmResourceClass::sbaTrackingBuffer);
    }

    {
        MockBufferObject bo(rootDeviceIndex, drm.get(), 3, 0, 0, 1);
        MockDrmAllocation allocation(rootDeviceIndex, AllocationType::debugModuleArea, MemoryPool::localMemory);
        allocation.bufferObjects[0] = &bo;
        allocation.registerBOBindExtHandle(drm.get());

        EXPECT_EQ(bo.getDrmResourceClass(), DrmResourceClass::moduleHeapDebugArea);
    }
}
