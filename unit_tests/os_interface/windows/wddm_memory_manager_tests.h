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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "test.h"

#include "unit_tests/mocks/mock_context.h"
#include "unit_tests/mocks/mock_gmm_page_table_mngr.h"
#include "unit_tests/mocks/mock_gmm.h"
#include "unit_tests/os_interface/windows/wddm_fixture.h"
#include "unit_tests/os_interface/windows/mock_gdi_interface.h"
#include "unit_tests/os_interface/windows/mock_wddm_memory_manager.h"

using namespace OCLRT;
using namespace ::testing;

class WddmMemoryManagerFixture : public GdiDllFixture {
  public:
    void SetUp() override;

    template <typename FamiltyType>
    void SetUpMm() {
        EXPECT_TRUE(wddm->init<FamiltyType>());
        uint64_t heap32Base = (uint64_t)(0x800000000000);
        if (sizeof(uintptr_t) == 4) {
            heap32Base = 0x1000;
        }
        wddm->setHeap32(heap32Base, 1000 * MemoryConstants::pageSize - 1);
        memoryManager.reset(new (std::nothrow) MockWddmMemoryManager(wddm));
        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
    }

    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    WddmMock *wddm = nullptr;
};

typedef ::Test<WddmMemoryManagerFixture> WddmMemoryManagerTest;

class MockWddmMemoryManagerFixture {
  public:
    void SetUp() {
        wddm = static_cast<WddmMock *>(Wddm::createWddm());
        gdi = new MockGdi();
        wddm->gdi.reset(gdi);
    }

    template <typename FamiltyType>
    void SetUpMm() {
        EXPECT_TRUE(wddm->init<FamiltyType>());
        uint64_t heap32Base = (uint64_t)(0x800000000000);
        if (sizeof(uintptr_t) == 4) {
            heap32Base = 0x1000;
        }
        wddm->setHeap32(heap32Base, 1000 * MemoryConstants::pageSize - 1);
        memoryManager.reset(new (std::nothrow) MockWddmMemoryManager(wddm));
        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
    }

    virtual void TearDown() {}
    std::unique_ptr<MockWddmMemoryManager> memoryManager;
    WddmMock *wddm = nullptr;
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

class WddmMemoryManagerFixtureWithGmockWddm {
  public:
    MockWddmMemoryManager *memoryManager = nullptr;

    void SetUp() {
        // wddm is deleted by memory manager
        wddm = new NiceMock<GmockWddm>;
        ASSERT_NE(nullptr, wddm);
    }

    template <typename FamiltyType>
    void SetUpMm() {
        wddm->init<FamiltyType>();
        memoryManager = new (std::nothrow) MockWddmMemoryManager(wddm);
        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);

        ON_CALL(*wddm, createAllocationsAndMapGpuVa(::testing::_)).WillByDefault(::testing::Invoke(wddm, &GmockWddm::baseCreateAllocationAndMapGpuVa));
    }
    void TearDown() {
        delete memoryManager;
        wddm = nullptr;
    }

    NiceMock<GmockWddm> *wddm;
};

typedef ::Test<WddmMemoryManagerFixtureWithGmockWddm> WddmMemoryManagerTest2;

class BufferWithWddmMemory : public ::testing::Test,
                             public WddmMemoryManagerFixture {
  public:
  protected:
    void SetUp() {
        WddmMemoryManagerFixture::SetUp();
        tmp = context.getMemoryManager();
    }

    template <typename FamiltyType>
    void SetUpMm() {
        EXPECT_TRUE(wddm->init<FamiltyType>());
        uint64_t heap32Base = (uint64_t)(0x800000000000);
        if (sizeof(uintptr_t) == 4) {
            heap32Base = 0x1000;
        }
        wddm->setHeap32(heap32Base, 1000 * MemoryConstants::pageSize - 1);
        memoryManager.reset(new (std::nothrow) MockWddmMemoryManager(wddm));
        //assert we have memory manager
        ASSERT_NE(nullptr, memoryManager);
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

typedef ::testing::Test MockWddmMemoryManagerTest;

typedef MockWddmMemoryManagerTest OsAgnosticMemoryManagerUsingWddmTest;
