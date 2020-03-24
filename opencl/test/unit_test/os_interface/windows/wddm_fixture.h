/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/os_environment_win.h"
#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"
#include "shared/test/unit_test/os_interface/windows/mock_gdi_interface.h"

#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"
#include "opencl/test/unit_test/mocks/mock_wddm.h"
#include "opencl/test/unit_test/mocks/mock_wddm_interface20.h"
#include "opencl/test/unit_test/mocks/mock_wddm_residency_allocations_container.h"
#include "opencl/test/unit_test/os_interface/windows/gdi_dll_fixture.h"
#include "test.h"

#include "mock_gmm_memory.h"

namespace NEO {
struct WddmFixture : ::testing::Test {
    void SetUp() override {
        executionEnvironment = platform()->peekExecutionEnvironment();
        rootDeviceEnvironemnt = executionEnvironment->rootDeviceEnvironments[0].get();
        auto osEnvironment = new OsEnvironmentWin();
        gdi = new MockGdi();
        osEnvironment->gdi.reset(gdi);
        executionEnvironment->osEnvironment.reset(osEnvironment);
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironemnt));
        rootDeviceEnvironemnt->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironemnt->osInterface->get()->setWddm(wddm);
        rootDeviceEnvironemnt->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        osInterface = rootDeviceEnvironemnt->osInterface.get();
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddm->init();
        auto hwInfo = rootDeviceEnvironemnt->getHardwareInfo();
        auto engine = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0];
        osContext = std::make_unique<OsContextWin>(*osInterface->get()->getWddm(), 0u, 1u, engine, preemptionMode,
                                                   false, false, false);
        mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(wddm->temporaryResources.get());
    }

    WddmMock *wddm = nullptr;
    OSInterface *osInterface;
    ExecutionEnvironment *executionEnvironment;
    RootDeviceEnvironment *rootDeviceEnvironemnt = nullptr;
    std::unique_ptr<OsContextWin> osContext;

    MockGdi *gdi = nullptr;
    MockWddmResidentAllocationsContainer *mockTemporaryResources;
};

struct WddmFixtureWithMockGdiDll : public GdiDllFixture {
    void SetUp() override {
        executionEnvironment = platform()->peekExecutionEnvironment();
        rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        GdiDllFixture::SetUp();
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment));
        wddmMockInterface = new WddmMockInterface20(*wddm);
        wddm->wddmInterface.reset(wddmMockInterface);
        rootDeviceEnvironment->osInterface = std::make_unique<OSInterface>();
        rootDeviceEnvironment->osInterface->get()->setWddm(wddm);
        rootDeviceEnvironment->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        osInterface = rootDeviceEnvironment->osInterface.get();
    }

    void init() {
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddmMockInterface = static_cast<WddmMockInterface20 *>(wddm->wddmInterface.release());
        wddm->init();
        wddm->wddmInterface.reset(wddmMockInterface);

        auto hwInfo = rootDeviceEnvironment->getHardwareInfo();
        auto engine = HwHelper::get(defaultHwInfo->platform.eRenderCoreFamily).getGpgpuEngineInstances(*hwInfo)[0];
        osContext = std::make_unique<OsContextWin>(*osInterface->get()->getWddm(), 0u, 1, engine, preemptionMode,
                                                   false, false, false);
    }

    void TearDown() override {
        GdiDllFixture::TearDown();
    }

    WddmMock *wddm = nullptr;
    OSInterface *osInterface;
    std::unique_ptr<OsContextWin> osContext;
    ExecutionEnvironment *executionEnvironment;
    WddmMockInterface20 *wddmMockInterface = nullptr;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
};

struct WddmInstrumentationGmmFixture {
    void SetUp() {
        executionEnvironment = platform()->peekExecutionEnvironment();
        auto rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0].get();
        wddm.reset(static_cast<WddmMock *>(Wddm::createWddm(nullptr, *rootDeviceEnvironment)));
        gmmMem = new ::testing::NiceMock<GmockGmmMemory>(rootDeviceEnvironment->getGmmClientContext());
        wddm->gmmMemory.reset(gmmMem);
    }
    void TearDown() {
    }

    std::unique_ptr<WddmMock> wddm;
    GmockGmmMemory *gmmMem = nullptr;
    ExecutionEnvironment *executionEnvironment;
};

using WddmTest = WddmFixture;
using WddmTestWithMockGdiDll = Test<WddmFixtureWithMockGdiDll>;
using WddmInstrumentationTest = Test<WddmInstrumentationGmmFixture>;
using WddmTestSingle = ::testing::Test;
} // namespace NEO
