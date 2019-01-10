/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/preemption.h"
#include "runtime/os_interface/windows/gdi_interface.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "unit_tests/fixtures/gmm_environment_fixture.h"
#include "unit_tests/mocks/mock_wddm.h"
#include "unit_tests/os_interface/windows/mock_gdi_interface.h"
#include "unit_tests/os_interface/windows/gdi_dll_fixture.h"
#include "mock_gmm_memory.h"
#include "test.h"

namespace OCLRT {
struct WddmFixture : public GmmEnvironmentFixture {
    void SetUp() override {
        GmmEnvironmentFixture::SetUp();
        wddm = static_cast<WddmMock *>(Wddm::createWddm());
        osInterface = std::make_unique<OSInterface>();
        osInterface->get()->setWddm(wddm);
        gdi = new MockGdi();
        wddm->gdi.reset(gdi);
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
        wddm->init(preemptionMode);
        osContext = std::make_unique<OsContext>(osInterface.get(), 0u, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0], preemptionMode);
        osContextWin = osContext->get();
        ASSERT_TRUE(wddm->isInitialized());
    }

    void TearDown() override {
        GmmEnvironmentFixture::TearDown();
    };

    WddmMock *wddm = nullptr;
    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<OsContext> osContext;
    OsContextWin *osContextWin = nullptr;

    MockGdi *gdi = nullptr;
};

struct WddmFixtureWithMockGdiDll : public GmmEnvironmentFixture, public GdiDllFixture {
    void SetUp() override {
        GmmEnvironmentFixture::SetUp();
        GdiDllFixture::SetUp();
        wddm = static_cast<WddmMock *>(Wddm::createWddm());
        osInterface = std::make_unique<OSInterface>();
        osInterface->get()->setWddm(wddm);
    }

    void init() {
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
        EXPECT_TRUE(wddm->init(preemptionMode));
        osContext = std::make_unique<OsContext>(osInterface.get(), 0u, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0], preemptionMode);
        osContextWin = osContext->get();
        ASSERT_TRUE(wddm->isInitialized());
    }

    void TearDown() override {
        GdiDllFixture::TearDown();
        GmmEnvironmentFixture::TearDown();
    }

    WddmMock *wddm = nullptr;
    std::unique_ptr<OSInterface> osInterface;
    std::unique_ptr<OsContext> osContext;
    OsContextWin *osContextWin = nullptr;
};

struct WddmInstrumentationGmmFixture : public GmmEnvironmentFixture {
    void SetUp() override {
        GmmEnvironmentFixture::SetUp();
        wddm.reset(static_cast<WddmMock *>(Wddm::createWddm()));
        gmmMem = new ::testing::NiceMock<GmockGmmMemory>();
        wddm->gmmMemory.reset(gmmMem);
    }
    void TearDown() override {
        GmmEnvironmentFixture::TearDown();
    }

    std::unique_ptr<WddmMock> wddm;
    GmockGmmMemory *gmmMem = nullptr;
};

using WddmTest = Test<WddmFixture>;
using WddmTestWithMockGdiDll = Test<WddmFixtureWithMockGdiDll>;
using WddmInstrumentationTest = Test<WddmInstrumentationGmmFixture>;
using WddmTestSingle = ::testing::Test;
} // namespace OCLRT
