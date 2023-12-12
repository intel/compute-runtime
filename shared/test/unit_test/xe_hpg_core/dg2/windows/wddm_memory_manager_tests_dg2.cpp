/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/windows/mock_gmm_memory_base.h"
#include "shared/test/common/os_interface/windows/gdi_dll_fixture.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using Dg2WddmTest = ::testing::Test;

DG2TEST_F(Dg2WddmTest, givenG10A0WhenGettingLocalMemoryAccessModeThenCorrectValueIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForcePreferredAllocationMethod.set(static_cast<int32_t>(GfxMemoryAllocationMethod::useUmdSystemPtr));
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->initGmm();
    WddmMock *wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
    MockGmmMemoryBase *gmmMemory = new MockGmmMemoryBase(rootDeviceEnvironment->getGmmClientContext());
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    wddm->init();

    wddm->gmmMemory.reset(gmmMemory);

    rootDeviceEnvironment->getMutableHardwareInfo()->platform.usRevId = revIdA0;
    rootDeviceEnvironment->getMutableHardwareInfo()->platform.usDeviceID = dg2G10DeviceIds[0];

    MockWddmMemoryManager memoryManager = MockWddmMemoryManager(false, true, executionEnvironment);

    auto graphicsAllocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, 4096u, AllocationType::linearStream, mockDeviceBitfield});

    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(graphicsAllocation->getAllocationType(), AllocationType::linearStream);
    auto gpuAddress = graphicsAllocation->getGpuAddress();
    auto gpuAddressEnd = gpuAddress + 4096u;
    auto gmmHelper = memoryManager.getGmmHelper(graphicsAllocation->getRootDeviceIndex());

    EXPECT_NE(MemoryPool::localMemory, graphicsAllocation->getMemoryPool());
    EXPECT_FALSE(graphicsAllocation->isLocked());
    auto &partition = wddm->getGfxPartition();

    EXPECT_GE(gpuAddress, gmmHelper->canonize(partition.SVM.Base));
    EXPECT_LE(gpuAddressEnd, gmmHelper->canonize(partition.SVM.Limit));

    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

DG2TEST_F(Dg2WddmTest, givenG10B0WhenGettingLocalMemoryAccessModeThenCorrectValueIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->initGmm();
    WddmMock *wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
    MockGmmMemoryBase *gmmMemory = new MockGmmMemoryBase(rootDeviceEnvironment->getGmmClientContext());
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    wddm->init();
    wddm->gmmMemory.reset(gmmMemory);

    rootDeviceEnvironment->getMutableHardwareInfo()->platform.usRevId = revIdB0;
    rootDeviceEnvironment->getMutableHardwareInfo()->platform.usDeviceID = dg2G10DeviceIds[0];

    MockWddmMemoryManager memoryManager = MockWddmMemoryManager(false, true, executionEnvironment);
    auto graphicsAllocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, 4096u, AllocationType::linearStream, mockDeviceBitfield});

    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(graphicsAllocation->getAllocationType(), AllocationType::linearStream);
    auto gpuAddress = graphicsAllocation->getGpuAddress();
    auto gpuAddressEnd = gpuAddress + 4096u;
    auto gmmHelper = memoryManager.getGmmHelper(graphicsAllocation->getRootDeviceIndex());

    EXPECT_EQ(MemoryPool::localMemory, graphicsAllocation->getMemoryPool());
    EXPECT_TRUE(graphicsAllocation->isLocked());
    auto &partition = wddm->getGfxPartition();

    if (is64bit) {
        EXPECT_GE(gpuAddress, gmmHelper->canonize(partition.Standard64KB.Base));
        EXPECT_LE(gpuAddressEnd, gmmHelper->canonize(partition.Standard64KB.Limit));
    } else {
        EXPECT_GE(gpuAddress, gmmHelper->canonize(partition.SVM.Base));
        EXPECT_LE(gpuAddressEnd, gmmHelper->canonize(partition.SVM.Limit));
    }

    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

DG2TEST_F(Dg2WddmTest, givenG11WhenGettingLocalMemoryAccessModeThenCorrectValueIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->initGmm();
    WddmMock *wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
    MockGmmMemoryBase *gmmMemory = new MockGmmMemoryBase(rootDeviceEnvironment->getGmmClientContext());
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    wddm->init();
    wddm->gmmMemory.reset(gmmMemory);

    rootDeviceEnvironment->getMutableHardwareInfo()->platform.usRevId = revIdB0;
    rootDeviceEnvironment->getMutableHardwareInfo()->platform.usDeviceID = dg2G11DeviceIds[0];

    MockWddmMemoryManager memoryManager = MockWddmMemoryManager(false, true, executionEnvironment);
    auto graphicsAllocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, 4096u, AllocationType::linearStream, mockDeviceBitfield});

    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(graphicsAllocation->getAllocationType(), AllocationType::linearStream);
    auto gpuAddress = graphicsAllocation->getGpuAddress();
    auto gpuAddressEnd = gpuAddress + 4096u;
    auto gmmHelper = memoryManager.getGmmHelper(graphicsAllocation->getRootDeviceIndex());

    EXPECT_EQ(MemoryPool::localMemory, graphicsAllocation->getMemoryPool());
    EXPECT_TRUE(graphicsAllocation->isLocked());
    auto &partition = wddm->getGfxPartition();

    if (is64bit) {
        EXPECT_GE(gpuAddress, gmmHelper->canonize(partition.Standard64KB.Base));
        EXPECT_LE(gpuAddressEnd, gmmHelper->canonize(partition.Standard64KB.Limit));
    } else {
        EXPECT_GE(gpuAddress, gmmHelper->canonize(partition.SVM.Base));
        EXPECT_LE(gpuAddressEnd, gmmHelper->canonize(partition.SVM.Limit));
    }

    memoryManager.freeGraphicsMemory(graphicsAllocation);
}

DG2TEST_F(Dg2WddmTest, givenG12WhenGettingLocalMemoryAccessModeThenCorrectValueIsReturned) {
    MockExecutionEnvironment executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironment = executionEnvironment.rootDeviceEnvironments[0].get();
    rootDeviceEnvironment->initGmm();
    WddmMock *wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
    MockGmmMemoryBase *gmmMemory = new MockGmmMemoryBase(rootDeviceEnvironment->getGmmClientContext());
    rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
    rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    wddm->init();
    wddm->gmmMemory.reset(gmmMemory);

    rootDeviceEnvironment->getMutableHardwareInfo()->platform.usRevId = revIdB0;
    rootDeviceEnvironment->getMutableHardwareInfo()->platform.usDeviceID = dg2G12DeviceIds[0];

    MockWddmMemoryManager memoryManager = MockWddmMemoryManager(false, true, executionEnvironment);
    auto graphicsAllocation = memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, 4096u, AllocationType::linearStream, mockDeviceBitfield});

    ASSERT_NE(nullptr, graphicsAllocation);
    EXPECT_NE(nullptr, graphicsAllocation->getUnderlyingBuffer());
    EXPECT_EQ(graphicsAllocation->getAllocationType(), AllocationType::linearStream);
    auto gpuAddress = graphicsAllocation->getGpuAddress();
    auto gpuAddressEnd = gpuAddress + 4096u;
    auto gmmHelper = memoryManager.getGmmHelper(graphicsAllocation->getRootDeviceIndex());

    EXPECT_EQ(MemoryPool::localMemory, graphicsAllocation->getMemoryPool());
    EXPECT_TRUE(graphicsAllocation->isLocked());
    auto &partition = wddm->getGfxPartition();

    if (is64bit) {
        EXPECT_GE(gpuAddress, gmmHelper->canonize(partition.Standard64KB.Base));
        EXPECT_LE(gpuAddressEnd, gmmHelper->canonize(partition.Standard64KB.Limit));
    } else {
        EXPECT_GE(gpuAddress, gmmHelper->canonize(partition.SVM.Base));
        EXPECT_LE(gpuAddressEnd, gmmHelper->canonize(partition.SVM.Limit));
    }

    memoryManager.freeGraphicsMemory(graphicsAllocation);
}
