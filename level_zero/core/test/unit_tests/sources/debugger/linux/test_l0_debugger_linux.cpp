/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdlist/cmdlist.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

#include <algorithm>
#include <memory>

using namespace NEO;

namespace L0 {
namespace ult {

struct L0DebuggerLinuxFixture {

    void setUp() {
        setUp(nullptr);
    }

    void setUp(HardwareInfo *hwInfo) {
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        auto mockBuiltIns = new NEO::MockBuiltins();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
        MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(hwInfo ? hwInfo : defaultHwInfo.get());
        UnitTestSetter::setCcsExposure(*executionEnvironment->rootDeviceEnvironments[0]);
        UnitTestSetter::setRcsExposure(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->calculateMaxOsContextCount();
        executionEnvironment->initializeMemoryManager();
        auto osInterface = new OSInterface();
        drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
        drmMock->allowDebugAttach = true;
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, rootDeviceIndex);

        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->enableProgramDebugging = NEO::DebuggingMode::online;

        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    void tearDown() {
        drmMock = nullptr;
        device = nullptr;
        neoDevice = nullptr;
    };
    const uint32_t rootDeviceIndex = 0u;
    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    DrmMockResources *drmMock = nullptr;
};

struct L0DebuggerLinuxMultitileFixture : public L0DebuggerLinuxFixture {

    void setUp() {

        debugManager.flags.CreateMultipleRootDevices.set(1);
        constexpr auto numSubDevices = 2u;
        debugManager.flags.CreateMultipleSubDevices.set(2);
        hwInfo = *defaultHwInfo;

        hwInfo.gtSystemInfo.MultiTileArchInfo.IsValid = 1;
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileCount = numSubDevices;
        hwInfo.gtSystemInfo.MultiTileArchInfo.Tile0 = 1;
        hwInfo.gtSystemInfo.MultiTileArchInfo.Tile1 = 1;
        hwInfo.gtSystemInfo.MultiTileArchInfo.TileMask = 3;

        L0DebuggerLinuxFixture::setUp(&hwInfo);

        subDevice0 = neoDevice->getSubDevice(0)->getSpecializedDevice<Device>();
        subDevice1 = neoDevice->getSubDevice(1)->getSpecializedDevice<Device>();
    }

    void tearDown() {
        L0DebuggerLinuxFixture::tearDown();
    }

    DebugManagerStateRestore restorer;
    HardwareInfo hwInfo;
    L0::Device *subDevice0 = nullptr;
    L0::Device *subDevice1 = nullptr;
};

using L0DebuggerLinuxTest = Test<L0DebuggerLinuxFixture>;
using L0DebuggerLinuxMultitileTest = Test<L0DebuggerLinuxMultitileFixture>;

TEST_F(L0DebuggerLinuxTest, givenProgramDebuggingEnabledWhenDriverHandleIsCreatedThenItAllocatesL0Debugger) {
    EXPECT_NE(nullptr, neoDevice->getDebugger());
}

TEST_F(L0DebuggerLinuxTest, whenDebuggerIsCreatedThenItCallsDrmToRegisterResourceClasses) {
    EXPECT_NE(nullptr, neoDevice->getDebugger());

    EXPECT_TRUE(drmMock->registerClassesCalled);
}

TEST_F(L0DebuggerLinuxTest, givenLinuxOSWhenL0DebuggerIsCreatedAddressModeIsNotSingleSpace) {
    EXPECT_FALSE(device->getL0Debugger()->getSingleAddressSpaceSbaTracking());
}

TEST(L0DebuggerLinux, givenVmBindAndPerContextVmEnabledInDrmWhenInitializingDebuggingInOsThenRegisterResourceClassesIsCalled) {
    auto executionEnvironment = std::make_unique<NEO::MockExecutionEnvironment>();

    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->initializeMemoryManager();
    auto osInterface = new OSInterface();
    auto drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock->callBaseIsVmBindAvailable = true;
    drmMock->bindAvailable = true;
    drmMock->setPerContextVMRequired(true);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drmMock));

    auto result = NEO::WhiteBox<NEO::DebuggerL0>::initDebuggingInOs(osInterface);
    EXPECT_TRUE(result);
    EXPECT_TRUE(drmMock->registerClassesCalled);
}

TEST(L0DebuggerLinux, givenVmBindAndOfflineDebuggingModeWhenInitializingDebuggingInOsThenRegisterResourceClassesIsCalled) {
    auto executionEnvironment = std::make_unique<NEO::MockExecutionEnvironment>();

    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::offline);
    executionEnvironment->initializeMemoryManager();
    auto osInterface = new OSInterface();
    auto drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock->callBaseIsVmBindAvailable = true;
    drmMock->bindAvailable = true;
    EXPECT_FALSE(drmMock->isPerContextVMRequired());

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drmMock));

    auto result = NEO::WhiteBox<NEO::DebuggerL0>::initDebuggingInOs(osInterface);
    EXPECT_TRUE(result);
    EXPECT_TRUE(drmMock->registerClassesCalled);
}

TEST(L0DebuggerLinux, givenVmBindNotAvailableInDrmWhenInitializingDebuggingInOsThenRegisterResourceClassesIsNotCalled) {
    auto executionEnvironment = std::make_unique<NEO::MockExecutionEnvironment>();

    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->initializeMemoryManager();
    auto osInterface = new OSInterface();
    auto drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock->callBaseIsVmBindAvailable = true;
    drmMock->bindAvailable = false;
    drmMock->setPerContextVMRequired(true);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drmMock));

    auto result = NEO::WhiteBox<NEO::DebuggerL0>::initDebuggingInOs(osInterface);
    EXPECT_FALSE(result);
    EXPECT_FALSE(drmMock->registerClassesCalled);
}

TEST(L0DebuggerLinux, givenPrintDebugSettingsAndIncorrectSetupWhenInitializingDebuggingInOsThenMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.PrintDebugMessages.set(1);

    auto executionEnvironment = std::make_unique<NEO::MockExecutionEnvironment>();

    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->initializeMemoryManager();
    auto osInterface = new OSInterface();
    auto drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock->callBaseIsVmBindAvailable = true;
    drmMock->bindAvailable = false;
    drmMock->setPerContextVMRequired(true);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drmMock));

    ::testing::internal::CaptureStderr();

    auto result = NEO::WhiteBox<NEO::DebuggerL0>::initDebuggingInOs(osInterface);
    std::string output = testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::string("Debugging not enabled. VmBind: 0, per-context VMs: 1\n"), output);
    EXPECT_FALSE(result);

    drmMock->bindAvailable = true;
    drmMock->setPerContextVMRequired(false);
    ::testing::internal::CaptureStderr();

    result = NEO::WhiteBox<NEO::DebuggerL0>::initDebuggingInOs(osInterface);
    output = testing::internal::GetCapturedStderr();
    auto drm = osInterface->getDriverModel()->as<NEO::Drm>();
    if (drm->getRootDeviceEnvironment().getHelper<CompilerProductHelper>().isHeaplessModeEnabled(*defaultHwInfo)) {
        EXPECT_NE(std::string("Debugging not enabled. VmBind: 1, per-context VMs: 0\n"), output);
        EXPECT_TRUE(result);

    } else {
        EXPECT_EQ(std::string("Debugging not enabled. VmBind: 1, per-context VMs: 0\n"), output);
        EXPECT_FALSE(result);
    }
}

TEST(L0DebuggerLinux, givenPerContextVmNotEnabledWhenInitializingDebuggingInOsThenRegisterResourceClassesIsNotCalled) {
    auto executionEnvironment = std::make_unique<NEO::MockExecutionEnvironment>();

    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->initializeMemoryManager();
    auto osInterface = new OSInterface();
    auto drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock->callBaseIsVmBindAvailable = true;
    drmMock->bindAvailable = true;
    drmMock->setPerContextVMRequired(false);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drmMock));

    auto result = NEO::WhiteBox<NEO::DebuggerL0>::initDebuggingInOs(osInterface);
    auto drm = osInterface->getDriverModel()->as<NEO::Drm>();

    if (drm->getRootDeviceEnvironment().getHelper<CompilerProductHelper>().isHeaplessModeEnabled(*defaultHwInfo)) {
        EXPECT_TRUE(result);
        EXPECT_TRUE(drmMock->registerClassesCalled);
    } else {
        EXPECT_FALSE(result);
        EXPECT_FALSE(drmMock->registerClassesCalled);
    }
}

TEST_F(L0DebuggerLinuxTest, whenRegisterElfAndLinkWithAllocationIsCalledThenItRegistersBindExtHandles) {
    NEO::DebugData debugData;
    debugData.vIsa = "01234567890";
    debugData.vIsaSize = 10;
    MockDrmAllocation isaAllocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::system4KBPages);
    MockBufferObject bo(rootDeviceIndex, drmMock, 3, 0, 0, 1);
    isaAllocation.bufferObjects[0] = &bo;
    device->getL0Debugger()->registerElfAndLinkWithAllocation(&debugData, &isaAllocation);

    EXPECT_EQ(static_cast<size_t>(10), drmMock->registeredDataSize);

    auto &bos = isaAllocation.getBOs();
    for (auto bo : bos) {
        if (bo) {
            auto extBindHandles = bo->getBindExtHandles();
            EXPECT_NE(static_cast<size_t>(0), extBindHandles.size());
        }
    }
}

TEST_F(L0DebuggerLinuxTest, whenRegisterElfAndLinkWithAllocationIsCalledInAllocationWithNoBOThenItRegistersBindExtHandles) {
    NEO::DebugData debugData;
    debugData.vIsa = "01234567890";
    debugData.vIsaSize = 10;
    MockDrmAllocation isaAllocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::system4KBPages);
    device->getL0Debugger()->registerElfAndLinkWithAllocation(&debugData, &isaAllocation);

    EXPECT_EQ(static_cast<size_t>(10u), drmMock->registeredDataSize);
}

HWTEST_F(L0DebuggerLinuxTest, givenFailureToRegisterElfWhenRegisterElfAndLinkWithAllocationIsCalledThenBindExtHandleIsNotAdded) {
    NEO::DebugData debugData;
    debugData.vIsa = "01234567890";
    debugData.vIsaSize = 10;
    MockDrmAllocation isaAllocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::system4KBPages);
    MockBufferObject bo(rootDeviceIndex, drmMock, 3, 0, 0, 1);
    isaAllocation.bufferObjects[0] = &bo;

    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());
    debuggerL0Hw->elfHandleToReturn = 0;
    device->getL0Debugger()->registerElfAndLinkWithAllocation(&debugData, &isaAllocation);

    auto &bos = isaAllocation.getBOs();
    for (auto bo : bos) {
        if (bo) {
            auto extBindHandles = bo->getBindExtHandles();
            EXPECT_EQ(static_cast<size_t>(0), extBindHandles.size());
        }
    }
}

TEST_F(L0DebuggerLinuxTest, givenAllocationsWhenAttachingZebinModuleThenAllAllocationsHaveRegisteredHandles) {
    MockDrmAllocation isaAllocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::system4KBPages);
    MockBufferObject bo(rootDeviceIndex, drmMock, 3, 0, 0, 1);
    isaAllocation.bufferObjects[0] = &bo;

    MockDrmAllocation isaAllocation2(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::system4KBPages);
    MockBufferObject bo2(rootDeviceIndex, drmMock, 3, 0, 0, 1);
    isaAllocation2.bufferObjects[0] = &bo2;

    uint32_t handle = 0;
    const uint32_t elfHandle = 198;

    StackVec<NEO::GraphicsAllocation *, 32> kernelAllocs;
    kernelAllocs.push_back(&isaAllocation);
    kernelAllocs.push_back(&isaAllocation2);

    drmMock->registeredDataSize = 0;
    drmMock->registeredClass = NEO::DrmResourceClass::maxSize;

    EXPECT_TRUE(device->getL0Debugger()->attachZebinModuleToSegmentAllocations(kernelAllocs, handle, elfHandle));

    EXPECT_EQ(sizeof(uint32_t), drmMock->registeredDataSize);
    EXPECT_EQ(NEO::DrmResourceClass::l0ZebinModule, drmMock->registeredClass);

    const auto containsModuleHandle = [handle](const auto &bufferObject) {
        const auto &bindExtHandles = bufferObject.getBindExtHandles();
        return std::find(bindExtHandles.begin(), bindExtHandles.end(), handle) != bindExtHandles.end();
    };

    const auto containsElfHandle = [elfHandle](const auto &bufferObject) {
        const auto &bindExtHandles = bufferObject.getBindExtHandles();
        return std::find(bindExtHandles.begin(), bindExtHandles.end(), elfHandle) != bindExtHandles.end();
    };

    EXPECT_TRUE(containsModuleHandle(bo));
    EXPECT_TRUE(containsModuleHandle(bo2));
    EXPECT_TRUE(containsElfHandle(bo));
    EXPECT_TRUE(containsElfHandle(bo2));
}

TEST_F(L0DebuggerLinuxTest, givenModuleAllocationsWhenAttachingZebinModuleThenBosRequireImmediateBind) {
    MockDrmAllocation isaAllocation(rootDeviceIndex, AllocationType::kernelIsa, MemoryPool::system4KBPages);
    MockBufferObject bo(rootDeviceIndex, drmMock, 3, 0, 0, 1);
    isaAllocation.bufferObjects[0] = &bo;

    MockDrmAllocation isaAllocation2(rootDeviceIndex, AllocationType::constantSurface, MemoryPool::system4KBPages);
    MockBufferObject bo2(rootDeviceIndex, drmMock, 3, 0, 0, 1);
    isaAllocation2.bufferObjects[0] = &bo2;

    uint32_t handle = 0;
    const uint32_t elfHandle = 198;

    StackVec<NEO::GraphicsAllocation *, 32> kernelAllocs;
    kernelAllocs.push_back(&isaAllocation);
    kernelAllocs.push_back(&isaAllocation2);

    drmMock->registeredDataSize = 0;
    drmMock->registeredClass = NEO::DrmResourceClass::maxSize;

    EXPECT_TRUE(device->getL0Debugger()->attachZebinModuleToSegmentAllocations(kernelAllocs, handle, elfHandle));

    const auto containsModuleHandle = [handle](const auto &bufferObject) {
        const auto &bindExtHandles = bufferObject.getBindExtHandles();
        return std::find(bindExtHandles.begin(), bindExtHandles.end(), handle) != bindExtHandles.end();
    };

    EXPECT_TRUE(containsModuleHandle(bo));
    EXPECT_TRUE(containsModuleHandle(bo2));
    EXPECT_TRUE(bo.isImmediateBindingRequired());
    EXPECT_TRUE(bo2.isImmediateBindingRequired());
}

TEST_F(L0DebuggerLinuxTest, givenModuleHandleWhenRemoveZebinModuleIsCalledThenHandleIsUnregistered) {
    uint32_t handle = 20;

    EXPECT_TRUE(device->getL0Debugger()->removeZebinModule(handle));

    EXPECT_EQ(1u, drmMock->unregisterCalledCount);
    EXPECT_EQ(20u, drmMock->unregisteredHandle);
}

TEST_F(L0DebuggerLinuxTest, givenModuleHandleZeroWhenRemoveZebinModuleIsCalledThenDrmUnregisterIsNotCalled) {
    uint32_t handle = 0;

    EXPECT_FALSE(device->getL0Debugger()->removeZebinModule(handle));
    EXPECT_EQ(0u, drmMock->unregisterCalledCount);
}

HWTEST_F(L0DebuggerLinuxTest, givenDebuggingEnabledWhenCommandQueuesAreCreatedAndDestroyedThenDebuggerL0IsNotified) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());

    neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().ensureContextInitialized(false);
    drmMock->ioctlCallsCount = 0;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue1 = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue);
    EXPECT_EQ(1u, drmMock->ioctlCallsCount);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueCreatedCount);

    auto commandQueue2 = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue);
    EXPECT_EQ(1u, drmMock->ioctlCallsCount);
    EXPECT_EQ(2u, debuggerL0Hw->commandQueueCreatedCount);

    commandQueue1->destroy();
    EXPECT_EQ(1u, drmMock->ioctlCallsCount);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueDestroyedCount);

    commandQueue2->destroy();
    EXPECT_EQ(1u, drmMock->unregisterCalledCount);
    EXPECT_EQ(2u, debuggerL0Hw->commandQueueDestroyedCount);
}

HWTEST_F(L0DebuggerLinuxTest, givenDebuggingEnabledAndDebugAttachAvailableWhenInitializingDriverThenSuccessIsReturned) {
    auto executionEnvironment = new NEO::ExecutionEnvironment();
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->initializeMemoryManager();
    auto osInterface = new OSInterface();
    auto drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));

    auto neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->enableProgramDebugging = NEO::DebuggingMode::online;

    drmMock->allowDebugAttach = true;

    ze_result_t result = driverHandle->initialize(std::move(devices));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

HWTEST_F(L0DebuggerLinuxTest, givenDebuggingEnabledWhenImmCommandListsCreatedAndDestroyedThenDebuggerL0IsNotified) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());

    neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().ensureContextInitialized(false);
    drmMock->ioctlCallsCount = 0;

    ze_command_queue_desc_t queueDesc = {ZE_STRUCTURE_TYPE_COMMAND_QUEUE_DESC, nullptr, 0, 0, 0, ZE_COMMAND_QUEUE_MODE_DEFAULT, ZE_COMMAND_QUEUE_PRIORITY_NORMAL};
    ze_result_t returnValue;

    ze_command_list_handle_t commandList1 = nullptr;
    ze_command_list_handle_t commandList2 = nullptr;

    returnValue = device->createCommandListImmediate(&queueDesc, &commandList1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, drmMock->ioctlCallsCount);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueCreatedCount);

    returnValue = device->createCommandListImmediate(&queueDesc, &commandList2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, drmMock->notifyFirstCommandQueueCreatedCallsCount);
    EXPECT_EQ(2u, debuggerL0Hw->commandQueueCreatedCount);

    CommandList::fromHandle(commandList1)->destroy();
    EXPECT_EQ(1u, drmMock->notifyFirstCommandQueueCreatedCallsCount);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueDestroyedCount);

    CommandList::fromHandle(commandList2)->destroy();
    EXPECT_EQ(1u, drmMock->unregisterCalledCount);
    EXPECT_EQ(2u, debuggerL0Hw->commandQueueDestroyedCount);
}

HWTEST_F(L0DebuggerLinuxMultitileTest, givenDebuggingEnabledWhenCommandQueuesCreatedThenDebuggerIsNotified) {

    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());
    drmMock->ioctlCallsCount = 0;
    neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().ensureContextInitialized(false);

    EXPECT_EQ(2u, debuggerL0Hw->uuidL0CommandQueueHandle.size());

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;

    auto commandQueue1 = CommandQueue::create(productFamily, subDevice0, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue);
    EXPECT_EQ(1u, drmMock->ioctlCallsCount);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueCreatedCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto commandQueue2 = CommandQueue::create(productFamily, subDevice1, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue);
    EXPECT_EQ(2u, debuggerL0Hw->commandQueueCreatedCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    for (uint32_t index = 0; index < neoDevice->getNumSubDevices(); index++) {
        EXPECT_NE(0u, debuggerL0Hw->uuidL0CommandQueueHandle[index]);
    }

    commandQueue1->destroy();
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueDestroyedCount);

    commandQueue2->destroy();
    EXPECT_EQ(2u, drmMock->unregisterCalledCount);
    EXPECT_EQ(2u, debuggerL0Hw->commandQueueDestroyedCount);

    for (uint32_t index = 0; index < neoDevice->getNumSubDevices(); index++) {
        EXPECT_EQ(0u, debuggerL0Hw->uuidL0CommandQueueHandle[index]);
    }
}

HWTEST_F(L0DebuggerLinuxMultitileTest, givenDebuggingEnabledWhenCommandQueueCreatedOnRootDeviceThenDebuggerIsNotifiedForAllSubdevices) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());
    drmMock->ioctlCallsCount = 0;
    neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().ensureContextInitialized(false);

    EXPECT_EQ(2u, debuggerL0Hw->uuidL0CommandQueueHandle.size());

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;

    auto commandQueue1 = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue);
    EXPECT_EQ(2u, drmMock->ioctlCallsCount);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueCreatedCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    for (uint32_t index = 0; index < neoDevice->getNumSubDevices(); index++) {
        EXPECT_NE(0u, debuggerL0Hw->uuidL0CommandQueueHandle[index]);
    }

    commandQueue1->destroy();
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueDestroyedCount);

    for (uint32_t index = 0; index < neoDevice->getNumSubDevices(); index++) {
        EXPECT_EQ(0u, debuggerL0Hw->uuidL0CommandQueueHandle[index]);
    }
}

HWTEST_F(L0DebuggerLinuxMultitileTest, givenSubDeviceFilteredByAffinityMaskWhenCommandQueueCreatedThenDebuggerIsNotifiedWithCorrectDeviceIndex) {

    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0.1");

    auto executionEnvironment = new NEO::ExecutionEnvironment();
    auto mockBuiltIns = new NEO::MockBuiltins();

    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->parseAffinityMask();

    executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), mockBuiltIns);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(&hwInfo);
    executionEnvironment->initializeMemoryManager();
    auto osInterface = new OSInterface();
    auto drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock->allowDebugAttach = true;
    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));

    auto neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);

    NEO::DeviceVector devices;
    devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
    auto driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
    driverHandle->enableProgramDebugging = NEO::DebuggingMode::online;

    driverHandle->initialize(std::move(devices));
    auto device = driverHandle->devices[0];

    EXPECT_FALSE(neoDevice->isSubDevice());

    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());
    drmMock->ioctlCallsCount = 0;
    neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().ensureContextInitialized(false);

    EXPECT_EQ(1u, debuggerL0Hw->uuidL0CommandQueueHandle.size());

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;

    auto commandQueue1 = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, false, returnValue);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueCreatedCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_NE(0u, debuggerL0Hw->uuidL0CommandQueueHandle[0]);

    auto notification = reinterpret_cast<const NEO::DebuggerL0::CommandQueueNotification *>(drmMock->capturedCmdQData.get());
    EXPECT_EQ(sizeof(NEO::DebuggerL0::CommandQueueNotification), drmMock->capturedCmdQSize);
    EXPECT_EQ(1u, notification->subDeviceIndex);
    EXPECT_EQ(0u, notification->subDeviceCount);

    commandQueue1->destroy();
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueDestroyedCount);
    EXPECT_EQ(1u, drmMock->unregisterCalledCount);
}
} // namespace ult
} // namespace L0
