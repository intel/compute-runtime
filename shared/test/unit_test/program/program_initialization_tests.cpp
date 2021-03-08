/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/program/program_initialization.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test_checks_shared.h"
#include "shared/test/unit_test/compiler_interface/linker_mock.h"

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
    MockSVMAllocsManager svmAllocsManager(device.getMemoryManager(), false);
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
    MockSVMAllocsManager svmAllocsManager(&memoryManager, false);
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
    MockSVMAllocsManager mockSvmAllocsManager(clDevice.getMemoryManager(), false);
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
    MockSVMAllocsManager svmAllocsManager(&memoryManager, false);
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

TEST(AllocateGlobalSurfaceTest, GivenAllocationInLocalMemoryWhichRequiresBlitterWhenAllocatingNonSvmAllocationThenBlitterIsUsed) {
    REQUIRE_SVM_OR_SKIP(defaultHwInfo.get());
    DebugManagerStateRestore restorer;

    uint32_t blitsCounter = 0;
    uint32_t expectedBlitsCount = 0;
    auto mockBlitMemoryToAllocation = [&blitsCounter](const Device &device, GraphicsAllocation *memory, size_t offset, const void *hostPtr,
                                                      Vec3<size_t> size) -> BlitOperationResult {
        blitsCounter++;
        return BlitOperationResult::Success;
    };
    VariableBackup<BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup{
        &BlitHelperFunctions::blitMemoryToAllocation, mockBlitMemoryToAllocation};

    LocalMemoryAccessMode localMemoryAccessModes[] = {
        LocalMemoryAccessMode::Default,
        LocalMemoryAccessMode::CpuAccessAllowed,
        LocalMemoryAccessMode::CpuAccessDisallowed};

    std::vector<uint8_t> initData;
    initData.resize(64, 7U);

    for (auto localMemoryAccessMode : localMemoryAccessModes) {
        DebugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(localMemoryAccessMode));
        for (auto isLocalMemorySupported : ::testing::Bool()) {
            DebugManager.flags.EnableLocalMemory.set(isLocalMemorySupported);
            MockDevice device;
            device.getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
            MockSVMAllocsManager svmAllocsManager(device.getMemoryManager(), false);

            auto pAllocation = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), true /* constant */,
                                                      nullptr /* linker input */, initData.data());
            ASSERT_NE(nullptr, pAllocation);
            EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(pAllocation->getGpuAddress()))));
            EXPECT_EQ(GraphicsAllocation::AllocationType::CONSTANT_SURFACE, pAllocation->getAllocationType());

            if (pAllocation->isAllocatedInLocalMemoryPool() && (localMemoryAccessMode == LocalMemoryAccessMode::CpuAccessDisallowed)) {
                expectedBlitsCount++;
            }
            EXPECT_EQ(expectedBlitsCount, blitsCounter);
            device.getMemoryManager()->freeGraphicsMemory(pAllocation);
        }
    }
}
