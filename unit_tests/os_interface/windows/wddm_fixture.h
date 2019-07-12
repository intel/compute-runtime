/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/preemption.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/os_interface/windows/gdi_interface.h"
#include "runtime/os_interface/windows/os_context_win.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/mocks/mock_wddm_residency_allocations_container.h"
#include "unit_tests/os_interface/windows/gdi_dll_fixture.h"
#include "unit_tests/os_interface/windows/mock_gdi_interface.h"

#include "mock_gmm_memory.h"

namespace NEO {
struct WddmFixture : ::testing::Test {
    void SetUp() override {
        executionEnvironment = platformImpl->peekExecutionEnvironment();
        wddm = static_cast<WddmMock *>(Wddm::createWddm());
        executionEnvironment->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->osInterface->get()->setWddm(wddm);
        osInterface = executionEnvironment->osInterface.get();
        gdi = new MockGdi();
        wddm->gdi.reset(gdi);
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
        auto hwInfo = *platformDevices[0];
        wddm->init(hwInfo);
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
        executionEnvironment = platformImpl->peekExecutionEnvironment();
        GdiDllFixture::SetUp();
        wddm = static_cast<WddmMock *>(Wddm::createWddm());
        executionEnvironment->osInterface = std::make_unique<OSInterface>();
        executionEnvironment->osInterface->get()->setWddm(wddm);
        osInterface = executionEnvironment->osInterface.get();
    }

    void init() {
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
        auto hwInfo = *platformDevices[0];
        wddm->init(hwInfo);
        osContext = std::make_unique<OsContextWin>(*osInterface->get()->getWddm(), 0u, 1, HwHelper::get(platformDevices[0]->platform.eRenderCoreFamily).getGpgpuEngineInstances()[0], preemptionMode, false);
    }

    void TearDown() override {
        GdiDllFixture::TearDown();
    }

    WddmMock *wddm = nullptr;
    OSInterface *osInterface;
    std::unique_ptr<OsContextWin> osContext;
    ExecutionEnvironment *executionEnvironment;
};

struct WddmInstrumentationGmmFixture {
    void SetUp() {
        executionEnvironment = platformImpl->peekExecutionEnvironment();
        wddm.reset(static_cast<WddmMock *>(Wddm::createWddm()));
        gmmMem = new ::testing::NiceMock<GmockGmmMemory>();
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
