/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/mocks/mock_wddm_interface32.h"
#include "shared/test/common/os_interface/windows/gdi_dll_fixture.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

struct Wddm32TestsWithoutWddmInit : public ::testing::Test, GdiDllFixture {
    void SetUp() override {
        GdiDllFixture::setUp();

        wddm = static_cast<WddmMock *>(Wddm::createWddm(nullptr, *executionEnvironment.rootDeviceEnvironments[0].get()));
        auto &osInterface = executionEnvironment.rootDeviceEnvironments[0]->osInterface;
        osInterface = std::make_unique<OSInterface>();
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(wddm));

        wddm->featureTable->flags.ftrWddmHwQueues = true;
        wddmMockInterface = new WddmMockInterface32(*wddm);
        wddm->wddmInterface.reset(wddmMockInterface);
    }

    void init() {
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo);
        wddmMockInterface = static_cast<WddmMockInterface32 *>(wddm->wddmInterface.release());
        wddm->init();
        wddm->wddmInterface.reset(wddmMockInterface);
        auto &gfxCoreHelper = this->executionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
        osContext = std::make_unique<OsContextWin>(*wddm, 0, 0u,
                                                   EngineDescriptorHelper::getDefaultDescriptor(gfxCoreHelper.getGpgpuEngineInstances(*this->executionEnvironment.rootDeviceEnvironments[0])[0], preemptionMode));
        osContext->ensureContextInitialized(false);
    }

    void TearDown() override {
        GdiDllFixture::tearDown();
    }

    MockExecutionEnvironment executionEnvironment;
    std::unique_ptr<OsContextWin> osContext;
    WddmMock *wddm = nullptr;
    WddmMockInterface32 *wddmMockInterface = nullptr;
};

struct Wddm32Tests : public Wddm32TestsWithoutWddmInit {
    using Wddm32TestsWithoutWddmInit::TearDown;
    void SetUp() override {
        Wddm32TestsWithoutWddmInit::SetUp();
        init();
    }
};