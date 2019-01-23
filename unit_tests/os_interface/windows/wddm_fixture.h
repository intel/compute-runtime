/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/os_interface/windows/gdi_interface.h"
#include "runtime/os_interface/windows/os_context_win.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/os_interface/windows/gdi_dll_fixture.h"
#include "unit_tests/os_interface/windows/mock_gdi_interface.h"

#include "mock_gmm_memory.h"

namespace OCLRT {
struct WddmFixture : ::testing::Test {
    void SetUp() {
        executionEnvironment = platformImpl->peekExecutionEnvironment();
        wddm = static_cast<WddmMock *>(Wddm::createWddm());
        osInterface = std::make_unique<OSInterface>();
        osInterface->get()->setWddm(wddm);
        gdi = new MockGdi();
        wddm->gdi.reset(gdi);
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
        wddm->init(preemptionMode);
        osContext = std::make_unique<OsContextWin>(*osInterface->get()->getWddm(), 0u, 1u, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0], preemptionMode);
        ASSERT_TRUE(wddm->isInitialized());
    }

    WddmMock *wddm = nullptr;
    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<OsContextWin> osContext;
    ExecutionEnvironment *executionEnvironment;

    MockGdi *gdi = nullptr;
};

struct WddmFixtureWithMockGdiDll : public GdiDllFixture {
    void SetUp() override {
        executionEnvironment = platformImpl->peekExecutionEnvironment();
        GdiDllFixture::SetUp();
        wddm = static_cast<WddmMock *>(Wddm::createWddm());
        osInterface = std::make_unique<OSInterface>();
        osInterface->get()->setWddm(wddm);
    }

    void init() {
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
        EXPECT_TRUE(wddm->init(preemptionMode));
        osContext = std::make_unique<OsContextWin>(*osInterface->get()->getWddm(), 0u, 1, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0], preemptionMode);
        ASSERT_TRUE(wddm->isInitialized());
    }

    void TearDown() override {
        GdiDllFixture::TearDown();
    }

    WddmMock *wddm = nullptr;
    std::unique_ptr<OSInterface> osInterface;
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
} // namespace OCLRT
