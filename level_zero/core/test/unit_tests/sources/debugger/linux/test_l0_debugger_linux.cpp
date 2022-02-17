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
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

#include <algorithm>
#include <memory>

using namespace NEO;

namespace L0 {
namespace ult {

struct L0DebuggerLinuxFixture {
    void SetUp() {
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        auto mockBuiltIns = new MockBuiltins();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->setDebuggingEnabled();
        executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltIns);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
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

    void TearDown() {
    }

    std::unique_ptr<Mock<L0::DriverHandleImp>> driverHandle;
    NEO::MockDevice *neoDevice = nullptr;
    L0::Device *device = nullptr;
    DrmMockResources *drmMock = nullptr;
};

using L0DebuggerLinuxTest = Test<L0DebuggerLinuxFixture>;

TEST_F(L0DebuggerLinuxTest, givenProgramDebuggingEnabledWhenDriverHandleIsCreatedThenItAllocatesL0Debugger) {
    EXPECT_NE(nullptr, neoDevice->getDebugger());
    EXPECT_FALSE(neoDevice->getDebugger()->isLegacy());

    EXPECT_EQ(nullptr, neoDevice->getSourceLevelDebugger());
}

TEST_F(L0DebuggerLinuxTest, whenDebuggerIsCreatedThenItCallsDrmToRegisterResourceClasses) {
    EXPECT_NE(nullptr, neoDevice->getDebugger());

    EXPECT_TRUE(drmMock->registerClassesCalled);
}

TEST(L0DebuggerLinux, givenVmBindAndPerContextVmEnabledInDrmWhenInitializingDebuggingInOsThenRegisterResourceClassesIsCalled) {
    auto executionEnvironment = std::make_unique<NEO::ExecutionEnvironment>();

    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->setDebuggingEnabled();

    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->initializeMemoryManager();
    auto osInterface = new OSInterface();
    auto drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
    drmMock->bindAvailable = true;
    drmMock->setPerContextVMRequired(true);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drmMock));

    auto result = WhiteBox<::L0::DebuggerL0>::initDebuggingInOs(osInterface);
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
    drmMock->bindAvailable = false;
    drmMock->setPerContextVMRequired(true);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drmMock));

    auto result = WhiteBox<::L0::DebuggerL0>::initDebuggingInOs(osInterface);
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
    drmMock->bindAvailable = true;
    drmMock->setPerContextVMRequired(false);

    executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<DriverModel>(drmMock));

    auto result = WhiteBox<::L0::DebuggerL0>::initDebuggingInOs(osInterface);
    EXPECT_FALSE(result);
    EXPECT_FALSE(drmMock->registerClassesCalled);
}

TEST_F(L0DebuggerLinuxTest, whenRegisterElfisCalledThenItRegistersBindExtHandles) {
    NEO::DebugData debugData;
    debugData.vIsa = "01234567890";
    debugData.vIsaSize = 10;
    MockDrmAllocation isaAllocation(AllocationType::KERNEL_ISA, MemoryPool::System4KBPages);
    MockBufferObject bo(drmMock, 0, 0, 1);
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
    NEO::OSInterface *OSInterface_tmp = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.release();
    NEO::DebugData debugData;
    debugData.vIsa = "01234567890";
    debugData.vIsaSize = 10;
    drmMock->registeredDataSize = 0;
    MockDrmAllocation isaAllocation(AllocationType::KERNEL_ISA, MemoryPool::System4KBPages);

    device->getL0Debugger()->registerElf(&debugData, &isaAllocation);

    EXPECT_EQ(static_cast<size_t>(0u), drmMock->registeredDataSize);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(OSInterface_tmp);
}

TEST_F(L0DebuggerLinuxTest, givenAllocationsWhenAttachingZebinModuleThenAllAllocationsHaveRegisteredHandle) {
    MockDrmAllocation isaAllocation(AllocationType::KERNEL_ISA, MemoryPool::System4KBPages);
    MockBufferObject bo(drmMock, 0, 0, 1);
    isaAllocation.bufferObjects[0] = &bo;

    MockDrmAllocation isaAllocation2(AllocationType::KERNEL_ISA, MemoryPool::System4KBPages);
    MockBufferObject bo2(drmMock, 0, 0, 1);
    isaAllocation2.bufferObjects[0] = &bo2;

    uint32_t handle = 0;

    StackVec<NEO::GraphicsAllocation *, 32> kernelAllocs;
    kernelAllocs.push_back(&isaAllocation);
    kernelAllocs.push_back(&isaAllocation2);

    drmMock->registeredDataSize = 0;
    drmMock->registeredClass = NEO::Drm::ResourceClass::MaxSize;

    EXPECT_TRUE(device->getL0Debugger()->attachZebinModuleToSegmentAllocations(kernelAllocs, handle));

    EXPECT_EQ(sizeof(uint32_t), drmMock->registeredDataSize);
    EXPECT_EQ(NEO::Drm::ResourceClass::L0ZebinModule, drmMock->registeredClass);

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

HWTEST_F(L0DebuggerLinuxTest, givenDebuggingEnabledAndCommandQueuesAreCreatedAndDestroyedThanDebuggerL0IsNotified) {
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

} // namespace ult
} // namespace L0
