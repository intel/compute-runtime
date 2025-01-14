/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/file_io.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/os_inc.h"
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
    VariableBackup<const char *> eudebugSysFsEntryBackup(&eudebugSysfsEntry[static_cast<uint32_t>(MockEuDebugInterface::euDebugInterfaceType)], "invalidEntry");

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
    xeIoctlHelper->unregisterResource(0x1234);
    EXPECT_EQ(drm->metadataID, 0x1234u);
}

TEST_F(IoctlHelperXeTest, whenGettingVmBindExtFromHandlesThenProperStructsAreReturned) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto drm = DrmMockXeDebug::create(*executionEnvironment->rootDeviceEnvironments[0]);
    auto xeIoctlHelper = static_cast<MockIoctlHelperXeDebug *>(drm->ioctlHelper.get());

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

TEST_F(IoctlHelperXeTest, givenRegisterIsaHandleWhenIsaIsTileInstancedThenBOCookieSet) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);

    drm.ioctlHelper.reset(xeIoctlHelper.release());

    MockBufferObject bo(rootDeviceIndex, &drm, 3, 0, 0, 1);
    MockDrmAllocation allocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::localMemory);
    allocation.bufferObjects[0] = &bo;
    allocation.storageInfo.tileInstanced = true;
    allocation.storageInfo.subDeviceBitfield.set();
    allocation.registerBOBindExtHandle(&drm);
    EXPECT_EQ(bo.getRegisteredBindHandleCookie(), allocation.storageInfo.subDeviceBitfield.to_ulong());
}

TEST_F(IoctlHelperXeTest, givenRegisterIsaHandleWhenIsaIsNotTileInstancedThenBOCookieNotSet) {
    const uint32_t rootDeviceIndex = 0u;
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    DrmMockResources drm(*executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]);
    auto xeIoctlHelper = std::make_unique<MockIoctlHelperXeDebug>(drm);

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