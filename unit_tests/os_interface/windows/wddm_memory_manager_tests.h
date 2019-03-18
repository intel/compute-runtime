/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/os_interface/windows/os_interface.h"
#include "test.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/mocks/mock_gmm_page_table_mngr.h"
#include "unit_tests/os_interface/windows/mock_gdi_interface.h"
#include "unit_tests/os_interface/windows/mock_wddm_memory_manager.h"
#include "unit_tests/os_interface/windows/wddm_fixture.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <type_traits>

using namespace OCLRT;
using namespace ::testing;

class WddmMemoryManagerFixture : public GdiDllFixture {
  public:
    void SetUp() override;

    void TearDown() override {
        GdiDllFixture::TearDown();
    }

    ExecutionEnvironment *executionEnvironment;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    WddmMock *wddm;
};

typedef ::Test<WddmMemoryManagerFixture> WddmMemoryManagerTest;

class MockWddmMemoryManagerFixture {
  public:
    void SetUp() {
        executionEnvironment = platformImpl->peekExecutionEnvironment();
        gdi = new MockGdi();

        wddm = static_cast<WddmMock *>(Wddm::createWddm());
        wddm->gdi.reset(gdi);
        constexpr uint64_t heap32Base = (is32bit) ? 0x1000 : 0x800000000000;
        wddm->setHeap32(heap32Base, 1000 * MemoryConstants::pageSize - 1);
        EXPECT_TRUE(wddm->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0])));

        executionEnvironment->osInterface.reset(new OSInterface());
        executionEnvironment->osInterface->get()->setWddm(wddm);

        memoryManager = std::make_unique<MockWddmMemoryManager>(wddm, *executionEnvironment);
        osContext = memoryManager->createAndRegisterOsContext(nullptr, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0],
                                                              1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]), false);

        osContext->incRefInternal();
    }

    void TearDown() {
        osContext->decRefInternal();
    }

    ExecutionEnvironment *executionEnvironment;
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    WddmMock *wddm = nullptr;
    OsContext *osContext = nullptr;
    MockGdi *gdi = nullptr;
};

typedef ::Test<MockWddmMemoryManagerFixture> WddmMemoryManagerResidencyTest;

class ExecutionEnvironmentFixture : public ::testing::Test {
  public:
    ExecutionEnvironmentFixture() {
        executionEnvironment = platformImpl->peekExecutionEnvironment();
    }

    ExecutionEnvironment *executionEnvironment;
};

class WddmMemoryManagerFixtureWithGmockWddm : public ExecutionEnvironmentFixture {
  public:
    MockWddmMemoryManager *memoryManager = nullptr;

    void SetUp() override {
        // wddm is deleted by memory manager
        wddm = new NiceMock<GmockWddm>;
        osInterface = std::make_unique<OSInterface>();
        ASSERT_NE(nullptr, wddm);
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
        EXPECT_TRUE(wddm->init(preemptionMode));
        osInterface->get()->setWddm(wddm);
        wddm->init(preemptionMode);
        memoryManager = new (std::nothrow) MockWddmMemoryManager(wddm, *executionEnvironment);
        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
        osContext = memoryManager->createAndRegisterOsContext(nullptr, HwHelper::get(platformDevices[0]->pPlatform->eRenderCoreFamily).getGpgpuEngineInstances()[0], 1, preemptionMode, false);

        osContext->incRefInternal();

        ON_CALL(*wddm, createAllocationsAndMapGpuVa(::testing::_)).WillByDefault(::testing::Invoke(wddm, &GmockWddm::baseCreateAllocationAndMapGpuVa));
    }

    void TearDown() override {
        osContext->decRefInternal();
        delete memoryManager;
    }

    NiceMock<GmockWddm> *wddm = nullptr;
    std::unique_ptr<OSInterface> osInterface;
    OsContext *osContext;
};

using WddmMemoryManagerTest2 = WddmMemoryManagerFixtureWithGmockWddm;

class BufferWithWddmMemory : public ::testing::Test,
                             public WddmMemoryManagerFixture {
  public:
  protected:
    void SetUp() {
        WddmMemoryManagerFixture::SetUp();
        tmp = context.getMemoryManager();
        context.setMemoryManager(memoryManager.get());
        flags = 0;
    }

    void TearDown() {
        context.setMemoryManager(tmp);
        WddmMemoryManagerFixture::TearDown();
    }

    MemoryManager *tmp;
    MockContext context;
    cl_mem_flags flags;
    cl_int retVal;
};

class WddmMemoryManagerSimpleTest : public MockWddmMemoryManagerFixture, public ::testing::Test {
  public:
    void SetUp() override {
        MockWddmMemoryManagerFixture::SetUp();
        wddm->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));
    }
    void TearDown() override {
        MockWddmMemoryManagerFixture::TearDown();
    }
};

using MockWddmMemoryManagerTest = ExecutionEnvironmentFixture;
using OsAgnosticMemoryManagerUsingWddmTest = MockWddmMemoryManagerTest;
