/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "unit_tests/os_interface/windows/wddm_fixture.h"
#include "runtime/command_stream/preemption.h"

#include "test.h"

using namespace OCLRT;

OsLibrary *setAdapterInfo(const PLATFORM *platform, const GT_SYSTEM_INFO *gtSystemInfo);

typedef Test<WddmFixture> WddmTest;
typedef Test<WddmFixtureWithMockGdiDll> WddmTestWithMockGdiDll;

typedef Test<WddmInstrumentationGmmFixture> WddmInstrumentationTest;

typedef ::testing::Test WddmTestSingle;

class WddmPreemptionTests : public WddmTestWithMockGdiDll {
  public:
    void SetUp() override {
        WddmTestWithMockGdiDll::SetUp();
        const HardwareInfo hwInfo = *platformDevices[0];
        memcpy(&hwInfoTest, &hwInfo, sizeof(hwInfoTest));
        dbgRestorer = new DebugManagerStateRestore();
    }

    void TearDown() override {
        delete dbgRestorer;
        WddmTestWithMockGdiDll::TearDown();
    }

    template <typename GfxFamily>
    void createAndInitWddm(unsigned int forceReturnPreemptionRegKeyValue) {
        wddm.reset(static_cast<WddmMock *>(Wddm::createWddm()));
        auto regReader = new RegistryReaderMock();
        wddm->registryReader.reset(regReader);
        regReader->forceRetValue = forceReturnPreemptionRegKeyValue;
        PreemptionMode preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfoTest);
        wddm->setPreemptionMode(preemptionMode);
        wddm->init<GfxFamily>();
    }

    DebugManagerStateRestore *dbgRestorer = nullptr;
    HardwareInfo hwInfoTest;
};
