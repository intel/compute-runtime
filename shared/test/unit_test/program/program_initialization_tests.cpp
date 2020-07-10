/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/program_initialization.h"
#include "shared/test/unit_test/compiler_interface/linker_mock.h"
#include "shared/test/unit_test/mocks/mock_device.h"
#include "shared/test/unit_test/test_macros/test_checks_shared.h"

#include "opencl/test/unit_test/mocks/mock_cl_device.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_svm_manager.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <cstdint>

using namespace NEO;

TEST(AllocateGlobalSurfaceTest, GivenSvmAllocsManagerWhenGlobalsAreNotExportedThenMemoryIsAllocatedAsNonSvmAllocation) {
    auto &device = *(new MockDevice);
    REQUIRE_SVM_OR_SKIP(&device);
    MockClDevice clDevice{&device};
    MockSVMAllocsManager svmAllocsManager(device.getMemoryManager());
    WhiteBox<LinkerInput> emptyLinkerInput;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    GraphicsAllocation *alloc = nullptr;

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), true /* constant */, nullptr /* linker input */, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(GraphicsAllocation::AllocationType::CONSTANT_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), false /* constant */, nullptr /* linker input */, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(GraphicsAllocation::AllocationType::GLOBAL_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), true /* constant */, &emptyLinkerInput, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(GraphicsAllocation::AllocationType::CONSTANT_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), false /* constant */, &emptyLinkerInput, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(GraphicsAllocation::AllocationType::GLOBAL_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);
}

TEST(AllocateGlobalSurfaceTest, GivenSvmAllocsManagerWhenGlobalsAreExportedThenMemoryIsAllocatedAsSvmAllocation) {
    auto &device = *(new MockDevice);
    REQUIRE_SVM_OR_SKIP(&device);
    MockClDevice clDevice{&device};
    MockMemoryManager memoryManager;
    MockSVMAllocsManager svmAllocsManager(&memoryManager);
    WhiteBox<LinkerInput> linkerInputExportGlobalVariables;
    WhiteBox<LinkerInput> linkerInputExportGlobalConstants;
    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;
    linkerInputExportGlobalConstants.traits.exportsGlobalConstants = true;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    GraphicsAllocation *alloc = nullptr;

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), true /* constant */, &linkerInputExportGlobalConstants, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    ASSERT_NE(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_FALSE(alloc->isMemObjectsAllocationWithWritableFlags());
    svmAllocsManager.freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress())));

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), true /* constant */, &linkerInputExportGlobalVariables, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), false /* constant */, &linkerInputExportGlobalConstants, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), false /* constant */, &linkerInputExportGlobalVariables, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_NE(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_TRUE(alloc->isMemObjectsAllocationWithWritableFlags());
    svmAllocsManager.freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress())));
}

TEST(AllocateGlobalSurfaceTest, GivenNullSvmAllocsManagerWhenGlobalsAreExportedThenMemoryIsAllocatedAsNonSvmAllocation) {
    auto pDevice = new MockDevice{};
    MockClDevice clDevice{pDevice};
    WhiteBox<LinkerInput> linkerInputExportGlobalVariables;
    WhiteBox<LinkerInput> linkerInputExportGlobalConstants;
    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;
    linkerInputExportGlobalConstants.traits.exportsGlobalConstants = true;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    GraphicsAllocation *alloc = nullptr;

    alloc = allocateGlobalsSurface(nullptr, *pDevice, initData.size(), true /* constant */, &linkerInputExportGlobalConstants, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(GraphicsAllocation::AllocationType::CONSTANT_SURFACE, alloc->getAllocationType());
    pDevice->getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(nullptr, *pDevice, initData.size(), true /* constant */, &linkerInputExportGlobalVariables, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(GraphicsAllocation::AllocationType::CONSTANT_SURFACE, alloc->getAllocationType());
    pDevice->getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(nullptr, *pDevice, initData.size(), false /* constant */, &linkerInputExportGlobalConstants, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(GraphicsAllocation::AllocationType::GLOBAL_SURFACE, alloc->getAllocationType());
    pDevice->getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(nullptr, *pDevice, initData.size(), false /* constant */, &linkerInputExportGlobalVariables, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(GraphicsAllocation::AllocationType::GLOBAL_SURFACE, alloc->getAllocationType());
    pDevice->getMemoryManager()->freeGraphicsMemory(alloc);
}

TEST(AllocateGlobalSurfaceTest, WhenGlobalsAreNotExportedAndAllocationFailsThenGracefullyReturnsNullptr) {
    auto pDevice = new MockDevice{};
    MockClDevice clDevice{pDevice};
    auto memoryManager = std::make_unique<MockMemoryManager>(*clDevice.getExecutionEnvironment());
    memoryManager->failInAllocateWithSizeAndAlignment = true;
    clDevice.injectMemoryManager(memoryManager.release());
    MockSVMAllocsManager mockSvmAllocsManager(clDevice.getMemoryManager());
    WhiteBox<LinkerInput> emptyLinkerInput;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    GraphicsAllocation *alloc = nullptr;

    alloc = allocateGlobalsSurface(&mockSvmAllocsManager, *pDevice, initData.size(), true /* constant */, nullptr /* linker input */, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(&mockSvmAllocsManager, *pDevice, initData.size(), false /* constant */, nullptr /* linker input */, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(&mockSvmAllocsManager, *pDevice, initData.size(), true /* constant */, &emptyLinkerInput, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(&mockSvmAllocsManager, *pDevice, initData.size(), false /* constant */, &emptyLinkerInput, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(nullptr /* svmAllocsManager */, *pDevice, initData.size(), true /* constant */, nullptr /* linker input */, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(nullptr /* svmAllocsManager */, *pDevice, initData.size(), false /* constant */, nullptr /* linker input */, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(nullptr /* svmAllocsManager */, *pDevice, initData.size(), true /* constant */, &emptyLinkerInput, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(nullptr /* svmAllocsManager */, *pDevice, initData.size(), false /* constant */, &emptyLinkerInput, initData.data());
    EXPECT_EQ(nullptr, alloc);
}

TEST(AllocateGlobalSurfaceTest, WhenGlobalsAreExportedAndAllocationFailsThenGracefullyReturnsNullptr) {
    auto pDevice = new MockDevice{};
    MockClDevice clDevice{pDevice};
    MockMemoryManager memoryManager{*clDevice.getExecutionEnvironment()};
    MockSVMAllocsManager svmAllocsManager(&memoryManager);
    memoryManager.failInAllocateWithSizeAndAlignment = true;
    WhiteBox<LinkerInput> linkerInputExportGlobalVariables;
    WhiteBox<LinkerInput> linkerInputExportGlobalConstants;
    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;
    linkerInputExportGlobalConstants.traits.exportsGlobalConstants = true;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    GraphicsAllocation *alloc = nullptr;

    alloc = allocateGlobalsSurface(&svmAllocsManager, *pDevice, initData.size(), true /* constant */, &linkerInputExportGlobalConstants, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, *pDevice, initData.size(), false /* constant */, &linkerInputExportGlobalVariables, initData.data());
    EXPECT_EQ(nullptr, alloc);
}
