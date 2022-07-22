/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/os_interface/os_interface.h"
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

    void SetUp() { // NOLINT(readability-identifier-naming)
        setUp(nullptr);
    }

    void setUp(HardwareInfo *hwInfo) {
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        auto mockBuiltIns = new NEO::MockBuiltins();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->setDebuggingEnabled();
        executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(hwInfo ? hwInfo : defaultHwInfo.get());
        executionEnvironment->initializeMemoryManager();
        auto osInterface = new OSInterface();
        drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));

        neoDevice = NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u);

        NEO::DeviceVector devices;
        devices.push_back(std::unique_ptr<NEO::Device>(neoDevice));
        driverHandle = std::make_unique<Mock<L0::DriverHandleImp>>();
        driverHandle->enableProgramDebugging = true;

        driverHandle->initialize(std::move(devices));
        device = driverHandle->devices[0];
    }

    void TearDown() { // NOLINT(readability-identifier-naming)
        drmMock = nullptr;
        device = nullptr;
        neoDevice = nullptr;
    };

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    DrmMockResources *drmMock = nullptr;
};

struct L0DebuggerLinuxMultitileFixture : public L0DebuggerLinuxFixture {

    void SetUp() { // NOLINT(readability-identifier-naming)

        DebugManager.flags.CreateMultipleRootDevices.set(1);
        constexpr auto numSubDevices = 2u;
        DebugManager.flags.CreateMultipleSubDevices.set(2);
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

    void TearDown() { // NOLINT(readability-identifier-naming)
        L0DebuggerLinuxFixture::TearDown();
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
    EXPECT_FALSE(neoDevice->getDebugger()->isLegacy());

    EXPECT_EQ(nullptr, neoDevice->getSourceLevelDebugger());
}

TEST_F(L0DebuggerLinuxTest, whenDebuggerIsCreatedThenItCallsDrmToRegisterResourceClasses) {
    EXPECT_NE(nullptr, neoDevice->getDebugger());

    EXPECT_TRUE(drmMock->registerClassesCalled);
}

TEST_F(L0DebuggerLinuxTest, givenLinuxOSWhenL0DebuggerIsCreatedAddressModeIsNotSingleSpace) {
    EXPECT_FALSE(device->getL0Debugger()->getSingleAddressSpaceSbaTracking());
}

TEST(L0DebuggerLinux, givenVmBindAndPerContextVmEnabledInDrmWhenInitializingDebuggingInOsThenRegisterResourceClassesIsCalled) {
    auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();

    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->setDebuggingEnabled();

    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
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

TEST(L0DebuggerLinux, givenVmBindNotAvailableInDrmWhenInitializingDebuggingInOsThenRegisterResourceClassesIsNotCalled) {
    auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();

    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->setDebuggingEnabled();

    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
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

TEST(L0DebuggerLinux, givenPerContextVmNotEnabledWhenInitializingDebuggingInOsThenRegisterResourceClassesIsNotCalled) {
    auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();

    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->setDebuggingEnabled();

    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->initializeMemoryManager();
    auto osInterface = new OSInterface();
    auto drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock->callBaseIsVmBindAvailable = true;
    drmMock->bindAvailable = true;
    drmMock->setPerContextVMRequired(false);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drmMock));

    auto result = NEO::WhiteBox<NEO::DebuggerL0>::initDebuggingInOs(osInterface);
    EXPECT_FALSE(result);
    EXPECT_FALSE(drmMock->registerClassesCalled);
}

TEST_F(L0DebuggerLinuxTest, whenRegisterElfisCalledThenItRegistersBindExtHandles) {
    NEO::DebugData debugData;
    debugData.vIsa = "01234567890";
    debugData.vIsaSize = 10;
    MockDrmAllocation isaAllocation(AllocationType::KERNEL_ISA, MemoryPool::System4KBPages);
    MockBufferObject bo(drmMock, 3, 0, 0, 1);
    isaAllocation.bufferObjects[0] = &bo;
    device->getL0Debugger()->registerElf(&debugData, &isaAllocation);

    EXPECT_EQ(static_cast<size_t>(10), drmMock->registeredDataSize);

    auto &bos = isaAllocation.getBOs();
    for (auto bo : bos) {
        if (bo) {
            auto extBindHandles = bo->getBindExtHandles();
            EXPECT_NE(static_cast<size_t>(0), extBindHandles.size());
        }
    }
}

TEST_F(L0DebuggerLinuxTest, whenRegisterElfisCalledInAllocationWithNoBOThenItRegistersBindExtHandles) {
    NEO::DebugData debugData;
    debugData.vIsa = "01234567890";
    debugData.vIsaSize = 10;
    MockDrmAllocation isaAllocation(AllocationType::KERNEL_ISA, MemoryPool::System4KBPages);
    device->getL0Debugger()->registerElf(&debugData, &isaAllocation);

    EXPECT_EQ(static_cast<size_t>(10u), drmMock->registeredDataSize);
}

TEST_F(L0DebuggerLinuxTest, givenNoOSInterfaceThenRegisterElfDoesNothing) {
    NEO::OSInterface *osInterfaceTmp = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.release();
    NEO::DebugData debugData;
    debugData.vIsa = "01234567890";
    debugData.vIsaSize = 10;
    drmMock->registeredDataSize = 0;
    MockDrmAllocation isaAllocation(AllocationType::KERNEL_ISA, MemoryPool::System4KBPages);

    device->getL0Debugger()->registerElf(&debugData, &isaAllocation);

    EXPECT_EQ(static_cast<size_t>(0u), drmMock->registeredDataSize);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(osInterfaceTmp);
}

TEST_F(L0DebuggerLinuxTest, givenAllocationsWhenAttachingZebinModuleThenAllAllocationsHaveRegisteredHandle) {
    MockDrmAllocation isaAllocation(AllocationType::KERNEL_ISA, MemoryPool::System4KBPages);
    MockBufferObject bo(drmMock, 3, 0, 0, 1);
    isaAllocation.bufferObjects[0] = &bo;

    MockDrmAllocation isaAllocation2(AllocationType::KERNEL_ISA, MemoryPool::System4KBPages);
    MockBufferObject bo2(drmMock, 3, 0, 0, 1);
    isaAllocation2.bufferObjects[0] = &bo2;

    uint32_t handle = 0;

    StackVec<NEO::GraphicsAllocation *, 32> kernelAllocs;
    kernelAllocs.push_back(&isaAllocation);
    kernelAllocs.push_back(&isaAllocation2);

    drmMock->registeredDataSize = 0;
    drmMock->registeredClass = NEO::DrmResourceClass::MaxSize;

    EXPECT_TRUE(device->getL0Debugger()->attachZebinModuleToSegmentAllocations(kernelAllocs, handle));

    EXPECT_EQ(sizeof(uint32_t), drmMock->registeredDataSize);
    EXPECT_EQ(NEO::DrmResourceClass::L0ZebinModule, drmMock->registeredClass);

    const auto containsModuleHandle = [handle](const auto &bufferObject) {
        const auto &bindExtHandles = bufferObject.getBindExtHandles();
        return std::find(bindExtHandles.begin(), bindExtHandles.end(), handle) != bindExtHandles.end();
    };

    EXPECT_TRUE(containsModuleHandle(bo));
    EXPECT_TRUE(containsModuleHandle(bo2));
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

    neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().ensureContextInitialized();
    drmMock->ioctlCallsCount = 0;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto commandQueue1 = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue);
    EXPECT_EQ(1u, drmMock->ioctlCallsCount);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueCreatedCount);

    auto commandQueue2 = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue);
    EXPECT_EQ(1u, drmMock->ioctlCallsCount);
    EXPECT_EQ(2u, debuggerL0Hw->commandQueueCreatedCount);

    commandQueue1->destroy();
    EXPECT_EQ(1u, drmMock->ioctlCallsCount);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueDestroyedCount);

    commandQueue2->destroy();
    EXPECT_EQ(1u, drmMock->unregisterCalledCount);
    EXPECT_EQ(2u, debuggerL0Hw->commandQueueDestroyedCount);
}

HWTEST_F(L0DebuggerLinuxTest, givenDebuggingEnabledWhenImmCommandListsCreatedAndDestroyedThenDebuggerL0IsNotified) {
    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());

    neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().ensureContextInitialized();
    drmMock->ioctlCallsCount = 0;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;

    ze_command_list_handle_t commandList1 = nullptr;
    ze_command_list_handle_t commandList2 = nullptr;

    returnValue = device->createCommandListImmediate(&queueDesc, &commandList1);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, drmMock->ioctlCallsCount);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueCreatedCount);

    returnValue = device->createCommandListImmediate(&queueDesc, &commandList2);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(1u, drmMock->ioctlCallsCount);
    EXPECT_EQ(2u, debuggerL0Hw->commandQueueCreatedCount);

    CommandList::fromHandle(commandList1)->destroy();
    EXPECT_EQ(1u, drmMock->ioctlCallsCount);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueDestroyedCount);

    CommandList::fromHandle(commandList2)->destroy();
    EXPECT_EQ(1u, drmMock->unregisterCalledCount);
    EXPECT_EQ(2u, debuggerL0Hw->commandQueueDestroyedCount);
}

HWTEST_F(L0DebuggerLinuxMultitileTest, givenDebuggingEnabledWhenCommandQueuesCreatedThenDebuggerIsNotified) {

    auto debuggerL0Hw = static_cast<MockDebuggerL0Hw<FamilyType> *>(device->getL0Debugger());
    drmMock->ioctlCallsCount = 0;
    neoDevice->getDefaultEngine().commandStreamReceiver->getOsContext().ensureContextInitialized();

    EXPECT_EQ(2u, debuggerL0Hw->uuidL0CommandQueueHandle.size());

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;

    auto commandQueue1 = CommandQueue::create(productFamily, subDevice0, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue);
    EXPECT_EQ(1u, drmMock->ioctlCallsCount);
    EXPECT_EQ(1u, debuggerL0Hw->commandQueueCreatedCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    auto commandQueue2 = CommandQueue::create(productFamily, subDevice1, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, false, returnValue);
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
} // namespace ult
} // namespace L0
