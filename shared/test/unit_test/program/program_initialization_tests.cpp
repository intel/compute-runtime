/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/blit_helper.h"
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
    device.injectMemoryManager(new MockMemoryManager());
    MockSVMAllocsManager svmAllocsManager(device.getMemoryManager());
    WhiteBox<LinkerInput> emptyLinkerInput;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    GraphicsAllocation *alloc = nullptr;

    size_t aligmentSize = alignUp(initData.size(), MemoryConstants::pageSize);
    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, true /* constant */, nullptr /* linker input */, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(aligmentSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::constantSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, false /* constant */, nullptr /* linker input */, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(aligmentSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::globalSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, true /* constant */, &emptyLinkerInput, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(aligmentSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::constantSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, false /* constant */, &emptyLinkerInput, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(aligmentSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::globalSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);
}

TEST(AllocateGlobalSurfaceTest, GivenSvmAllocsManagerWhenGlobalsAreExportedThenMemoryIsAllocatedAsUsmDeviceAllocation) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceLocalMemoryAccessMode.set(0);
    MockDevice device{};
    REQUIRE_SVM_OR_SKIP(&device);
    device.injectMemoryManager(new MockMemoryManager());
    MockSVMAllocsManager svmAllocsManager(device.getMemoryManager());
    WhiteBox<LinkerInput> linkerInputExportGlobalVariables;
    WhiteBox<LinkerInput> linkerInputExportGlobalConstants;
    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;
    linkerInputExportGlobalConstants.traits.exportsGlobalConstants = true;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    GraphicsAllocation *alloc = nullptr;
    size_t expectedAlignedSize = alignUp(initData.size(), MemoryConstants::pageSize);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, true /* constant */, &linkerInputExportGlobalConstants, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(MemoryConstants::pageSize64k, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    ASSERT_NE(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_TRUE(alloc->isMemObjectsAllocationWithWritableFlags());
    auto svmData = svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(alloc->getGpuAddress()));
    EXPECT_EQ(InternalMemoryType::deviceUnifiedMemory, svmData->memoryType);
    EXPECT_TRUE(svmData->isInternalAllocation);
    EXPECT_EQ(AllocationType::constantSurface, alloc->getAllocationType());
    EXPECT_FALSE(alloc->getDefaultGmm()->resourceParams.Flags.Info.NotLockable);
    EXPECT_TRUE(svmAllocsManager.requestedZeroedOutAllocation);
    svmAllocsManager.freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress())));

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, true /* constant */, &linkerInputExportGlobalVariables, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(expectedAlignedSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::constantSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalConstants, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(expectedAlignedSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::globalSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalVariables, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(MemoryConstants::pageSize64k, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_NE(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_TRUE(alloc->isMemObjectsAllocationWithWritableFlags());
    EXPECT_EQ(InternalMemoryType::deviceUnifiedMemory, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(alloc->getGpuAddress()))->memoryType);
    EXPECT_EQ(AllocationType::globalSurface, alloc->getAllocationType());
    EXPECT_FALSE(alloc->getDefaultGmm()->resourceParams.Flags.Info.NotLockable);
    svmAllocsManager.freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress())));
}

TEST(AllocateGlobalSurfaceTest, GivenNullSvmAllocsManagerWhenGlobalsAreExportedThenMemoryIsAllocatedAsNonSvmAllocation) {
    MockDevice device{};
    device.injectMemoryManager(new MockMemoryManager());
    WhiteBox<LinkerInput> linkerInputExportGlobalVariables;
    WhiteBox<LinkerInput> linkerInputExportGlobalConstants;
    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;
    linkerInputExportGlobalConstants.traits.exportsGlobalConstants = true;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    GraphicsAllocation *alloc = nullptr;
    size_t expectedAlignedSize = alignUp(initData.size(), MemoryConstants::pageSize);

    alloc = allocateGlobalsSurface(nullptr, device, initData.size(), 0u, true /* constant */, &linkerInputExportGlobalConstants, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(expectedAlignedSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(AllocationType::constantSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(nullptr, device, initData.size(), 0u, true /* constant */, &linkerInputExportGlobalVariables, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(expectedAlignedSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(AllocationType::constantSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(nullptr, device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalConstants, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(expectedAlignedSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(AllocationType::globalSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    alloc = allocateGlobalsSurface(nullptr, device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalVariables, initData.data());
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(expectedAlignedSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(AllocationType::globalSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);
}

TEST(AllocateGlobalSurfaceTest, WhenGlobalsAreNotExportedAndAllocationFailsThenGracefullyReturnsNullptr) {
    MockDevice device{};
    auto memoryManager = std::make_unique<MockMemoryManager>(*device.getExecutionEnvironment());
    memoryManager->failInAllocateWithSizeAndAlignment = true;
    device.injectMemoryManager(memoryManager.release());
    MockSVMAllocsManager mockSvmAllocsManager(device.getMemoryManager());
    WhiteBox<LinkerInput> emptyLinkerInput;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    GraphicsAllocation *alloc = nullptr;

    alloc = allocateGlobalsSurface(&mockSvmAllocsManager, device, initData.size(), 0u, true /* constant */, nullptr /* linker input */, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(&mockSvmAllocsManager, device, initData.size(), 0u, false /* constant */, nullptr /* linker input */, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(&mockSvmAllocsManager, device, initData.size(), 0u, true /* constant */, &emptyLinkerInput, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(&mockSvmAllocsManager, device, initData.size(), 0u, false /* constant */, &emptyLinkerInput, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(nullptr /* svmAllocsManager */, device, initData.size(), 0u, true /* constant */, nullptr /* linker input */, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(nullptr /* svmAllocsManager */, device, initData.size(), 0u, false /* constant */, nullptr /* linker input */, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(nullptr /* svmAllocsManager */, device, initData.size(), 0u, true /* constant */, &emptyLinkerInput, initData.data());
    EXPECT_EQ(nullptr, alloc);

    alloc = allocateGlobalsSurface(nullptr /* svmAllocsManager */, device, initData.size(), 0u, false /* constant */, &emptyLinkerInput, initData.data());
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
        return BlitOperationResult::success;
    };
    VariableBackup<BlitHelperFunctions::BlitMemoryToAllocationFunc> blitMemoryToAllocationFuncBackup{
        &BlitHelperFunctions::blitMemoryToAllocation, mockBlitMemoryToAllocation};

    LocalMemoryAccessMode localMemoryAccessModes[] = {
        LocalMemoryAccessMode::defaultMode,
        LocalMemoryAccessMode::cpuAccessAllowed,
        LocalMemoryAccessMode::cpuAccessDisallowed};

    std::vector<uint8_t> initData;
    initData.resize(64, 7U);

    for (auto localMemoryAccessMode : localMemoryAccessModes) {
        debugManager.flags.ForceLocalMemoryAccessMode.set(static_cast<int32_t>(localMemoryAccessMode));
        for (auto isLocalMemorySupported : ::testing::Bool()) {
            debugManager.flags.EnableLocalMemory.set(isLocalMemorySupported);
            MockDevice device;
            device.getExecutionEnvironment()->rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.blitterOperationsSupported = true;
            MockSVMAllocsManager svmAllocsManager(device.getMemoryManager());

            auto pAllocation = allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, true /* constant */,
                                                      nullptr /* linker input */, initData.data());
            ASSERT_NE(nullptr, pAllocation);
            EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(pAllocation->getGpuAddress()))));
            EXPECT_EQ(AllocationType::constantSurface, pAllocation->getAllocationType());

            if (pAllocation->isAllocatedInLocalMemoryPool() && (localMemoryAccessMode == LocalMemoryAccessMode::cpuAccessDisallowed)) {
                expectedBlitsCount++;
            }
            EXPECT_EQ(expectedBlitsCount, blitsCounter);
            device.getMemoryManager()->freeGraphicsMemory(pAllocation);
        }
    }
}

TEST(AllocateGlobalSurfaceTest, whenAllocatingGlobalSurfaceWithNonZeroZeroInitSizeThenTransferOnlyInitDataToAllocation) {
    MockDevice device{};
    WhiteBox<LinkerInput> emptyLinkerInput;
    device.injectMemoryManager(new MockMemoryManager());
    emptyLinkerInput.traits.exportsGlobalConstants = true;
    std::vector<uint8_t> initData;
    initData.resize(64, 7u);
    std::fill(initData.begin() + 32, initData.end(), 16u); // this data should not be transfered
    GraphicsAllocation *alloc = nullptr;
    size_t zeroInitSize = 32u;
    size_t expectedAlignedSize = alignUp(initData.size(), MemoryConstants::pageSize);
    alloc = allocateGlobalsSurface(nullptr, device, initData.size(), zeroInitSize, true, &emptyLinkerInput, initData.data());
    ASSERT_NE(nullptr, alloc);
    EXPECT_EQ(expectedAlignedSize, alloc->getUnderlyingBufferSize());

    auto dataPtr = reinterpret_cast<uint8_t *>(alloc->getUnderlyingBuffer());
    EXPECT_EQ(0, memcmp(dataPtr, initData.data(), 32u));
    EXPECT_NE(0, memcmp(dataPtr + 32, initData.data() + 32, 32u));
    device.getMemoryManager()->freeGraphicsMemory(alloc);
}

TEST(AllocateGlobalSurfaceTest, whenAllocatingGlobalSurfaceWithZeroInitSizeGreaterThanZeroAndInitDataSizeSetToZeroThenDoNotTransferMemoryToAllocation) {
    MockDevice device{};
    auto memoryManager = std::make_unique<MockMemoryManager>(*device.getExecutionEnvironment());
    device.injectMemoryManager(memoryManager.release());
    ASSERT_EQ(0u, static_cast<MockMemoryManager *>(device.getMemoryManager())->copyMemoryToAllocationBanksCalled);
    size_t totalSize = 64u, zeroInitSize = 64u;

    GraphicsAllocation *alloc = nullptr;
    alloc = allocateGlobalsSurface(nullptr, device, totalSize, zeroInitSize, true, nullptr, nullptr);
    ASSERT_NE(nullptr, alloc);
    EXPECT_EQ(0u, static_cast<MockMemoryManager *>(device.getMemoryManager())->copyMemoryToAllocationBanksCalled);

    device.getMemoryManager()->freeGraphicsMemory(alloc);
}
