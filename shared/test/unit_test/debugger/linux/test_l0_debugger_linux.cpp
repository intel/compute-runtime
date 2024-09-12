/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/kernel/debug_data.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_drm_allocation.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include <algorithm>
#include <memory>

using namespace NEO;

struct L0DebuggerSharedLinuxFixture {

    void setUp() {
        setUp(nullptr);
    }

    void setUp(HardwareInfo *hwInfo) {
        auto executionEnvironment = new NEO::ExecutionEnvironment();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->setDebuggingMode(NEO::DebuggingMode::online);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(hwInfo ? hwInfo : defaultHwInfo.get());
        executionEnvironment->initializeMemoryManager();
        auto osInterface = new OSInterface();
        drmMock = new DrmMockResources(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(osInterface);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->setDriverModel(std::unique_ptr<Drm>(drmMock));

        neoDevice.reset(NEO::MockDevice::create<NEO::MockDevice>(executionEnvironment, 0u));

        auto rootDeviceEnvironment = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->initDebuggerL0(neoDevice.get());
    }

    void tearDown() {
        drmMock = nullptr;
    };

    std::unique_ptr<NEO::MockDevice> neoDevice = nullptr;
    DrmMockResources *drmMock = nullptr;
};

using L0DebuggerSharedLinuxTest = Test<L0DebuggerSharedLinuxFixture>;

TEST_F(L0DebuggerSharedLinuxTest, givenNoOSInterfaceThenRegisterElfDoesNothing) {
    NEO::OSInterface *osInterfaceTmp = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.release();
    NEO::DebugData debugData;
    debugData.vIsa = "01234567890";
    debugData.vIsaSize = 10;
    drmMock->registeredDataSize = 0;

    neoDevice->getL0Debugger()->registerElf(&debugData);

    EXPECT_EQ(static_cast<size_t>(0u), drmMock->registeredDataSize);
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(osInterfaceTmp);
}

TEST_F(L0DebuggerSharedLinuxTest, givenNoOSInterfaceThenRegisterElfAndLinkWithAllocationDoesNothing) {
    NEO::OSInterface *osInterfaceTmp = neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.release();
    NEO::DebugData debugData;
    debugData.vIsa = "01234567890";
    debugData.vIsaSize = 10;
    drmMock->registeredDataSize = 0;

    MockDrmAllocation isaAllocation(neoDevice->getRootDeviceIndex(), AllocationType::kernelIsa, MemoryPool::system4KBPages);
    MockBufferObject bo(neoDevice->getRootDeviceIndex(), drmMock, 3, 0, 0, 1);
    isaAllocation.bufferObjects[0] = &bo;

    neoDevice->getL0Debugger()->registerElfAndLinkWithAllocation(&debugData, &isaAllocation);

    EXPECT_EQ(static_cast<size_t>(0u), drmMock->registeredDataSize);
    EXPECT_EQ(0u, isaAllocation.bufferObjects[0]->getBindExtHandles().size());
    neoDevice->getExecutionEnvironment()->rootDeviceEnvironments[0]->osInterface.reset(osInterfaceTmp);
}

struct SingleAddressSpaceLinuxFixture : public Test<NEO::DeviceFixture> {
    void SetUp() override {
        Test<NEO::DeviceFixture>::SetUp();
    }

    void TearDown() override {
        Test<NEO::DeviceFixture>::TearDown();
    }
};

HWTEST_F(SingleAddressSpaceLinuxFixture, givenDebuggingModeOfflineWhenDebuggerIsCreatedThenItHasCorrectSingleAddressSpaceValue) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->initialize();
    EXPECT_FALSE(debugger->singleAddressSpaceSbaTracking);

    pDevice->getExecutionEnvironment()->setDebuggingMode(DebuggingMode::offline);

    debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->initialize();
    EXPECT_TRUE(debugger->singleAddressSpaceSbaTracking);
}
