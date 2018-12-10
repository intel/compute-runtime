/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "test.h"

#include "unit_tests/fixtures/gmm_environment_fixture.h"
#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_gmm_page_table_mngr.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/os_interface/windows/wddm_fixture.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "unit_tests/os_interface/windows/mock_gdi_interface.h"
#include "unit_tests/os_interface/windows/mock_wddm_memory_manager.h"
#include <type_traits>

using namespace OCLRT;
using namespace ::testing;

class WddmMemoryManagerFixture : public GmmEnvironmentFixture, public GdiDllFixture {
  public:
    void SetUp() override;

    void TearDown() override {
        GdiDllFixture::TearDown();
        GmmEnvironmentFixture::TearDown();
    }

    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    WddmMock *wddm;
};

typedef ::Test<WddmMemoryManagerFixture> WddmMemoryManagerTest;

class MockWddmMemoryManagerFixture : public GmmEnvironmentFixture {
  public:
    void SetUp() {
        GmmEnvironmentFixture::SetUp();

        gdi = new MockGdi();

        wddm = static_cast<WddmMock *>(Wddm::createWddm());
        wddm->gdi.reset(gdi);
        constexpr uint64_t heap32Base = (is32bit) ? 0x1000 : 0x800000000000;
        wddm->setHeap32(heap32Base, 1000 * MemoryConstants::pageSize - 1);
        EXPECT_TRUE(wddm->init(PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0])));

        executionEnvironment.osInterface.reset(new OSInterface());
        executionEnvironment.osInterface->get()->setWddm(wddm);

        memoryManager = std::make_unique<MockWddmMemoryManager>(wddm, executionEnvironment);
        memoryManager->createAndRegisterOsContext(gpgpuEngineInstances[0], PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));

        osContext = memoryManager->getRegisteredOsContext(0);
        osContext->incRefInternal();
    }

    void TearDown() {
        osContext->decRefInternal();
        GmmEnvironmentFixture::TearDown();
    }

    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    WddmMock *wddm = nullptr;
    OsContext *osContext = nullptr;
    MockGdi *gdi = nullptr;
};

typedef ::Test<MockWddmMemoryManagerFixture> WddmMemoryManagerResidencyTest;

class GmockWddm : public Wddm {
  public:
    using Wddm::device;

    GmockWddm() {
        virtualAllocAddress = OCLRT::windowsMinAddress;
    }
    ~GmockWddm() = default;

    bool virtualFreeWrapper(void *ptr, size_t size, uint32_t flags) {
        return true;
    }

    void *virtualAllocWrapper(void *inPtr, size_t size, uint32_t flags, uint32_t type) {
        void *tmp = reinterpret_cast<void *>(virtualAllocAddress);
        size += MemoryConstants::pageSize;
        size -= size % MemoryConstants::pageSize;
        virtualAllocAddress += size;
        return tmp;
    }
    uintptr_t virtualAllocAddress;
    MOCK_METHOD4(makeResident, bool(D3DKMT_HANDLE *handles, uint32_t count, bool cantTrimFurther, uint64_t *numberOfBytesToTrim));
    MOCK_METHOD1(createAllocationsAndMapGpuVa, NTSTATUS(OsHandleStorage &osHandles));

    NTSTATUS baseCreateAllocationAndMapGpuVa(OsHandleStorage &osHandles) {
        return Wddm::createAllocationsAndMapGpuVa(osHandles);
    }
};

class WddmMemoryManagerFixtureWithGmockWddm : public GmmEnvironmentFixture {
  public:
    MockWddmMemoryManager *memoryManager = nullptr;

    void SetUp() {
        GmmEnvironmentFixture::SetUp();
        // wddm is deleted by memory manager
        wddm = new NiceMock<GmockWddm>;
        osInterface = std::make_unique<OSInterface>();
        ASSERT_NE(nullptr, wddm);
        auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]);
        EXPECT_TRUE(wddm->init(preemptionMode));
        osInterface->get()->setWddm(wddm);
        wddm->init(preemptionMode);
        memoryManager = new (std::nothrow) MockWddmMemoryManager(wddm, executionEnvironment);
        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
        memoryManager->createAndRegisterOsContext(gpgpuEngineInstances[0], preemptionMode);

        osContext = memoryManager->getRegisteredOsContext(0);
        osContext->incRefInternal();

        ON_CALL(*wddm, createAllocationsAndMapGpuVa(::testing::_)).WillByDefault(::testing::Invoke(wddm, &GmockWddm::baseCreateAllocationAndMapGpuVa));
    }

    void TearDown() {
        osContext->decRefInternal();
        delete memoryManager;
        GmmEnvironmentFixture::TearDown();
    }

    NiceMock<GmockWddm> *wddm = nullptr;
    std::unique_ptr<OSInterface> osInterface;
    OsContext *osContext;
};

typedef ::Test<WddmMemoryManagerFixtureWithGmockWddm> WddmMemoryManagerTest2;

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

using MockWddmMemoryManagerTest = ::Test<GmmEnvironmentFixture>;
using OsAgnosticMemoryManagerUsingWddmTest = MockWddmMemoryManagerTest;
