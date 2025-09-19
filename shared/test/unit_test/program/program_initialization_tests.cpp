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
#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/source/program/program_initialization.h"
#include "shared/test/common/compiler_interface/linker_mock.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"

#include "gtest/gtest.h"

#include <cstdint>

using namespace NEO;

TEST(AllocateGlobalSurfaceTest, GivenSvmAllocsManagerWhenGlobalsAreNotExportedThenMemoryIsAllocatedAsNonSvmAllocation) {
    MockDevice device{};
    device.injectMemoryManager(new MockMemoryManager());
    MockSVMAllocsManager svmAllocsManager(device.getMemoryManager());
    WhiteBox<LinkerInput> emptyLinkerInput;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    std::unique_ptr<SharedPoolAllocation> globalSurface;
    GraphicsAllocation *alloc{nullptr};

    size_t alignmentSize = alignUp(initData.size(), MemoryConstants::pageSize);
    globalSurface.reset(allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, true /* constant */, nullptr /* linker input */, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(alignmentSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::constantSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    globalSurface.reset(allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, false /* constant */, nullptr /* linker input */, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(alignmentSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::globalSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    globalSurface.reset(allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, true /* constant */, &emptyLinkerInput, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(alignmentSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::constantSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    globalSurface.reset(allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, false /* constant */, &emptyLinkerInput, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(alignmentSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::globalSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);
}

TEST(AllocateGlobalSurfaceTest, GivenSvmAllocsManagerWhenGlobalsAreExportedThenMemoryIsAllocatedAsUsmDeviceAllocation) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceLocalMemoryAccessMode.set(0);
    MockDevice device{};
    device.injectMemoryManager(new MockMemoryManager());
    MockSVMAllocsManager svmAllocsManager(device.getMemoryManager());
    WhiteBox<LinkerInput> linkerInputExportGlobalVariables;
    WhiteBox<LinkerInput> linkerInputExportGlobalConstants;
    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;
    linkerInputExportGlobalConstants.traits.exportsGlobalConstants = true;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    std::unique_ptr<SharedPoolAllocation> globalSurface;
    GraphicsAllocation *alloc{nullptr};
    size_t expectedAlignedSize = alignUp(initData.size(), MemoryConstants::pageSize);

    globalSurface.reset(allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, true /* constant */, &linkerInputExportGlobalConstants, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
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

    globalSurface.reset(allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, true /* constant */, &linkerInputExportGlobalVariables, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(expectedAlignedSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::constantSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    globalSurface.reset(allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalConstants, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(expectedAlignedSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::globalSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    globalSurface.reset(allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalVariables, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
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
    std::unique_ptr<SharedPoolAllocation> globalSurface;
    GraphicsAllocation *alloc{nullptr};
    size_t expectedAlignedSize = alignUp(initData.size(), MemoryConstants::pageSize);

    globalSurface.reset(allocateGlobalsSurface(nullptr, device, initData.size(), 0u, true /* constant */, &linkerInputExportGlobalConstants, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(expectedAlignedSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(AllocationType::constantSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    globalSurface.reset(allocateGlobalsSurface(nullptr, device, initData.size(), 0u, true /* constant */, &linkerInputExportGlobalVariables, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(expectedAlignedSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(AllocationType::constantSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    globalSurface.reset(allocateGlobalsSurface(nullptr, device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalConstants, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(expectedAlignedSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(AllocationType::globalSurface, alloc->getAllocationType());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    globalSurface.reset(allocateGlobalsSurface(nullptr, device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalVariables, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
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
    std::unique_ptr<SharedPoolAllocation> globalSurface;

    globalSurface.reset(allocateGlobalsSurface(&mockSvmAllocsManager, device, initData.size(), 0u, true /* constant */, nullptr /* linker input */, initData.data()));
    EXPECT_EQ(nullptr, globalSurface);

    globalSurface.reset(allocateGlobalsSurface(&mockSvmAllocsManager, device, initData.size(), 0u, false /* constant */, nullptr /* linker input */, initData.data()));
    EXPECT_EQ(nullptr, globalSurface);

    globalSurface.reset(allocateGlobalsSurface(&mockSvmAllocsManager, device, initData.size(), 0u, true /* constant */, &emptyLinkerInput, initData.data()));
    EXPECT_EQ(nullptr, globalSurface);

    globalSurface.reset(allocateGlobalsSurface(&mockSvmAllocsManager, device, initData.size(), 0u, false /* constant */, &emptyLinkerInput, initData.data()));
    EXPECT_EQ(nullptr, globalSurface);

    globalSurface.reset(allocateGlobalsSurface(nullptr /* svmAllocsManager */, device, initData.size(), 0u, true /* constant */, nullptr /* linker input */, initData.data()));
    EXPECT_EQ(nullptr, globalSurface);

    globalSurface.reset(allocateGlobalsSurface(nullptr /* svmAllocsManager */, device, initData.size(), 0u, false /* constant */, nullptr /* linker input */, initData.data()));
    EXPECT_EQ(nullptr, globalSurface);

    globalSurface.reset(allocateGlobalsSurface(nullptr /* svmAllocsManager */, device, initData.size(), 0u, true /* constant */, &emptyLinkerInput, initData.data()));
    EXPECT_EQ(nullptr, globalSurface);

    globalSurface.reset(allocateGlobalsSurface(nullptr /* svmAllocsManager */, device, initData.size(), 0u, false /* constant */, &emptyLinkerInput, initData.data()));
    EXPECT_EQ(nullptr, globalSurface);
}

TEST(AllocateGlobalSurfaceTest, GivenAllocationInLocalMemoryWhichRequiresBlitterWhenAllocatingNonSvmAllocationThenBlitterIsUsed) {
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
            std::unique_ptr<SharedPoolAllocation> globalSurface = std::unique_ptr<SharedPoolAllocation>(allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, true /* constant */, nullptr /* linker input */, initData.data()));

            ASSERT_NE(nullptr, globalSurface);
            GraphicsAllocation *pAllocation{globalSurface->getGraphicsAllocation()};
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
    std::fill(initData.begin() + 32, initData.end(), 16u); // this data should not be transferred
    std::unique_ptr<SharedPoolAllocation> globalSurface;
    GraphicsAllocation *alloc{nullptr};
    size_t zeroInitSize = 32u;
    size_t expectedAlignedSize = alignUp(initData.size(), MemoryConstants::pageSize);
    globalSurface.reset(allocateGlobalsSurface(nullptr, device, initData.size(), zeroInitSize, true, &emptyLinkerInput, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
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

    std::unique_ptr<SharedPoolAllocation> globalSurface;
    GraphicsAllocation *alloc{nullptr};
    globalSurface.reset(allocateGlobalsSurface(nullptr, device, totalSize, zeroInitSize, true, nullptr, nullptr));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
    ASSERT_NE(nullptr, alloc);
    EXPECT_EQ(0u, static_cast<MockMemoryManager *>(device.getMemoryManager())->copyMemoryToAllocationBanksCalled);

    device.getMemoryManager()->freeGraphicsMemory(alloc);
}

struct AllocateGlobalSurfaceWithUsmPoolTest : public ::testing::Test {
    void SetUp() override {
        device.injectMemoryManager(new MockMemoryManager());
        device.resetUsmConstantSurfaceAllocPool(new UsmMemAllocPool);
        device.resetUsmGlobalSurfaceAllocPool(new UsmMemAllocPool);

        mockProductHelper = new MockProductHelper;
        device.getRootDeviceEnvironmentRef().productHelper.reset(mockProductHelper);

        svmAllocsManager = std::make_unique<MockSVMAllocsManager>(device.getMemoryManager());
    }

    MockProductHelper *mockProductHelper{nullptr};
    std::unique_ptr<MockSVMAllocsManager> svmAllocsManager;
    WhiteBox<LinkerInput> linkerInputExportGlobalVariables;
    WhiteBox<LinkerInput> linkerInputExportGlobalConstants;
    MockDevice device{};

    DebugManagerStateRestore restore;
};

TEST_F(AllocateGlobalSurfaceWithUsmPoolTest, GivenUsmAllocPoolAnd2MBLocalMemAlignmentDisabledThenGlobalSurfaceAllocationNotTakenFromUsmPool) {
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = false;

    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;
    linkerInputExportGlobalConstants.traits.exportsGlobalConstants = true;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    std::unique_ptr<SharedPoolAllocation> globalSurface;

    globalSurface.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, true /* constant */, &linkerInputExportGlobalConstants, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    EXPECT_FALSE(device.getUsmConstantSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(globalSurface->getGpuAddress())));
    EXPECT_EQ(globalSurface->getGraphicsAllocation()->getUnderlyingBufferSize(), globalSurface->getSize());
    EXPECT_EQ(0u, globalSurface->getOffset());
    svmAllocsManager->freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(globalSurface->getGpuAddress())));

    globalSurface.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalVariables, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    EXPECT_FALSE(device.getUsmGlobalSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(globalSurface->getGpuAddress())));
    EXPECT_EQ(globalSurface->getGraphicsAllocation()->getUnderlyingBufferSize(), globalSurface->getSize());
    EXPECT_EQ(0u, globalSurface->getOffset());
    svmAllocsManager->freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(globalSurface->getGpuAddress())));
}

TEST_F(AllocateGlobalSurfaceWithUsmPoolTest, GivenUsmAllocPoolAnd2MBLocalMemAlignmentEnabledThenGlobalSurfaceAllocationTakenFromUsmPool) {
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;
    linkerInputExportGlobalConstants.traits.exportsGlobalConstants = true;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);

    {
        std::unique_ptr<SharedPoolAllocation> constantSurface1;
        std::unique_ptr<SharedPoolAllocation> constantSurface2;

        constantSurface1.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, true /* constant */, &linkerInputExportGlobalConstants, initData.data()));
        ASSERT_NE(nullptr, constantSurface1);
        EXPECT_TRUE(device.getUsmConstantSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(constantSurface1->getGpuAddress())));
        EXPECT_NE(constantSurface1->getGraphicsAllocation()->getUnderlyingBufferSize(), constantSurface1->getSize());
        EXPECT_EQ(0, memcmp(constantSurface1->getUnderlyingBuffer(), initData.data(), initData.size()));
        EXPECT_TRUE(constantSurface1->getGraphicsAllocation()->isMemObjectsAllocationWithWritableFlags());
        EXPECT_EQ(AllocationType::constantSurface, constantSurface1->getGraphicsAllocation()->getAllocationType());

        constantSurface2.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, true /* constant */, &linkerInputExportGlobalConstants, initData.data()));
        ASSERT_NE(nullptr, constantSurface2);
        EXPECT_TRUE(device.getUsmConstantSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(constantSurface2->getGpuAddress())));
        EXPECT_NE(constantSurface2->getGraphicsAllocation()->getUnderlyingBufferSize(), constantSurface2->getSize());
        EXPECT_EQ(0, memcmp(constantSurface2->getUnderlyingBuffer(), initData.data(), initData.size()));
        EXPECT_TRUE(constantSurface2->getGraphicsAllocation()->isMemObjectsAllocationWithWritableFlags());
        EXPECT_EQ(AllocationType::constantSurface, constantSurface2->getGraphicsAllocation()->getAllocationType());

        EXPECT_EQ(constantSurface1->getGraphicsAllocation(), constantSurface2->getGraphicsAllocation());
        EXPECT_EQ(constantSurface1->getSize(), constantSurface2->getSize());
        EXPECT_NE(constantSurface1->getGpuAddress(), constantSurface2->getGpuAddress());
        EXPECT_NE(constantSurface1->getOffset(), constantSurface2->getOffset());
    }

    {
        std::unique_ptr<SharedPoolAllocation> globalSurface1;
        std::unique_ptr<SharedPoolAllocation> globalSurface2;

        globalSurface1.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalVariables, initData.data()));
        ASSERT_NE(nullptr, globalSurface1);
        EXPECT_TRUE(device.getUsmGlobalSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(globalSurface1->getGpuAddress())));
        EXPECT_NE(globalSurface1->getGraphicsAllocation()->getUnderlyingBufferSize(), globalSurface1->getSize());
        EXPECT_EQ(0, memcmp(globalSurface1->getUnderlyingBuffer(), initData.data(), initData.size()));
        EXPECT_TRUE(globalSurface1->getGraphicsAllocation()->isMemObjectsAllocationWithWritableFlags());
        EXPECT_EQ(AllocationType::globalSurface, globalSurface1->getGraphicsAllocation()->getAllocationType());

        globalSurface2.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalVariables, initData.data()));
        ASSERT_NE(nullptr, globalSurface2);
        EXPECT_TRUE(device.getUsmGlobalSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(globalSurface2->getGpuAddress())));
        EXPECT_NE(globalSurface2->getGraphicsAllocation()->getUnderlyingBufferSize(), globalSurface2->getSize());
        EXPECT_EQ(0, memcmp(globalSurface2->getUnderlyingBuffer(), initData.data(), initData.size()));
        EXPECT_TRUE(globalSurface2->getGraphicsAllocation()->isMemObjectsAllocationWithWritableFlags());
        EXPECT_EQ(AllocationType::globalSurface, globalSurface2->getGraphicsAllocation()->getAllocationType());

        EXPECT_EQ(globalSurface1->getGraphicsAllocation(), globalSurface2->getGraphicsAllocation());
        EXPECT_EQ(globalSurface1->getSize(), globalSurface2->getSize());
        EXPECT_NE(globalSurface1->getGpuAddress(), globalSurface2->getGpuAddress());
        EXPECT_NE(globalSurface1->getOffset(), globalSurface2->getOffset());
    }
}

TEST_F(AllocateGlobalSurfaceWithUsmPoolTest, Given2MBLocalMemAlignmentEnabledButUsmPoolInitializeFailsThenDoNotUseUsmPool) {
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

    auto usmConstantSurfaceAllocPool = new MockUsmMemAllocPool;
    auto usmGlobalSurfaceAllocPool = new MockUsmMemAllocPool;

    device.resetUsmConstantSurfaceAllocPool(usmConstantSurfaceAllocPool);
    device.resetUsmGlobalSurfaceAllocPool(usmGlobalSurfaceAllocPool);

    usmConstantSurfaceAllocPool->callBaseInitialize = false;
    usmConstantSurfaceAllocPool->initializeResult = false;

    usmGlobalSurfaceAllocPool->callBaseInitialize = false;
    usmGlobalSurfaceAllocPool->initializeResult = false;

    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;
    linkerInputExportGlobalConstants.traits.exportsGlobalConstants = true;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);

    std::unique_ptr<SharedPoolAllocation> globalSurface;

    globalSurface.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, true /* constant */, &linkerInputExportGlobalConstants, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    EXPECT_FALSE(device.getUsmConstantSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(globalSurface->getGpuAddress())));
    svmAllocsManager->freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(globalSurface->getGpuAddress())));

    globalSurface.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalVariables, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    EXPECT_FALSE(device.getUsmGlobalSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(globalSurface->getGpuAddress())));
    svmAllocsManager->freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(globalSurface->getGpuAddress())));
}

TEST_F(AllocateGlobalSurfaceWithUsmPoolTest, Given2MBLocalMemAlignmentEnabledButAllocatingFromUsmPoolFailsThenDoNotUseUsmPool) {
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;

    auto usmConstantSurfaceAllocPool = new MockUsmMemAllocPool;
    auto usmGlobalSurfaceAllocPool = new MockUsmMemAllocPool;

    device.resetUsmConstantSurfaceAllocPool(usmConstantSurfaceAllocPool);
    device.resetUsmGlobalSurfaceAllocPool(usmGlobalSurfaceAllocPool);

    usmConstantSurfaceAllocPool->callBaseCreateUnifiedMemoryAllocation = false;
    usmConstantSurfaceAllocPool->createUnifiedMemoryAllocationResult = nullptr;

    usmGlobalSurfaceAllocPool->callBaseCreateUnifiedMemoryAllocation = false;
    usmGlobalSurfaceAllocPool->createUnifiedMemoryAllocationResult = nullptr;

    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;
    linkerInputExportGlobalConstants.traits.exportsGlobalConstants = true;
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);

    std::unique_ptr<SharedPoolAllocation> globalSurface;

    globalSurface.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, true /* constant */, &linkerInputExportGlobalConstants, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    EXPECT_FALSE(device.getUsmConstantSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(globalSurface->getGpuAddress())));
    svmAllocsManager->freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(globalSurface->getGpuAddress())));

    globalSurface.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalVariables, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    EXPECT_FALSE(device.getUsmGlobalSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(globalSurface->getGpuAddress())));
    svmAllocsManager->freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(globalSurface->getGpuAddress())));
}