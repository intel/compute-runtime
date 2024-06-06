/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/fixtures/mock_execution_environment_gmm_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/mock_wddm_interface20.h"
#include "shared/test/common/mocks/mock_wddm_residency_allocations_container.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/mocks/windows/mock_gmm_memory_base.h"
#include "shared/test/common/os_interface/windows/gdi_dll_fixture.h"
#include "shared/test/common/os_interface/windows/mock_wddm_memory_manager.h"
#include "shared/test/common/test_macros/test.h"

namespace NEO {
struct WddmFixture : public Test<MockExecutionEnvironmentGmmFixture> {
    void SetUp() override {
        MockExecutionEnvironmentGmmFixture::setUp();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->initProductHelper();
        auto osEnvironment = new OsEnvironmentWin();
        gdi = new MockGdi();
        osEnvironment->gdi.reset(gdi);
        executionEnvironment->osEnvironment.reset(osEnvironment);
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        rootDeviceEnvironment->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        osInterface = rootDeviceEnvironment->osInterface.get();
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddm->init();
        executionEnvironment->initializeMemoryManager();
        memoryManager = std::make_unique<MockWddmMemoryManager>(*executionEnvironment);

        auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
        auto engine = gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment)[0];
        osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engine, preemptionMode));
        osContext->ensureContextInitialized(false);
        mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(wddm->temporaryResources.get());
    }

    WddmMock *wddm = nullptr;
    OSInterface *osInterface;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    std::unique_ptr<OsContextWin> osContext;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;

    MockGdi *gdi = nullptr;
    MockWddmResidentAllocationsContainer *mockTemporaryResources;
};

struct WddmFixtureLuid : public Test<MockExecutionEnvironmentGmmFixture> {
    void SetUp() override {
        MockExecutionEnvironmentGmmFixture::setUp();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        osEnvironment = new OsEnvironmentWin();
        gdi = new MockGdi();
        osEnvironment->gdi.reset(gdi);
        executionEnvironment->osEnvironment.reset(osEnvironment);
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        rootDeviceEnvironment->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        osInterface = rootDeviceEnvironment->osInterface.get();
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddm->init();
        auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
        auto engine = gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment)[0];
        osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engine, preemptionMode));
        osContext->ensureContextInitialized(false);
        mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(wddm->temporaryResources.get());
    }

    WddmMock *wddm = nullptr;
    OSInterface *osInterface;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
    OsEnvironmentWin *osEnvironment = nullptr;
    std::unique_ptr<OsContextWin> osContext;

    MockGdi *gdi = nullptr;
    MockWddmResidentAllocationsContainer *mockTemporaryResources;
};

struct WddmFixtureWithMockGdiDll : public GdiDllFixture, public MockExecutionEnvironmentGmmFixture {
    void setUp() {
        MockExecutionEnvironmentGmmFixture::setUp();
        GdiDllFixture::setUp();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        rootDeviceEnvironment->initProductHelper();
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
        wddmMockInterface = new WddmMockInterface20(*wddm);
        wddm->wddmInterface.reset(wddmMockInterface);
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        rootDeviceEnvironment->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        osInterface = rootDeviceEnvironment->osInterface.get();
    }

    void init() {
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddmMockInterface = static_cast<WddmMockInterface20 *>(wddm->wddmInterface.release());
        wddm->init();
        wddm->wddmInterface.reset(wddmMockInterface);

        auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
        auto engine = gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment)[0];
        osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engine, preemptionMode));
        osContext->ensureContextInitialized(false);
    }

    void tearDown() {
        GdiDllFixture::tearDown();
    }

    WddmMock *wddm = nullptr;
    OSInterface *osInterface;
    std::unique_ptr<OsContextWin> osContext;
    WddmMockInterface20 *wddmMockInterface = nullptr;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
};

struct NoCleanupWddmMock : WddmMock {
    using WddmMock::WddmMock;
    bool isDriverAvailable() override {
        return false;
    }

    bool skipResourceCleanup() {
        return true;
    }
};

struct WddmFixtureWithMockGdiDllWddmNoCleanup : public GdiDllFixture, public MockExecutionEnvironmentGmmFixture {
    void setUp() {
        MockExecutionEnvironmentGmmFixture::setUp();
        GdiDllFixture::setUp();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        wddm = new NoCleanupWddmMock(*rootDeviceEnvironment);
        wddmMockInterface = new WddmMockInterface20(*wddm);
        wddm->wddmInterface.reset(wddmMockInterface);
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
        rootDeviceEnvironment->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        osInterface = rootDeviceEnvironment->osInterface.get();
    }

    void init() {
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddmMockInterface = static_cast<WddmMockInterface20 *>(wddm->wddmInterface.release());
        wddm->init();
        wddm->wddmInterface.reset(wddmMockInterface);

        auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
        auto engine = gfxCoreHelper.getGpgpuEngineInstances(*rootDeviceEnvironment)[0];
        osContext = std::make_unique<OsContextWin>(*osInterface->getDriverModel()->as<Wddm>(), 0, 0u, EngineDescriptorHelper::getDefaultDescriptor(engine, preemptionMode));
        osContext->ensureContextInitialized(false);
    }

    void tearDown() {
        GdiDllFixture::tearDown();
    }

    NoCleanupWddmMock *wddm = nullptr;
    OSInterface *osInterface;
    std::unique_ptr<OsContextWin> osContext;
    WddmMockInterface20 *wddmMockInterface = nullptr;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
};

struct WddmInstrumentationGmmFixture : DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();
        executionEnvironment = pDevice->getExecutionEnvironment();
        auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
        gmmMem = new MockGmmMemoryBase(rootDeviceEnvironment->getGmmClientContext());
        wddm->gmmMemory.reset(gmmMem);
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));
    }
    void tearDown() {
        DeviceFixture::tearDown();
    }

    WddmMock *wddm;
    MockGmmMemoryBase *gmmMem = nullptr;
    ExecutionEnvironment *executionEnvironment;
};

using WddmTest = WddmFixture;
using WddmTestWithMockGdiDll = Test<WddmFixtureWithMockGdiDll>;
using WddmTestWithMockGdiDllNoCleanup = Test<WddmFixtureWithMockGdiDllWddmNoCleanup>;
using WddmInstrumentationTest = Test<WddmInstrumentationGmmFixture>;
using WddmTestSingle = ::testing::Test;
} // namespace NEO
