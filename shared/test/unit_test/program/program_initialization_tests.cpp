/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/program/program_initialization.h"
#include "shared/test/common/compiler_interface/linker_mock.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/test_macros/test_checks_shared.h"

#include "gtest/gtest.h"

#include <cstdint>

using namespace NEO;

TEST(AllocateGlobalSurfaceTest, GivenSvmAllocsManagerWhenGlobalsAreNotExportedThenMemoryIsAllocatedAsNonSvmAllocation) {
    MockDevice device{};
    REQUIRE_SVM_OR_SKIP(&device);
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
    EXPECT_EQ(AllocationType::CONSTANT_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), false /* constant */, nullptr /* linker input */, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::GLOBAL_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), true /* constant */, &emptyLinkerInput, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::CONSTANT_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), false /* constant */, &emptyLinkerInput, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::GLOBAL_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);
}

TEST(AllocateGlobalSurfaceTest, GivenSvmAllocsManagerWhenGlobalsAreExportedThenMemoryIsAllocatedAsUsmDeviceAllocation) {
    MockDevice device{};
    REQUIRE_SVM_OR_SKIP(&device);
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
    ASSERT_EQ(MemoryConstants::pageSize64k, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    ASSERT_NE(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_TRUE(alloc->isMemObjectsAllocationWithWritableFlags());
    EXPECT_EQ(DEVICE_UNIFIED_MEMORY, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(alloc->getGpuAddress()))->memoryType);
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
    ASSERT_EQ(MemoryConstants::pageSize64k, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_NE(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_TRUE(alloc->isMemObjectsAllocationWithWritableFlags());
    EXPECT_EQ(DEVICE_UNIFIED_MEMORY, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(alloc->getGpuAddress()))->memoryType);
    svmAllocsManager.freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress())));
}

TEST(AllocateGlobalSurfaceTest, GivenNullSvmAllocsManagerWhenGlobalsAreExportedThenMemoryIsAllocatedAsNonSvmAllocation) {
    MockDevice device{};
    WhiteBox<LinkerInput> linkerInputExportGlobalVariables;
    WhiteBox<LinkerInput> linkerInputExportGlobalConstants;
    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;
    linkerInputExportGlobalConstants.traits.exportsGlobalConstants = true;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    GraphicsAllocation *alloc = nullptr;

    alloc = allocateGlobalsSurface(nullptr, device, initData.size(), true /* constant */, &linkerInputExportGlobalConstants, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(AllocationType::CONSTANT_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(nullptr, device, initData.size(), true /* constant */, &linkerInputExportGlobalVariables, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(AllocationType::CONSTANT_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(nullptr, device, initData.size(), false /* constant */, &linkerInputExportGlobalConstants, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(AllocationType::GLOBAL_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(nullptr, device, initData.size(), false /* constant */, &linkerInputExportGlobalVariables, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(initData.size(), alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(AllocationType::GLOBAL_SURFACE, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);
}

TEST(AllocateGlobalSurfaceTest, WhenGlobalsAreNotExportedAndAllocationFailsThenGracefullyReturnsNullptr) {
    MockDevice device{};
    auto memoryManager = std::make_unique<MockMemoryManager>(*device.getExecutionEnvironment());
    memoryManager->failInAllocateWithSizeAndAlignment = true;
    device.injectMemoryManager(memoryManager.release());
    MockSVMAllocsManager mockSvmAllocsManager(device.getMemoryManager(), false);
    WhiteBox<LinkerInput> emptyLinkerInput;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    GraphicsAllocation *alloc = nullptr;

    alloc = allocateGlobalsSurface(&mockSvmAllocsManager, device, initData.size(), true /* constant */, nullptr /* linker input */, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(&mockSvmAllocsManager, device, initData.size(), false /* constant */, nullptr /* linker input */, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(&mockSvmAllocsManager, device, initData.size(), true /* constant */, &emptyLinkerInput, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(&mockSvmAllocsManager, device, initData.size(), false /* constant */, &emptyLinkerInput, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(nullptr /* svmAllocsManager */, device, initData.size(), true /* constant */, nullptr /* linker input */, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(nullptr /* svmAllocsManager */, device, initData.size(), false /* constant */, nullptr /* linker input */, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(nullptr /* svmAllocsManager */, device, initData.size(), true /* constant */, &emptyLinkerInput, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(nullptr /* svmAllocsManager */, device, initData.size(), false /* constant */, &emptyLinkerInput, initData.data());
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
            EXPECT_EQ(AllocationType::CONSTANT_SURFACE, pAllocation->getAllocationType());

            if (pAllocation->isAllocatedInLocalMemoryPool() && (localMemoryAccessMode == LocalMemoryAccessMode::CpuAccessDisallowed)) {
                expectedBlitsCount++;
            }
            EXPECT_EQ(expectedBlitsCount, blitsCounter);
            device.getMemoryManager()->freeGraphicsMemory(pAllocation);
        }
    }
}
