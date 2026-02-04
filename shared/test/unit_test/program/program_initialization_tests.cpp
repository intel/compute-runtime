/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/external_functions.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/blit_helper.h"
#include "shared/source/helpers/local_memory_access_modes.h"
#include "shared/source/memory_manager/unified_memory_pooling.h"
#include "shared/source/program/program_initialization.h"
#include "shared/test/common/compiler_interface/linker_mock.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_product_helper.h"
#include "shared/test/common/mocks/mock_svm_manager.h"
#include "shared/test/common/mocks/mock_usm_memory_pool.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

#include <cstdint>

using namespace NEO;

struct AllocateGlobalSurfacePoolingDisabledTest : public ::testing::Test {
    void SetUp() override {
        device.injectMemoryManager(new MockMemoryManager());
        device.resetUsmConstantSurfaceAllocPool(nullptr);
        device.resetUsmGlobalSurfaceAllocPool(nullptr);

        mockProductHelper = new MockProductHelper;
        device.getRootDeviceEnvironmentRef().productHelper.reset(mockProductHelper);

        mockProductHelper->is2MBLocalMemAlignmentEnabledResult = false;
    }

    MockProductHelper *mockProductHelper{nullptr};
    MockDevice device{};

    DebugManagerStateRestore restore;
};

TEST_F(AllocateGlobalSurfacePoolingDisabledTest, GivenSvmAllocsManagerWhenGlobalsAreNotExportedThenMemoryIsAllocatedAsNonSvmAllocation) {
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
    EXPECT_FALSE(globalSurface->isFromPool());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    globalSurface.reset(allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, false /* constant */, nullptr /* linker input */, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(alignmentSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::globalSurface, alloc->getAllocationType());
    EXPECT_FALSE(globalSurface->isFromPool());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    globalSurface.reset(allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, true /* constant */, &emptyLinkerInput, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(alignmentSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::constantSurface, alloc->getAllocationType());
    EXPECT_FALSE(globalSurface->isFromPool());
    device.getMemoryManager()->freeGraphicsMemory(alloc);

    globalSurface.reset(allocateGlobalsSurface(&svmAllocsManager, device, initData.size(), 0u, false /* constant */, &emptyLinkerInput, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    alloc = globalSurface->getGraphicsAllocation();
    ASSERT_NE(nullptr, alloc);
    ASSERT_EQ(alignmentSize, alloc->getUnderlyingBufferSize());
    EXPECT_EQ(0, memcmp(alloc->getUnderlyingBuffer(), initData.data(), initData.size()));
    EXPECT_EQ(nullptr, svmAllocsManager.getSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress()))));
    EXPECT_EQ(AllocationType::globalSurface, alloc->getAllocationType());
    EXPECT_FALSE(globalSurface->isFromPool());
    device.getMemoryManager()->freeGraphicsMemory(alloc);
}

TEST_F(AllocateGlobalSurfacePoolingDisabledTest, GivenSvmAllocsManagerWhenGlobalsAreExportedThenMemoryIsAllocatedAsUsmDeviceAllocation) {
    debugManager.flags.ForceLocalMemoryAccessMode.set(0);
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
    auto *gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(alloc->getDefaultGmm()->resourceParamsData.data());
    EXPECT_FALSE(gmmResourceParams->Flags.Info.NotLockable);
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
    gmmResourceParams = reinterpret_cast<GMM_RESCREATE_PARAMS *>(alloc->getDefaultGmm()->resourceParamsData.data());
    EXPECT_FALSE(gmmResourceParams->Flags.Info.NotLockable);
    svmAllocsManager.freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(alloc->getGpuAddress())));
}

TEST_F(AllocateGlobalSurfacePoolingDisabledTest, GivenNullSvmAllocsManagerWhenGlobalsAreExportedThenMemoryIsAllocatedAsNonSvmAllocation) {
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
            if (device.getConstantSurfacePoolAllocator().isPoolBuffer(pAllocation)) {
                device.getConstantSurfacePoolAllocator().free(globalSurface.release());
            } else {
                device.getMemoryManager()->freeGraphicsMemory(pAllocation);
            }
        }
    }
}

TEST_F(AllocateGlobalSurfacePoolingDisabledTest, whenAllocatingGlobalSurfaceWithNonZeroZeroInitSizeThenTransferOnlyInitDataToAllocation) {
    WhiteBox<LinkerInput> emptyLinkerInput;
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

    if (device.getConstantSurfacePoolAllocator().isPoolBuffer(alloc)) {
        device.getConstantSurfacePoolAllocator().free(globalSurface.release());
    } else {
        device.getMemoryManager()->freeGraphicsMemory(alloc);
    }
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
    EXPECT_FALSE(globalSurface->isFromPool());
    EXPECT_EQ(globalSurface->getGraphicsAllocation()->getUnderlyingBufferSize(), globalSurface->getSize());
    EXPECT_EQ(0u, globalSurface->getOffset());
    svmAllocsManager->freeSVMAlloc(reinterpret_cast<void *>(static_cast<uintptr_t>(globalSurface->getGpuAddress())));

    globalSurface.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalVariables, initData.data()));
    ASSERT_NE(nullptr, globalSurface);
    EXPECT_FALSE(device.getUsmGlobalSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(globalSurface->getGpuAddress())));
    EXPECT_FALSE(globalSurface->isFromPool());
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
        EXPECT_TRUE(constantSurface1->isFromPool());
        EXPECT_NE(constantSurface1->getGraphicsAllocation()->getUnderlyingBufferSize(), constantSurface1->getSize());
        EXPECT_EQ(0, memcmp(constantSurface1->getUnderlyingBuffer(), initData.data(), initData.size()));
        EXPECT_TRUE(constantSurface1->getGraphicsAllocation()->isMemObjectsAllocationWithWritableFlags());
        EXPECT_EQ(AllocationType::constantSurface, constantSurface1->getGraphicsAllocation()->getAllocationType());

        constantSurface2.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, true /* constant */, &linkerInputExportGlobalConstants, initData.data()));
        ASSERT_NE(nullptr, constantSurface2);
        EXPECT_TRUE(device.getUsmConstantSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(constantSurface2->getGpuAddress())));
        EXPECT_TRUE(constantSurface2->isFromPool());
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
        EXPECT_TRUE(globalSurface1->isFromPool());
        EXPECT_NE(globalSurface1->getGraphicsAllocation()->getUnderlyingBufferSize(), globalSurface1->getSize());
        EXPECT_EQ(0, memcmp(globalSurface1->getUnderlyingBuffer(), initData.data(), initData.size()));
        EXPECT_TRUE(globalSurface1->getGraphicsAllocation()->isMemObjectsAllocationWithWritableFlags());
        EXPECT_EQ(AllocationType::globalSurface, globalSurface1->getGraphicsAllocation()->getAllocationType());

        globalSurface2.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, initData.size(), 0u, false /* constant */, &linkerInputExportGlobalVariables, initData.data()));
        ASSERT_NE(nullptr, globalSurface2);
        EXPECT_TRUE(device.getUsmGlobalSurfaceAllocPool()->isInPool(reinterpret_cast<void *>(globalSurface2->getGpuAddress())));
        EXPECT_TRUE(globalSurface2->isFromPool());
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

TEST_F(AllocateGlobalSurfaceWithUsmPoolTest, givenPooledUSMAllocationWhenReusedChunkThenDataIsProperlyInitializedAndRestIsZeroed) {
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;
    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;

    constexpr size_t initSize = 32u;
    constexpr size_t zeroInitSize = 32u;
    constexpr size_t totalSize = initSize + zeroInitSize;
    constexpr uint8_t initValue = 7u;
    constexpr uint8_t dirtyValue = 9u;

    std::vector<uint8_t> initData(initSize, initValue);

    auto verifyAllocation = [&](SharedPoolAllocation *allocation) {
        ASSERT_NE(nullptr, allocation);
        EXPECT_TRUE(device.getUsmGlobalSurfaceAllocPool()->isInPool(
            reinterpret_cast<void *>(allocation->getGpuAddress())));
        EXPECT_NE(allocation->getGraphicsAllocation()->getUnderlyingBufferSize(),
                  allocation->getSize());
        EXPECT_TRUE(allocation->getGraphicsAllocation()->isMemObjectsAllocationWithWritableFlags());
        EXPECT_EQ(AllocationType::globalSurface,
                  allocation->getGraphicsAllocation()->getAllocationType());
    };

    std::unique_ptr<SharedPoolAllocation> globalSurface1;
    std::unique_ptr<SharedPoolAllocation> globalSurface2;

    // First allocation - new chunk from pool
    globalSurface1.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, totalSize, zeroInitSize, false, &linkerInputExportGlobalVariables, initData.data()));
    verifyAllocation(globalSurface1.get());
    EXPECT_EQ(0, memcmp(globalSurface1->getUnderlyingBuffer(), initData.data(), initSize));

    // Dirty the chunk before returning to pool
    std::memset(globalSurface1->getUnderlyingBuffer(), dirtyValue, globalSurface1->getSize());
    device.getUsmGlobalSurfaceAllocPool()->freeSVMAlloc(reinterpret_cast<void *>(globalSurface1->getGpuAddress()), false);

    // Second allocation - should reuse the same chunk
    globalSurface2.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, totalSize, zeroInitSize, false, &linkerInputExportGlobalVariables, initData.data()));
    verifyAllocation(globalSurface2.get());

    // Verify it's the same chunk
    EXPECT_EQ(globalSurface1->getGraphicsAllocation(), globalSurface2->getGraphicsAllocation());
    EXPECT_EQ(globalSurface1->getGpuAddress(), globalSurface2->getGpuAddress());
    EXPECT_EQ(globalSurface1->getOffset(), globalSurface2->getOffset());
    EXPECT_EQ(globalSurface1->getSize(), globalSurface2->getSize());

    // Verify proper initialization: initData followed by zeros for entire chunk
    std::vector<uint8_t> expectedData(globalSurface2->getSize(), 0);
    std::memcpy(expectedData.data(), initData.data(), initSize);

    EXPECT_EQ(0, memcmp(globalSurface2->getUnderlyingBuffer(), expectedData.data(), expectedData.size()));
}

TEST_F(AllocateGlobalSurfaceWithUsmPoolTest, givenPooledUSMAllocationWhenReusedChunkWithBssOnlyDataThenEntireChunkIsZeroed) {
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;
    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;

    constexpr size_t totalSize = 64u;
    constexpr size_t zeroInitSize = totalSize; // BSS only - no init data
    constexpr uint8_t dirtyValue = 9u;

    auto verifyAllocation = [&](SharedPoolAllocation *allocation) {
        ASSERT_NE(nullptr, allocation);
        EXPECT_TRUE(device.getUsmGlobalSurfaceAllocPool()->isInPool(
            reinterpret_cast<void *>(allocation->getGpuAddress())));
        EXPECT_NE(allocation->getGraphicsAllocation()->getUnderlyingBufferSize(),
                  allocation->getSize());
        EXPECT_TRUE(allocation->getGraphicsAllocation()->isMemObjectsAllocationWithWritableFlags());
        EXPECT_EQ(AllocationType::globalSurface,
                  allocation->getGraphicsAllocation()->getAllocationType());
    };

    std::unique_ptr<SharedPoolAllocation> globalSurface1;
    std::unique_ptr<SharedPoolAllocation> globalSurface2;

    // First allocation - BSS only (no init data)
    globalSurface1.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, totalSize, zeroInitSize, false, &linkerInputExportGlobalVariables, nullptr));
    verifyAllocation(globalSurface1.get());

    // Verify initial allocation is zeroed
    std::vector<uint8_t> expectedZeros(globalSurface1->getSize(), 0);
    EXPECT_EQ(0, memcmp(globalSurface1->getUnderlyingBuffer(), expectedZeros.data(), expectedZeros.size()));

    // Dirty the chunk before returning to pool
    std::memset(globalSurface1->getUnderlyingBuffer(), dirtyValue, globalSurface1->getSize());
    device.getUsmGlobalSurfaceAllocPool()->freeSVMAlloc(reinterpret_cast<void *>(globalSurface1->getGpuAddress()), false);

    // Second allocation - should reuse the same chunk
    globalSurface2.reset(allocateGlobalsSurface(svmAllocsManager.get(), device, totalSize, zeroInitSize, false, &linkerInputExportGlobalVariables, nullptr));
    verifyAllocation(globalSurface2.get());

    // Verify it's the same chunk
    EXPECT_EQ(globalSurface1->getGraphicsAllocation(), globalSurface2->getGraphicsAllocation());
    EXPECT_EQ(globalSurface1->getGpuAddress(), globalSurface2->getGpuAddress());
    EXPECT_EQ(globalSurface1->getOffset(), globalSurface2->getOffset());
    EXPECT_EQ(globalSurface1->getSize(), globalSurface2->getSize());

    // Verify entire chunk is zeroed (no dirty data from previous use)
    EXPECT_EQ(0, memcmp(globalSurface2->getUnderlyingBuffer(), expectedZeros.data(), expectedZeros.size()));
}

TEST_F(AllocateGlobalSurfaceWithUsmPoolTest, givenPooledUSMAllocationWhenOnlyInitDataWithoutBssSectionThenMemsetAllocationIsNotCalled) {
    mockProductHelper->isBlitCopyRequiredForLocalMemoryResult = false;
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;
    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;

    constexpr size_t initSize = 64u;
    constexpr size_t zeroInitSize = 0u;
    constexpr size_t totalSize = initSize + zeroInitSize;
    constexpr uint8_t initValue = 7u;

    std::vector<uint8_t> initData(initSize, initValue);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device.getMemoryManager());
    mockMemoryManager->memsetAllocationCalled = 0;

    auto globalSurface = std::unique_ptr<SharedPoolAllocation>(allocateGlobalsSurface(svmAllocsManager.get(), device, totalSize, zeroInitSize, false, &linkerInputExportGlobalVariables, initData.data()));

    ASSERT_NE(nullptr, globalSurface);
    EXPECT_EQ(0u, mockMemoryManager->memsetAllocationCalled);
}

TEST_F(AllocateGlobalSurfaceWithUsmPoolTest, givenPooledUSMAllocationWhenInitDataAndBssSectionThenMemsetAllocationIsCalledOnceForBssSection) {
    mockProductHelper->isBlitCopyRequiredForLocalMemoryResult = false;
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;
    linkerInputExportGlobalVariables.traits.exportsGlobalVariables = true;

    constexpr size_t initSize = 32u;
    constexpr size_t zeroInitSize = 32u;
    constexpr size_t totalSize = initSize + zeroInitSize;
    constexpr uint8_t initValue = 7u;

    std::vector<uint8_t> initData(initSize, initValue);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device.getMemoryManager());
    mockMemoryManager->memsetAllocationCalled = 0;

    auto globalSurface = std::unique_ptr<SharedPoolAllocation>(allocateGlobalsSurface(svmAllocsManager.get(), device, totalSize, zeroInitSize, false, &linkerInputExportGlobalVariables, initData.data()));

    ASSERT_NE(nullptr, globalSurface);
    EXPECT_EQ(1u, mockMemoryManager->memsetAllocationCalled);
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

struct AllocateGlobalSurfaceWithGenericPoolTest : public ::testing::Test {
    void SetUp() override {
        device.injectMemoryManager(new MockMemoryManager());

        mockProductHelper = new MockProductHelper;
        device.getRootDeviceEnvironmentRef().productHelper.reset(mockProductHelper);

        mockProductHelper->is2MBLocalMemAlignmentEnabledResult = true;
    }

    MockProductHelper *mockProductHelper{nullptr};
    WhiteBox<LinkerInput> linkerInput;
    MockDevice device{};

    DebugManagerStateRestore restore;
};

TEST_F(AllocateGlobalSurfaceWithGenericPoolTest, Given2MBLocalMemAlignmentDisabledThenGlobalSurfaceAllocationNotTakenFromGenericPool) {
    mockProductHelper->is2MBLocalMemAlignmentEnabledResult = false;

    std::vector<uint8_t> initData;
    initData.resize(64, 7U);
    std::unique_ptr<SharedPoolAllocation> globalSurface;
    size_t expectedAlignedSize = alignUp(initData.size(), MemoryConstants::pageSize);

    for (auto isConstant : {true, false}) {
        auto expectedAllocType = isConstant ? AllocationType::constantSurface : AllocationType::globalSurface;
        globalSurface.reset(allocateGlobalsSurface(nullptr, device, initData.size(), 0u, isConstant /* constant */, &linkerInput, initData.data()));
        ASSERT_NE(nullptr, globalSurface);
        EXPECT_FALSE(globalSurface->isFromPool());
        EXPECT_EQ(expectedAlignedSize, globalSurface->getGraphicsAllocation()->getUnderlyingBufferSize());
        EXPECT_EQ(globalSurface->getGraphicsAllocation()->getUnderlyingBufferSize(), globalSurface->getSize());
        EXPECT_EQ(0u, globalSurface->getOffset());
        EXPECT_EQ(0, memcmp(globalSurface->getUnderlyingBuffer(), initData.data(), initData.size()));
        EXPECT_EQ(expectedAllocType, globalSurface->getGraphicsAllocation()->getAllocationType());
        EXPECT_FALSE(device.getGlobalSurfacePoolAllocator().isPoolBuffer(globalSurface->getGraphicsAllocation()));
        EXPECT_FALSE(device.getConstantSurfacePoolAllocator().isPoolBuffer(globalSurface->getGraphicsAllocation()));
        device.getMemoryManager()->freeGraphicsMemory(globalSurface->getGraphicsAllocation());
    }
}

TEST_F(AllocateGlobalSurfaceWithGenericPoolTest, Given2MBLocalMemAlignmentEnabledThenGlobalSurfaceAllocationTakenFromGenericPool) {
    std::vector<uint8_t> initData;
    initData.resize(64, 7U);

    for (auto isConstant : {true, false}) {
        auto expectedAllocType = isConstant ? AllocationType::constantSurface : AllocationType::globalSurface;

        std::unique_ptr<SharedPoolAllocation> globalSurface1;
        std::unique_ptr<SharedPoolAllocation> globalSurface2;

        globalSurface1.reset(allocateGlobalsSurface(nullptr, device, initData.size(), 0u, isConstant /* constant */, &linkerInput, initData.data()));
        ASSERT_NE(nullptr, globalSurface1);
        EXPECT_TRUE(globalSurface1->isFromPool());
        EXPECT_NE(globalSurface1->getGraphicsAllocation()->getUnderlyingBufferSize(), globalSurface1->getSize());
        EXPECT_EQ(0, memcmp(globalSurface1->getUnderlyingBuffer(), initData.data(), initData.size()));
        EXPECT_EQ(expectedAllocType, globalSurface1->getGraphicsAllocation()->getAllocationType());

        globalSurface2.reset(allocateGlobalsSurface(nullptr, device, initData.size(), 0u, isConstant /* constant */, &linkerInput, initData.data()));
        ASSERT_NE(nullptr, globalSurface2);
        EXPECT_TRUE(globalSurface2->isFromPool());
        EXPECT_NE(globalSurface2->getGraphicsAllocation()->getUnderlyingBufferSize(), globalSurface2->getSize());
        EXPECT_EQ(0, memcmp(globalSurface2->getUnderlyingBuffer(), initData.data(), initData.size()));
        EXPECT_EQ(expectedAllocType, globalSurface2->getGraphicsAllocation()->getAllocationType());

        // Both allocations should come from the same pool
        EXPECT_EQ(globalSurface1->getGraphicsAllocation(), globalSurface2->getGraphicsAllocation());
        EXPECT_EQ(globalSurface1->getSize(), globalSurface2->getSize());
        EXPECT_NE(globalSurface1->getGpuAddress(), globalSurface2->getGpuAddress());
        EXPECT_NE(globalSurface1->getOffset(), globalSurface2->getOffset());

        if (isConstant) {
            EXPECT_TRUE(device.getConstantSurfacePoolAllocator().isPoolBuffer(globalSurface1->getGraphicsAllocation()));
            EXPECT_TRUE(device.getConstantSurfacePoolAllocator().isPoolBuffer(globalSurface2->getGraphicsAllocation()));
            device.getConstantSurfacePoolAllocator().free(globalSurface1.release());
            device.getConstantSurfacePoolAllocator().free(globalSurface2.release());
        } else {
            EXPECT_TRUE(device.getGlobalSurfacePoolAllocator().isPoolBuffer(globalSurface1->getGraphicsAllocation()));
            EXPECT_TRUE(device.getGlobalSurfacePoolAllocator().isPoolBuffer(globalSurface2->getGraphicsAllocation()));
            device.getGlobalSurfacePoolAllocator().free(globalSurface1.release());
            device.getGlobalSurfacePoolAllocator().free(globalSurface2.release());
        }
    }
}

TEST_F(AllocateGlobalSurfaceWithGenericPoolTest, givenPooledAllocationWhenDataIsInitializedThenNoCorruptionOccurs) {
    constexpr size_t initSize = 32u;
    constexpr size_t zeroInitSize = 32u;
    constexpr size_t totalSize = initSize + zeroInitSize;
    constexpr uint8_t initValue = 7u;

    std::vector<uint8_t> initData(initSize, initValue);

    auto verifyAllocation = [&](SharedPoolAllocation *allocation) {
        ASSERT_NE(nullptr, allocation);
        EXPECT_TRUE(allocation->isFromPool());
        EXPECT_NE(allocation->getGraphicsAllocation()->getUnderlyingBufferSize(),
                  allocation->getSize());
        EXPECT_EQ(AllocationType::globalSurface,
                  allocation->getGraphicsAllocation()->getAllocationType());
    };

    for (int i = 0; i < 3; ++i) {
        auto globalSurface = allocateGlobalsSurface(nullptr, device, totalSize, zeroInitSize,
                                                    false, &linkerInput, initData.data());
        verifyAllocation(globalSurface);

        // Verify proper initialization: initData followed by zeros
        std::vector<uint8_t> expectedData(totalSize, 0);
        std::memcpy(expectedData.data(), initData.data(), initSize);
        EXPECT_EQ(0, memcmp(globalSurface->getUnderlyingBuffer(), expectedData.data(), totalSize));

        device.getGlobalSurfacePoolAllocator().free(globalSurface);
    }
}

TEST_F(AllocateGlobalSurfaceWithGenericPoolTest, givenPooledAllocationWithBssOnlyWhenAllocatedMultipleTimesThenAlwaysZeroed) {
    constexpr size_t totalSize = 64u;
    constexpr size_t zeroInitSize = totalSize;

    std::vector<uint8_t> expectedZeros(totalSize, 0);

    for (int i = 0; i < 3; ++i) {
        auto constantSurface = allocateGlobalsSurface(nullptr, device, totalSize, zeroInitSize,
                                                      true, &linkerInput, nullptr);
        ASSERT_NE(nullptr, constantSurface);
        EXPECT_TRUE(constantSurface->isFromPool());

        // Verify entire allocation is zeroed
        EXPECT_EQ(0, memcmp(constantSurface->getUnderlyingBuffer(), expectedZeros.data(), totalSize));

        device.getConstantSurfacePoolAllocator().free(constantSurface);
    }
}

TEST_F(AllocateGlobalSurfaceWithGenericPoolTest, givenPooledAllocationWhenOnlyInitDataWithoutBssSectionThenMemsetAllocationIsNotCalled) {
    mockProductHelper->isBlitCopyRequiredForLocalMemoryResult = false;

    constexpr size_t initSize = 64u;
    constexpr size_t zeroInitSize = 0u;
    constexpr size_t totalSize = initSize + zeroInitSize;
    constexpr uint8_t initValue = 7u;

    std::vector<uint8_t> initData(initSize, initValue);

    auto mockMemoryManager = static_cast<MockMemoryManager *>(device.getMemoryManager());
    mockMemoryManager->memsetAllocationCalled = 0;

    auto globalSurface = std::unique_ptr<SharedPoolAllocation>(allocateGlobalsSurface(nullptr, device, totalSize, zeroInitSize, false, &linkerInput, initData.data()));

    ASSERT_NE(nullptr, globalSurface);
    EXPECT_EQ(0u, mockMemoryManager->memsetAllocationCalled);

    device.getGlobalSurfacePoolAllocator().free(globalSurface.release());
}

TEST_F(AllocateGlobalSurfaceWithGenericPoolTest, GivenAllocationExceedsMaxSizeThenFallbackToDirectAllocation) {
    for (auto isConstant : {false, true}) {
        auto expectedAllocType = isConstant ? AllocationType::constantSurface : AllocationType::globalSurface;

        size_t oversizedAllocation = isConstant ? ConstantSurfacePoolTraits::maxAllocationSize : GlobalSurfacePoolTraits::maxAllocationSize;
        oversizedAllocation += 1;

        std::vector<uint8_t> initData(oversizedAllocation, 7u);

        auto globalSurface = std::unique_ptr<SharedPoolAllocation>(allocateGlobalsSurface(nullptr, device, oversizedAllocation, 0u, isConstant, &linkerInput, initData.data()));
        ASSERT_NE(nullptr, globalSurface);
        EXPECT_FALSE(globalSurface->isFromPool());
        EXPECT_EQ(0u, globalSurface->getOffset());
        EXPECT_EQ(expectedAllocType, globalSurface->getGraphicsAllocation()->getAllocationType());

        EXPECT_FALSE(device.getConstantSurfacePoolAllocator().isPoolBuffer(globalSurface->getGraphicsAllocation()));
        EXPECT_FALSE(device.getGlobalSurfacePoolAllocator().isPoolBuffer(globalSurface->getGraphicsAllocation()));

        device.getMemoryManager()->freeGraphicsMemory(globalSurface->getGraphicsAllocation());
    }
}