/*
 * Copyright (c) 2017, Intel Corporation
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

#include "test.h"

using namespace OCLRT;

OsLibrary *setAdapterInfo(const void *platform, const void *gtSystemInfo);

typedef Test<WddmGmmFixture> WddmTest;

typedef Test<WddmInstrumentationGmmFixture> WddmInstrumentationTest;

typedef WddmDummyFixture WddmTestSingle;

class WddmPreemptionTests : public WddmTest {
  public:
    void SetUp() override {
        WddmTest::SetUp();
        dbgRestorer = new DebugManagerStateRestore();
    }

    void TearDown() override {
        if (mockWddm) {
            delete mockWddm;
        }
        delete dbgRestorer;
        WddmTest::TearDown();
    }

    template <typename GfxFamily>
    void createAndInitWddm(unsigned int forceReturnPreemptionRegKeyValue) {
        mockWddm = new WddmMock();
        auto regReader = new RegistryReaderMock();
        mockWddm->registryReader.reset(regReader);
        regReader->forceRetValue = forceReturnPreemptionRegKeyValue;
        mockWddm->init<GfxFamily>();
    }

    WddmMock *mockWddm = nullptr;
    DebugManagerStateRestore *dbgRestorer = nullptr;
};

class WddmGmmMockGdiFixture : public GmmFixture, public WddmFixture {
  public:
    virtual void SetUp() {
        GmmFixture::SetUp();
        WddmFixture::SetUp(&gdi);
    }

    virtual void TearDown() {
        WddmFixture::TearDown();
        GmmFixture::TearDown();
    }
    MockGdi gdi;
};

typedef Test<WddmGmmMockGdiFixture> WddmWithMockGdiTest;
