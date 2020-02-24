/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/program_initialization.h"
#include "shared/test/unit_test/compiler_interface/linker_mock.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_memory_manager.h"
#include "opencl/test/unit_test/mocks/mock_svm_manager.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include <cstdint>

TEST(AllocateGlobalSurfaceTest, GivenSvmAllocsManagerWhenGlobalsAreNotExportedThenMemoryIsAllocatedAsNonSvmAllocation) {
    MockDevice device;
    if (0 == device.getDeviceInfo().svmCapabilities) {
        return;
    }
    MockSVMAllocsManager svmAllocsManager(device.getMemoryManager());
    WhiteBox<LinkerInput> emptyLinkerInput;
    LinkerInput *linkerInput;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    bool constant = true;
    GraphicsAllocation *alloc = nullptr;

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), constant = true, linkerInput = nullptr, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(GraphicsAllocation::AllocationType::CONSTANT_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), constant = false, linkerInput = nullptr, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(GraphicsAllocation::AllocationType::GLOBAL_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), constant = true, linkerInput = &emptyLinkerInput, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(GraphicsAllocation::AllocationType::CONSTANT_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), constant = false, linkerInput = &emptyLinkerInput, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(GraphicsAllocation::AllocationType::GLOBAL_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);
}

TEST(AllocateGlobalSurfaceTest, GivenSvmAllocsManagerWhenGlobalsAreExportedThenMemoryIsAllocatedAsSvmAllocation) {
    MockDevice device;
    if (0 == device.getDeviceInfo().svmCapabilities) {
        return;
    }
    MockMemoryManager memoryManager;
    MockSVMAllocsManager svmAllocsManager(&memoryManager);
    WhiteBox<LinkerInput> linkerInputExportGlobalVariables;
    WhiteBox<LinkerInput> linkerInputExportGlobalConstants;
    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;
    linkerInputExportGlobalConstants.traits.exportsGlobalConstants = true;
    LinkerInput *linkerInput;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    bool constant = true;
    GraphicsAllocation *alloc = nullptr;

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), constant = true, linkerInput = &linkerInputExportGlobalConstants, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    ASSERT_NE(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_FALSE(alloc->isMemObjectsAllocationWithWritableFlags());
    svmAllocsManager.freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress())));

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), constant = true, linkerInput = &linkerInputExportGlobalVariables, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), constant = false, linkerInput = &linkerInputExportGlobalConstants, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), constant = false, linkerInput = &linkerInputExportGlobalVariables, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_NE(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_TRUE(alloc->isMemObjectsAllocationWithWritableFlags());
    svmAllocsManager.freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress())));
}

TEST(AllocateGlobalSurfaceTest, GivenNullSvmAllocsManagerWhenGlobalsAreExportedThenMemoryIsAllocatedAsNonSvmAllocation) {
    MockDevice device;
    WhiteBox<LinkerInput> linkerInputExportGlobalVariables;
    WhiteBox<LinkerInput> linkerInputExportGlobalConstants;
    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;
    linkerInputExportGlobalConstants.traits.exportsGlobalConstants = true;
    LinkerInput *linkerInput;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    bool constant = true;
    GraphicsAllocation *alloc = nullptr;

    alloc = allocateGlobalsSurface(nullptr, device, initData.size(), constant = true, linkerInput = &linkerInputExportGlobalConstants, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(GraphicsAllocation::AllocationType::CONSTANT_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(nullptr, device, initData.size(), constant = true, linkerInput = &linkerInputExportGlobalVariables, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(GraphicsAllocation::AllocationType::CONSTANT_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(nullptr, device, initData.size(), constant = false, linkerInput = &linkerInputExportGlobalConstants, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(GraphicsAllocation::AllocationType::GLOBAL_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(nullptr, device, initData.size(), constant = false, linkerInput = &linkerInputExportGlobalVariables, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(GraphicsAllocation::AllocationType::GLOBAL_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);
}

TEST(AllocateGlobalSurfaceTest, WhenGlobalsAreNotExportedAndAllocationFailsThenGracefullyReturnsNullptr) {
    MockDevice device;
    auto memoryManager = std::make_unique<MockMemoryManager>();
    memoryManager->failInAllocateWithSizeAndAlignment = true;
    device.injectMemoryManager(memoryManager.release());
    MockSVMAllocsManager mockSvmAllocsManager(device.getMemoryManager());
    WhiteBox<LinkerInput> emptyLinkerInput;
    LinkerInput *linkerInput;
    SVMAllocsManager *svmAllocsManager = nullptr;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    bool constant = true;
    GraphicsAllocation *alloc = nullptr;

    alloc = allocateGlobalsSurface(svmAllocsManager = &mockSvmAllocsManager, device, initData.size(), constant = true, linkerInput = nullptr, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(svmAllocsManager = &mockSvmAllocsManager, device, initData.size(), constant = false, linkerInput = nullptr, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(svmAllocsManager = &mockSvmAllocsManager, device, initData.size(), constant = true, linkerInput = &emptyLinkerInput, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(svmAllocsManager = &mockSvmAllocsManager, device, initData.size(), constant = false, linkerInput = &emptyLinkerInput, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(svmAllocsManager = nullptr, device, initData.size(), constant = true, linkerInput = nullptr, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(svmAllocsManager = nullptr, device, initData.size(), constant = false, linkerInput = nullptr, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(svmAllocsManager = nullptr, device, initData.size(), constant = true, linkerInput = &emptyLinkerInput, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(svmAllocsManager = nullptr, device, initData.size(), constant = false, linkerInput = &emptyLinkerInput, initData.data());
    EXPECT_EQ(nullptr, alloc);
}

TEST(AllocateGlobalSurfaceTest, WhenGlobalsAreExportedAndAllocationFailsThenGracefullyReturnsNullptr) {
    MockDevice device;
    MockMemoryManager memoryManager;
    MockSVMAllocsManager svmAllocsManager(&memoryManager);
    memoryManager.failInAllocateWithSizeAndAlignment = true;
    WhiteBox<LinkerInput> linkerInputExportGlobalVariables;
    WhiteBox<LinkerInput> linkerInputExportGlobalConstants;
    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;
    linkerInputExportGlobalConstants.traits.exportsGlobalConstants = true;
    LinkerInput *linkerInput;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    bool constant = true;
    GraphicsAllocation *alloc = nullptr;

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), constant = true, linkerInput = &linkerInputExportGlobalConstants, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), constant = false, linkerInput = &linkerInputExportGlobalVariables, initData.data());
    EXPECT_EQ(nullptr, alloc);
}
