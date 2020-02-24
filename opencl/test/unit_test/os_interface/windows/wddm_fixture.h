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
#include "shared/source/os_interface/windows/os_interface.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"
#include "shared/test/unit_test/os_interface/windows/mock_gdi_interface.h"
#include "opencl/source/platform/platform.h"
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
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[0].get()));
        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->get()->setWddm(wddm);
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.get();
        gdi = new MockGdi();
        wddm->resetGdi(gdi);
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
        wddm->init();
        osContext = std::make_unique<OsContextWin>(*osInterface->get()->getWddm(), 0u, 1u, HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0], preemptionMode, false);
        mockTemporaryResources = static_cast<MockWddmResidentAllocationsContainer *>(wddm->temporaryResources.get());
    }

    WddmMock *wddm = nullptr;
    OSInterface *osInterface;
    std::unique_ptr<OsContextWin> osContext;
    ExecutionEnvironment *executionEnvironment;

    MockGdi *gdi = nullptr;
    MockWddmResidentAllocationsContainer *mockTemporaryResources;
};

struct WddmFixtureWithMockGdiDll : public GdiDllFixture {
    void SetUp() override {
        executionEnvironment = platform()->peekExecutionEnvironment();
        GdiDllFixture::SetUp();
        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[0].get()));
        wddmMockInterface = new WddmMockInterface20(*wddm);
        wddm->wddmInterface.reset(wddmMockInterface);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->rootDeviceEnvironments[0]->osInterface->get()->setWddm(wddm);
        executionEnvironment->rootDeviceEnvironments[0]->memoryOperationsInterface = std::make_unique<WddmMemoryOperationsHandler>(wddm);
        osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.get();
    }

    void init() {
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
        wddmMockInterface = static_cast<WddmMockInterface20 *>(wddm->wddmInterface.release());
        wddm->init();
        wddm->wddmInterface.reset(wddmMockInterface);
        osContext = std::make_unique<OsContextWin>(*osInterface->get()->getWddm(), 0u, 1, HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0], preemptionMode, false);
    }

    void TearDown() override {
        GdiDllFixture::TearDown();
    }

    WddmMock *wddm = nullptr;
    OSInterface *osInterface;
    std::unique_ptr<OsContextWin> osContext;
    ExecutionEnvironment *executionEnvironment;
    WddmMockInterface20 *wddmMockInterface = nullptr;
};

struct WddmInstrumentationGmmFixture {
    void SetUp() {
        executionEnvironment = platform()->peekExecutionEnvironment();
        wddm.reset(static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment->rootDeviceEnvironments[0].get())));
        gmmMem = new ::testing::NiceMock<GmockGmmMemory>(executionEnvironment->getGmmClientContext());
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
