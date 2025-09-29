/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/local_id_gen.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/utilities/mem_lifetime.h"
#include "shared/test/common/helpers/raii_gfx_core_helper.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/mocks/mock_modules_zebin.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/kernel/kernel_shared_state.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/kernel_max_cooperative_groups_count_fixture.h"
#include "level_zero/core/test/unit_tests/fixtures/module_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include "encode_surface_state_args.h"
#include "implicit_args.h"

namespace L0 {
#include "level_zero/core/source/kernel/patch_with_implicit_surface.inl"

namespace ult {

using KernelImpTest = Test<DeviceFixture>;

TEST_F(KernelImpTest, GivenKernelMutableStateWhenPerThreadDataForWholeGroupReservedThenReallocatedIfNeeded) {

    KernelMutableState privateState{};
    constexpr size_t perThreadDataSize1{5U};
    privateState.reservePerThreadDataForWholeThreadGroup(perThreadDataSize1);

    auto alloc1{privateState.perThreadDataForWholeThreadGroup};
    EXPECT_NE(alloc1, nullptr);
    EXPECT_EQ(privateState.perThreadDataSizeForWholeThreadGroup, perThreadDataSize1);
    EXPECT_EQ(privateState.perThreadDataSizeForWholeThreadGroupAllocated, perThreadDataSize1);

    constexpr size_t perThreadDataSize2{3U};
    privateState.reservePerThreadDataForWholeThreadGroup(perThreadDataSize2);
    auto alloc2{privateState.perThreadDataForWholeThreadGroup};
    EXPECT_EQ(alloc1, alloc2);
    EXPECT_EQ(privateState.perThreadDataSizeForWholeThreadGroupAllocated, perThreadDataSize1);
    EXPECT_EQ(privateState.perThreadDataSizeForWholeThreadGroup, perThreadDataSize2);
}

TEST_F(KernelImpTest, GivenKernelMutableStateWhenAssigningToItselfThenTheCurrentObjectReturned) {
    constexpr size_t mockSize{8U};

    KernelMutableState state1{};
    state1.crossThreadData.clear();
    state1.crossThreadData.resize(mockSize, 0x0);
    auto addressBeforeAssignment{state1.crossThreadData.data()};

    auto &notReallyDifferentState{state1};
    state1 = notReallyDifferentState;
    auto addressAfterAssignment{state1.crossThreadData.data()};

    auto &&notReallyDifferentState2{std::move(state1)};
    state1 = std::move(notReallyDifferentState2);
    auto addressAfterAssignment2{state1.crossThreadData.data()};

    EXPECT_EQ(addressBeforeAssignment, addressAfterAssignment);
    EXPECT_EQ(addressBeforeAssignment, addressAfterAssignment2);
}

void fillKernelMutableStateWithMockData(KernelMutableState &state) {
    state.unifiedMemoryControls = {true, true, true};
    state.pImplicitArgs.ptr.reset(new ImplicitArgs{});
    state.pImplicitArgs->v0 = ImplicitArgsV0{
        .numWorkDim = 2,
        .simdWidth = 4,
        .globalSizeX = 8,
        .rtGlobalBufferPtr = 0x987654321,
    };
    state.pExtension = std::make_unique<KernelExt>();

    constexpr size_t mockSize{8U};
    state.crossThreadData.clear();
    state.crossThreadData.reserve(mockSize);
    std::ranges::copy(std::to_array<uint8_t, mockSize>({11, 12, 13, 14, 15, 16, 17, 18}), std::back_inserter(state.crossThreadData));

    state.surfaceStateHeapData.clear();
    state.surfaceStateHeapData.reserve(mockSize);
    std::ranges::copy(std::to_array<uint8_t, mockSize>({21, 22, 23, 24, 25, 26, 27, 28}), std::back_inserter(state.surfaceStateHeapData));

    state.dynamicStateHeapData.clear();
    state.dynamicStateHeapData.reserve(mockSize);
    std::ranges::copy(std::to_array<uint8_t>({31, 32, 33, 34, 35, 36, 37, 38}), std::back_inserter(state.dynamicStateHeapData));

    state.reservePerThreadDataForWholeThreadGroup(mockSize);
    std::memcpy(state.perThreadDataForWholeThreadGroup, std::to_array<uint8_t>({41, 42, 43, 44, 45, 46, 47, 48}).data(), mockSize);

    KernelMutableState::SuggestGroupSizeCacheEntry mockGroupSizeCacheEntry(Vec3<size_t>{52U, 54U, 58U}.values,
                                                                           std::numeric_limits<uint32_t>::max(),
                                                                           Vec3<size_t>{62U, 64U, 68U}.values);
    state.suggestGroupSizeCache.push_back(mockGroupSizeCacheEntry);

    state.kernelArgInfos.push_back({.value = reinterpret_cast<void *>(0x87654321ULL)});
    state.kernelArgHandlers.push_back(&KernelImp::setArgBuffer);
    state.argumentsResidencyContainer.push_back(reinterpret_cast<NEO::GraphicsAllocation *>(0x76543210ULL));
    state.implicitArgsResidencyContainerIndices.push_back(10);
    state.internalResidencyContainer.push_back(reinterpret_cast<NEO::GraphicsAllocation *>(0x65432109ULL));
    state.isArgUncached.push_back(true);
    state.isBindlessOffsetSet.push_back(true);
    state.usingSurfaceStateHeap.push_back(true);
    state.slmArgSizes.push_back(252U);
    state.slmArgOffsetValues.push_back(151U);
    state.syncBufferIndex = std::numeric_limits<size_t>::max() - 10;
    state.regionGroupBarrierIndex = std::numeric_limits<size_t>::max() - 11;
    state.globalOffsets[0] = 71;
    state.globalOffsets[1] = 72;
    state.globalOffsets[2] = 73;
    state.groupSize[0] = 74;
    state.groupSize[1] = 75;
    state.groupSize[2] = 76;
    state.perThreadDataSize = 81U;
    state.slmArgsTotalSize = 82U;
    state.kernelRequiresQueueUncachedMocsCount = 83U;
    state.kernelRequiresUncachedMocsCount = 84U;
    state.requiredWorkgroupOrder = 85U;
    state.threadExecutionMask = 86U;
    state.numThreadsPerThreadGroup = 87U;
    state.cacheConfigFlags = 88U;
    state.kernelHasIndirectAccess = true;
    state.kernelRequiresGenerationOfLocalIdsByRuntime = false;
}
TEST_F(KernelImpTest, GivenKernelMutableStateWhenAssignmentOperatorUsedThenProperDeepCopyOrMoveMade) {
    KernelMutableState state1{};
    fillKernelMutableStateWithMockData(state1);

    KernelMutableState state2{};
    state2 = state1; // assignment operator is being tested

    EXPECT_EQ(0, std::memcmp(state1.crossThreadData.data(), state2.crossThreadData.data(), state1.crossThreadData.size()));
    EXPECT_EQ(0, std::memcmp(state1.surfaceStateHeapData.data(), state2.surfaceStateHeapData.data(), state1.surfaceStateHeapData.size()));
    EXPECT_EQ(0, std::memcmp(state1.dynamicStateHeapData.data(), state2.dynamicStateHeapData.data(), state1.dynamicStateHeapData.size()));

    EXPECT_EQ(0, std::memcmp(state1.perThreadDataForWholeThreadGroup, state2.perThreadDataForWholeThreadGroup, state1.perThreadDataSizeForWholeThreadGroup));

    EXPECT_TRUE(state1.suggestGroupSizeCache == state2.suggestGroupSizeCache);
    EXPECT_TRUE(state1.kernelArgInfos == state2.kernelArgInfos);
    EXPECT_TRUE(state1.kernelArgHandlers == state2.kernelArgHandlers);
    EXPECT_TRUE(state1.argumentsResidencyContainer == state2.argumentsResidencyContainer);
    EXPECT_TRUE(state1.implicitArgsResidencyContainerIndices == state2.implicitArgsResidencyContainerIndices);
    EXPECT_TRUE(state1.internalResidencyContainer == state2.internalResidencyContainer);
    EXPECT_TRUE(state1.isArgUncached == state2.isArgUncached);
    EXPECT_TRUE(state1.isBindlessOffsetSet == state2.isBindlessOffsetSet);
    EXPECT_TRUE(state1.usingSurfaceStateHeap == state2.usingSurfaceStateHeap);
    EXPECT_TRUE(state1.slmArgSizes == state2.slmArgSizes);
    EXPECT_TRUE(state1.slmArgOffsetValues == state2.slmArgOffsetValues);
    EXPECT_TRUE(state1.syncBufferIndex == state2.syncBufferIndex);
    EXPECT_TRUE(state1.regionGroupBarrierIndex == state2.regionGroupBarrierIndex);

    EXPECT_EQ(0, std::memcmp(state1.globalOffsets, state2.globalOffsets, KernelMutableState::dimMax * sizeof(uint32_t)));
    EXPECT_EQ(0, std::memcmp(state1.groupSize, state2.groupSize, KernelMutableState::dimMax * sizeof(uint32_t)));
    EXPECT_EQ(state1.crossThreadData, state2.crossThreadData);
    EXPECT_EQ(state1.surfaceStateHeapData, state2.surfaceStateHeapData);
    EXPECT_EQ(state1.perThreadDataSize, state2.perThreadDataSize);
    EXPECT_EQ(state1.slmArgsTotalSize, state2.slmArgsTotalSize);
    EXPECT_EQ(state1.perThreadDataSizeForWholeThreadGroup, state2.perThreadDataSizeForWholeThreadGroup);
    EXPECT_EQ(state1.perThreadDataSizeForWholeThreadGroupAllocated, state2.perThreadDataSizeForWholeThreadGroupAllocated);
    EXPECT_EQ(state1.kernelRequiresQueueUncachedMocsCount, state2.kernelRequiresQueueUncachedMocsCount);
    EXPECT_EQ(state1.kernelRequiresUncachedMocsCount, state2.kernelRequiresUncachedMocsCount);
    EXPECT_EQ(state1.requiredWorkgroupOrder, state2.requiredWorkgroupOrder);
    EXPECT_EQ(state1.threadExecutionMask, state2.threadExecutionMask);
    EXPECT_EQ(state1.numThreadsPerThreadGroup, state2.numThreadsPerThreadGroup);
    EXPECT_EQ(state1.cacheConfigFlags, state2.cacheConfigFlags);
    EXPECT_EQ(state1.kernelHasIndirectAccess, state2.kernelHasIndirectAccess);

    KernelMutableState state3{};
    state3 = std::move(state1);

    EXPECT_EQ(0U, state1.crossThreadData.size());
    EXPECT_EQ(0U, state1.surfaceStateHeapData.size());
    EXPECT_EQ(0U, state1.dynamicStateHeapData.size());
    EXPECT_EQ(nullptr, state1.pImplicitArgs);
    EXPECT_EQ(nullptr, state1.pExtension);
    EXPECT_EQ(nullptr, state1.perThreadDataForWholeThreadGroup);
    EXPECT_EQ(0U, state1.perThreadDataSizeForWholeThreadGroup);
    EXPECT_EQ(0U, state1.perThreadDataSizeForWholeThreadGroupAllocated);

    EXPECT_EQ(0, std::memcmp(state3.crossThreadData.data(), state2.crossThreadData.data(), state3.crossThreadData.size()));
    EXPECT_EQ(0, std::memcmp(state3.surfaceStateHeapData.data(), state2.surfaceStateHeapData.data(), state3.surfaceStateHeapData.size()));
    EXPECT_EQ(0, std::memcmp(state3.dynamicStateHeapData.data(), state2.dynamicStateHeapData.data(), state3.dynamicStateHeapData.size()));

    EXPECT_EQ(0, std::memcmp(state3.perThreadDataForWholeThreadGroup, state2.perThreadDataForWholeThreadGroup, state3.perThreadDataSizeForWholeThreadGroup));

    EXPECT_TRUE(state3.suggestGroupSizeCache == state2.suggestGroupSizeCache);
    EXPECT_TRUE(state3.kernelArgInfos == state2.kernelArgInfos);
    EXPECT_TRUE(state3.kernelArgHandlers == state2.kernelArgHandlers);
    EXPECT_TRUE(state3.argumentsResidencyContainer == state2.argumentsResidencyContainer);
    EXPECT_TRUE(state3.implicitArgsResidencyContainerIndices == state2.implicitArgsResidencyContainerIndices);
    EXPECT_TRUE(state3.internalResidencyContainer == state2.internalResidencyContainer);
    EXPECT_TRUE(state3.isArgUncached == state2.isArgUncached);
    EXPECT_TRUE(state3.isBindlessOffsetSet == state2.isBindlessOffsetSet);
    EXPECT_TRUE(state3.usingSurfaceStateHeap == state2.usingSurfaceStateHeap);
    EXPECT_TRUE(state3.slmArgSizes == state2.slmArgSizes);
    EXPECT_TRUE(state3.slmArgOffsetValues == state2.slmArgOffsetValues);
    EXPECT_TRUE(state3.syncBufferIndex == state2.syncBufferIndex);
    EXPECT_TRUE(state3.regionGroupBarrierIndex == state2.regionGroupBarrierIndex);

    EXPECT_EQ(0, std::memcmp(state3.globalOffsets, state2.globalOffsets, KernelMutableState::dimMax * sizeof(uint32_t)));
    EXPECT_EQ(0, std::memcmp(state3.groupSize, state2.groupSize, KernelMutableState::dimMax * sizeof(uint32_t)));
    EXPECT_EQ(state3.crossThreadData.size(), state2.crossThreadData.size());
    EXPECT_EQ(state3.surfaceStateHeapData.size(), state2.surfaceStateHeapData.size());
    EXPECT_EQ(state3.dynamicStateHeapData.size(), state2.dynamicStateHeapData.size());
    EXPECT_EQ(state3.perThreadDataSize, state2.perThreadDataSize);
    EXPECT_EQ(state3.slmArgsTotalSize, state2.slmArgsTotalSize);
    EXPECT_EQ(state3.perThreadDataSizeForWholeThreadGroup, state2.perThreadDataSizeForWholeThreadGroup);
    EXPECT_EQ(state3.perThreadDataSizeForWholeThreadGroupAllocated, state2.perThreadDataSizeForWholeThreadGroupAllocated);
    EXPECT_EQ(state3.kernelRequiresQueueUncachedMocsCount, state2.kernelRequiresQueueUncachedMocsCount);
    EXPECT_EQ(state3.kernelRequiresUncachedMocsCount, state2.kernelRequiresUncachedMocsCount);
    EXPECT_EQ(state3.requiredWorkgroupOrder, state2.requiredWorkgroupOrder);
    EXPECT_EQ(state3.threadExecutionMask, state2.threadExecutionMask);
    EXPECT_EQ(state3.numThreadsPerThreadGroup, state2.numThreadsPerThreadGroup);
    EXPECT_EQ(state3.cacheConfigFlags, state2.cacheConfigFlags);
    EXPECT_EQ(state3.kernelHasIndirectAccess, state2.kernelHasIndirectAccess);
}

TEST_F(KernelImpTest, GivenKernelPrivateStateWhenKernelImpClonedThenSharedStateIsSharedAndPrivateIsCopied) {
    NEO::KernelDescriptor descriptor;
    WhiteBox<::L0::KernelImmutableData> kernelInfo{};
    kernelInfo.kernelDescriptor = &descriptor;

    Mock<Module> module(device, nullptr);
    WhiteBox<KernelImp> kernel1;
    kernel1.sharedState->kernelImmData = &kernelInfo;
    kernel1.module = &module;

    constexpr size_t mockSize{8U};
    kernel1.privateState.crossThreadData.clear();
    kernel1.privateState.crossThreadData.reserve(mockSize);
    std::ranges::copy(std::to_array<uint8_t, mockSize>({91, 92, 93, 94, 95, 96, 97, 98}), std::back_inserter(kernel1.privateState.crossThreadData));
    kernel1.privateState.reservePerThreadDataForWholeThreadGroup(mockSize);
    std::memcpy(kernel1.privateState.perThreadDataForWholeThreadGroup, std::to_array<uint8_t>({81, 82, 83, 84, 85, 86, 87, 88}).data(), mockSize);

    EXPECT_NE(nullptr, kernel1.ownedSharedState.get());
    EXPECT_EQ(kernel1.sharedState, kernel1.ownedSharedState.get());
    auto clonedKernel = kernel1.makeDependentClone();
    auto kernel2 = static_cast<WhiteBox<KernelImp> *>(clonedKernel.get());
    EXPECT_EQ(nullptr, kernel2->ownedSharedState.get());
    EXPECT_EQ(kernel2->sharedState, kernel1.ownedSharedState.get());

    // KernelMutableState part taken from `state`
    EXPECT_EQ(0, std::memcmp(kernel2->privateState.crossThreadData.data(), kernel1.privateState.crossThreadData.data(), mockSize));
    EXPECT_EQ(0, std::memcmp(kernel2->privateState.perThreadDataForWholeThreadGroup, kernel1.privateState.perThreadDataForWholeThreadGroup, mockSize));

    // KernelImp part taken from `kernel1`
    EXPECT_EQ(kernel2->sharedState->kernelImmData, &kernelInfo);
    EXPECT_EQ(kernel2->sharedState->devicePrintfKernelMutex, kernel1.sharedState->devicePrintfKernelMutex);
    EXPECT_EQ(kernel2->sharedState->privateMemoryGraphicsAllocation, kernel1.sharedState->privateMemoryGraphicsAllocation);
    EXPECT_EQ(kernel2->sharedState->printfBuffer, kernel1.sharedState->printfBuffer);
    EXPECT_EQ(kernel2->sharedState->surfaceStateAlignmentMask, kernel1.sharedState->surfaceStateAlignmentMask);
    EXPECT_EQ(kernel2->sharedState->surfaceStateAlignment, kernel1.sharedState->surfaceStateAlignment);
    EXPECT_EQ(kernel2->sharedState->implicitArgsVersion, kernel1.sharedState->implicitArgsVersion);
    EXPECT_EQ(kernel2->sharedState->walkerInlineDataSize, kernel1.sharedState->walkerInlineDataSize);
    EXPECT_EQ(kernel2->sharedState->maxWgCountPerTileCcs, kernel1.sharedState->maxWgCountPerTileCcs);
    EXPECT_EQ(kernel2->sharedState->maxWgCountPerTileRcs, kernel1.sharedState->maxWgCountPerTileRcs);
    EXPECT_EQ(kernel2->sharedState->maxWgCountPerTileCooperative, kernel1.sharedState->maxWgCountPerTileCooperative);
    EXPECT_EQ(kernel2->sharedState->heaplessEnabled, kernel1.sharedState->heaplessEnabled);
    EXPECT_EQ(kernel2->sharedState->implicitScalingEnabled, kernel1.sharedState->implicitScalingEnabled);
    EXPECT_EQ(kernel2->sharedState->rcsAvailable, kernel1.sharedState->rcsAvailable);
    EXPECT_EQ(kernel2->sharedState->cooperativeSupport, kernel1.sharedState->cooperativeSupport);
}

TEST_F(KernelImpTest, GivenCrossThreadDataThenIsCorrectlyPatchedWithGlobalWorkSizeAndGroupCount) {
    WhiteBox<::L0::KernelImmutableData> kernelInfo = {};
    NEO::KernelDescriptor descriptor;
    kernelInfo.kernelDescriptor = &descriptor;
    kernelInfo.kernelDescriptor->payloadMappings.dispatchTraits.globalWorkSize[0] = 0 * sizeof(uint32_t);
    kernelInfo.kernelDescriptor->payloadMappings.dispatchTraits.globalWorkSize[1] = 1 * sizeof(uint32_t);
    kernelInfo.kernelDescriptor->payloadMappings.dispatchTraits.globalWorkSize[2] = 2 * sizeof(uint32_t);
    kernelInfo.kernelDescriptor->payloadMappings.dispatchTraits.numWorkGroups[0] = 3 * sizeof(uint32_t);
    kernelInfo.kernelDescriptor->payloadMappings.dispatchTraits.numWorkGroups[1] = 4 * sizeof(uint32_t);
    kernelInfo.kernelDescriptor->payloadMappings.dispatchTraits.numWorkGroups[2] = 5 * sizeof(uint32_t);

    Mock<KernelImp> kernel;
    kernel.sharedState->kernelImmData = &kernelInfo;
    kernel.privateState.crossThreadData.resize(sizeof(uint32_t[6]));
    kernel.privateState.groupSize[0] = 2;
    kernel.privateState.groupSize[1] = 3;
    kernel.privateState.groupSize[2] = 5;

    kernel.KernelImp::setGroupCount(7, 11, 13);
    auto crossThread = kernel.KernelImp::getCrossThreadData();
    ASSERT_NE(nullptr, crossThread);
    const uint32_t *globalWorkSizes = reinterpret_cast<const uint32_t *>(crossThread);
    EXPECT_EQ(2U * 7U, globalWorkSizes[0]);
    EXPECT_EQ(3U * 11U, globalWorkSizes[1]);
    EXPECT_EQ(5U * 13U, globalWorkSizes[2]);

    const uint32_t *numGroups = globalWorkSizes + 3;
    EXPECT_EQ(7U, numGroups[0]);
    EXPECT_EQ(11U, numGroups[1]);
    EXPECT_EQ(13U, numGroups[2]);

    kernel.privateState.crossThreadData.clear();
}

TEST_F(KernelImpTest, givenExecutionMaskWithoutReminderWhenProgrammingItsValueThenSetValidNumberOfBits) {
    NEO::KernelDescriptor descriptor = {};
    WhiteBox<KernelImmutableData> kernelInfo = {};
    kernelInfo.kernelDescriptor = &descriptor;

    Mock<Module> module(device, nullptr);
    Mock<KernelImp> kernel;
    kernel.sharedState->kernelImmData = &kernelInfo;
    kernel.module = &module;

    const std::array<uint32_t, 4> testedSimd = {{1, 8, 16, 32}};
    kernel.privateState.groupSize[0] = 0;
    kernel.privateState.groupSize[1] = 0;
    kernel.privateState.groupSize[2] = 0;

    for (auto simd : testedSimd) {
        descriptor.kernelAttributes.simdSize = simd;
        kernel.KernelImp::setGroupSize(simd, 1, 1);

        if (isSimd1(simd)) {
            EXPECT_EQ(maxNBitValue(32), kernel.KernelImp::getThreadExecutionMask());
        } else {
            EXPECT_EQ(maxNBitValue(simd), kernel.KernelImp::getThreadExecutionMask());
        }
    }
}

TEST_F(KernelImpTest, WhenSuggestingGroupSizeThenClampToMaxGroupSize) {
    DebugManagerStateRestore restorer;

    WhiteBox<KernelImmutableData> kernelInfo = {};
    NEO::KernelDescriptor descriptor;
    kernelInfo.kernelDescriptor = &descriptor;

    NEO::debugManager.flags.EnableComputeWorkSizeND.set(false);

    Mock<Module> module(device, nullptr);
    module.getMaxGroupSizeResult = 8;

    Mock<KernelImp> kernel;
    kernel.sharedState->kernelImmData = &kernelInfo;
    kernel.module = &module;
    uint32_t groupSize[3];
    kernel.KernelImp::suggestGroupSize(256, 1, 1, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(8U, groupSize[0]);
    EXPECT_EQ(1U, groupSize[1]);
    EXPECT_EQ(1U, groupSize[2]);
}

TEST_F(KernelImpTest, WhenSuggestingGroupSizeThenCacheValues) {
    DebugManagerStateRestore restorer;

    WhiteBox<KernelImmutableData> kernelInfo = {};
    NEO::KernelDescriptor descriptor;
    kernelInfo.kernelDescriptor = &descriptor;

    NEO::debugManager.flags.EnableComputeWorkSizeND.set(false);

    Mock<Module> module(device, nullptr);
    module.getMaxGroupSizeResult = 8;

    Mock<KernelImp> kernel;
    kernel.sharedState->kernelImmData = &kernelInfo;
    kernel.module = &module;
    auto &suggestGroupSizeCache = kernel.privateState.suggestGroupSizeCache;

    EXPECT_EQ(suggestGroupSizeCache.size(), 0u);
    EXPECT_EQ(kernel.getSlmTotalSize(), 0u);

    uint32_t groupSize[3];
    kernel.KernelImp::suggestGroupSize(256, 1, 1, groupSize, groupSize + 1, groupSize + 2);

    EXPECT_EQ(suggestGroupSizeCache.size(), 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].slmArgsTotalSize, 0u);
    EXPECT_EQ(suggestGroupSizeCache[0].groupSize[0], 256u);
    EXPECT_EQ(suggestGroupSizeCache[0].groupSize[1], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].groupSize[2], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[0], 8u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[1], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[2], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[0], groupSize[0]);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[1], groupSize[1]);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[2], groupSize[2]);

    kernel.KernelImp::suggestGroupSize(256, 1, 1, groupSize, groupSize + 1, groupSize + 2);

    EXPECT_EQ(suggestGroupSizeCache.size(), 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].slmArgsTotalSize, 0u);
    EXPECT_EQ(suggestGroupSizeCache[0].groupSize[0], 256u);
    EXPECT_EQ(suggestGroupSizeCache[0].groupSize[1], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].groupSize[2], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[0], 8u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[1], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[2], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[0], groupSize[0]);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[1], groupSize[1]);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[2], groupSize[2]);

    kernel.KernelImp::suggestGroupSize(2048, 1, 1, groupSize, groupSize + 1, groupSize + 2);

    EXPECT_EQ(suggestGroupSizeCache.size(), 2u);
    EXPECT_EQ(suggestGroupSizeCache[0].slmArgsTotalSize, 0u);
    EXPECT_EQ(suggestGroupSizeCache[0].groupSize[0], 256u);
    EXPECT_EQ(suggestGroupSizeCache[0].groupSize[1], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].groupSize[2], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[0], 8u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[1], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[2], 1u);
    EXPECT_EQ(suggestGroupSizeCache[1].slmArgsTotalSize, 0u);
    EXPECT_EQ(suggestGroupSizeCache[1].groupSize[0], 2048u);
    EXPECT_EQ(suggestGroupSizeCache[1].groupSize[1], 1u);
    EXPECT_EQ(suggestGroupSizeCache[1].groupSize[2], 1u);
    EXPECT_EQ(suggestGroupSizeCache[1].suggestedGroupSize[0], 8u);
    EXPECT_EQ(suggestGroupSizeCache[1].suggestedGroupSize[1], 1u);
    EXPECT_EQ(suggestGroupSizeCache[1].suggestedGroupSize[2], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[0], groupSize[0]);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[1], groupSize[1]);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[2], groupSize[2]);

    kernel.privateState.slmArgsTotalSize = 1;
    kernel.KernelImp::suggestGroupSize(2048, 1, 1, groupSize, groupSize + 1, groupSize + 2);

    EXPECT_EQ(suggestGroupSizeCache.size(), 3u);
    EXPECT_EQ(suggestGroupSizeCache[0].slmArgsTotalSize, 0u);
    EXPECT_EQ(suggestGroupSizeCache[0].groupSize[0], 256u);
    EXPECT_EQ(suggestGroupSizeCache[0].groupSize[1], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].groupSize[2], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[0], 8u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[1], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[2], 1u);
    EXPECT_EQ(suggestGroupSizeCache[1].slmArgsTotalSize, 0u);
    EXPECT_EQ(suggestGroupSizeCache[1].groupSize[0], 2048u);
    EXPECT_EQ(suggestGroupSizeCache[1].groupSize[1], 1u);
    EXPECT_EQ(suggestGroupSizeCache[1].groupSize[2], 1u);
    EXPECT_EQ(suggestGroupSizeCache[1].suggestedGroupSize[0], 8u);
    EXPECT_EQ(suggestGroupSizeCache[1].suggestedGroupSize[1], 1u);
    EXPECT_EQ(suggestGroupSizeCache[1].suggestedGroupSize[2], 1u);
    EXPECT_EQ(suggestGroupSizeCache[2].slmArgsTotalSize, 1u);
    EXPECT_EQ(suggestGroupSizeCache[2].groupSize[0], 2048u);
    EXPECT_EQ(suggestGroupSizeCache[2].groupSize[1], 1u);
    EXPECT_EQ(suggestGroupSizeCache[2].groupSize[2], 1u);
    EXPECT_EQ(suggestGroupSizeCache[2].suggestedGroupSize[0], 8u);
    EXPECT_EQ(suggestGroupSizeCache[2].suggestedGroupSize[1], 1u);
    EXPECT_EQ(suggestGroupSizeCache[2].suggestedGroupSize[2], 1u);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[0], groupSize[0]);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[1], groupSize[1]);
    EXPECT_EQ(suggestGroupSizeCache[0].suggestedGroupSize[2], groupSize[2]);
}

class KernelImpSuggestGroupSize : public DeviceFixture, public ::testing::TestWithParam<uint32_t> {
  public:
    void SetUp() override {
        DeviceFixture::setUp();
    }

    void TearDown() override {
        DeviceFixture::tearDown();
    }
};

INSTANTIATE_TEST_SUITE_P(, KernelImpSuggestGroupSize,
                         ::testing::Values(4, 7, 8, 16, 32, 192, 1024, 4097, 16000));

TEST_P(KernelImpSuggestGroupSize, WhenSuggestingGroupThenProperGroupSizeChosen) {
    DebugManagerStateRestore restorer;

    WhiteBox<KernelImmutableData> kernelInfo = {};
    NEO::KernelDescriptor descriptor;
    kernelInfo.kernelDescriptor = &descriptor;

    NEO::debugManager.flags.EnableComputeWorkSizeND.set(false);

    Mock<Module> module(device, nullptr);

    uint32_t size = GetParam();

    Mock<KernelImp> kernel;
    kernel.sharedState->kernelImmData = &kernelInfo;
    kernel.module = &module;
    uint32_t groupSize[3];
    kernel.KernelImp::suggestGroupSize(size, 1, 1, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, 1U % groupSize[1]);
    EXPECT_EQ(0U, 1U % groupSize[2]);

    kernel.KernelImp::suggestGroupSize(size, size, 1, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, size % groupSize[1]);
    EXPECT_EQ(0U, 1U % groupSize[2]);

    kernel.KernelImp::suggestGroupSize(size, size, size, groupSize, groupSize + 1,
                                       groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, size % groupSize[1]);
    EXPECT_EQ(0U, size % groupSize[2]);

    kernel.KernelImp::suggestGroupSize(size, 1, 1, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, 1U % groupSize[1]);
    EXPECT_EQ(0U, 1U % groupSize[2]);

    kernel.KernelImp::suggestGroupSize(1, size, 1, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, 1U % groupSize[0]);
    EXPECT_EQ(0U, size % groupSize[1]);
    EXPECT_EQ(0U, 1U % groupSize[2]);

    kernel.KernelImp::suggestGroupSize(1, 1, size, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, 1U % groupSize[0]);
    EXPECT_EQ(0U, 1U % groupSize[1]);
    EXPECT_EQ(0U, size % groupSize[2]);

    kernel.KernelImp::suggestGroupSize(1, size, size, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, 1U % groupSize[0]);
    EXPECT_EQ(0U, size % groupSize[1]);
    EXPECT_EQ(0U, size % groupSize[2]);

    kernel.KernelImp::suggestGroupSize(size, 1, size, groupSize, groupSize + 1, groupSize + 2);
    EXPECT_EQ(0U, size % groupSize[0]);
    EXPECT_EQ(0U, 1U % groupSize[1]);
    EXPECT_EQ(0U, size % groupSize[2]);
}

TEST_P(KernelImpSuggestGroupSize, WhenSlmSizeExceedsLocalMemorySizeAndSuggestingGroupSizeThenDebugMsgErrIsPrintedAndOutOfDeviceMemoryIsReturned) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.PrintDebugMessages.set(true);

    WhiteBox<KernelImmutableData> funcInfo = {};
    NEO::KernelDescriptor descriptor;
    funcInfo.kernelDescriptor = &descriptor;

    Mock<Module> module(device, nullptr);

    uint32_t size = GetParam();

    Mock<KernelImp> function;
    function.sharedState->kernelImmData = &funcInfo;
    function.module = &module;
    uint32_t groupSize[3];

    StreamCapture capture;
    capture.captureStderr();

    auto localMemSize = static_cast<uint32_t>(device->getNEODevice()->getDeviceInfo().localMemSize);
    funcInfo.kernelDescriptor->kernelAttributes.slmInlineSize = localMemSize + 10u;
    EXPECT_EQ(ZE_RESULT_ERROR_OUT_OF_DEVICE_MEMORY, function.KernelImp::suggestGroupSize(size, 1, 1, groupSize, groupSize + 1, groupSize + 2));

    auto output = capture.getCapturedStderr();
    const auto &slmInlineSize = funcInfo.kernelDescriptor->kernelAttributes.slmInlineSize;
    std::string expectedOutput = "Size of SLM (" + std::to_string(slmInlineSize) + ") larger than available (" + std::to_string(localMemSize) + ")\n";
    EXPECT_EQ(expectedOutput, output);

    const char *errorMsg = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getDriverHandle()->getErrorDescription(&errorMsg));
    EXPECT_EQ(0, strcmp(expectedOutput.c_str(), errorMsg));
}

TEST_F(KernelImpTest, GivenInvalidValuesWhenSettingGroupSizeThenInvalidArgumentErrorIsReturned) {
    Mock<KernelImp> kernel;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, kernel.KernelImp::setGroupSize(0U, 1U, 1U));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, kernel.KernelImp::setGroupSize(1U, 0U, 1U));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, kernel.KernelImp::setGroupSize(1U, 1U, 0U));
}

TEST_F(KernelImpTest, givenSetGroupSizeWithGreaterGroupSizeThanAllowedThenCorrectErrorCodeIsReturned) {
    WhiteBox<KernelImmutableData> kernelInfo = {};
    NEO::KernelDescriptor descriptor;
    kernelInfo.kernelDescriptor = &descriptor;

    Mock<Module> module(device, nullptr);
    Mock<KernelImp> kernel;
    kernel.sharedState->kernelImmData = &kernelInfo;
    kernel.module = &module;

    uint32_t maxGroupSizeX = static_cast<uint32_t>(device->getDeviceInfo().maxWorkItemSizes[0]);
    uint32_t maxGroupSizeY = static_cast<uint32_t>(device->getDeviceInfo().maxWorkItemSizes[1]);
    uint32_t maxGroupSizeZ = static_cast<uint32_t>(device->getDeviceInfo().maxWorkItemSizes[2]);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, kernel.KernelImp::setGroupSize(maxGroupSizeX, maxGroupSizeY, maxGroupSizeZ));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, kernel.KernelImp::setGroupSize(maxGroupSizeX + 1U, 1U, 1U));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, kernel.KernelImp::setGroupSize(1U, maxGroupSizeY + 1U, 1U));
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_GROUP_SIZE_DIMENSION, kernel.KernelImp::setGroupSize(1U, 1U, maxGroupSizeZ + 1U));
}

TEST_F(KernelImpTest, GivenNumChannelsZeroWhenSettingGroupSizeThenLocalIdsNotGenerated) {
    WhiteBox<KernelImmutableData> kernelInfo = {};
    NEO::KernelDescriptor descriptor;
    kernelInfo.kernelDescriptor = &descriptor;

    Mock<Module> module(device, nullptr);
    Mock<KernelImp> kernel;
    kernel.sharedState->kernelImmData = &kernelInfo;
    kernel.module = &module;

    kernel.KernelImp::setGroupSize(16U, 16U, 1U);
    std::vector<char> memBefore;
    {
        auto perThreadData =
            reinterpret_cast<const char *>(kernel.KernelImp::getPerThreadData());
        memBefore.assign(perThreadData,
                         perThreadData + kernel.KernelImp::getPerThreadDataSize());
    }

    kernel.KernelImp::setGroupSize(8U, 32U, 1U);
    std::vector<char> memAfter;
    {
        auto perThreadData =
            reinterpret_cast<const char *>(kernel.KernelImp::getPerThreadData());
        memAfter.assign(perThreadData,
                        perThreadData + kernel.KernelImp::getPerThreadDataSize());
    }

    EXPECT_EQ(memAfter, memBefore);
}

HWTEST_F(KernelImpTest, givenSurfaceStateHeapWhenPatchWithImplicitSurfaceCalledThenIsDebuggerActiveIsSetCorrectly) {
    static EncodeSurfaceStateArgs savedSurfaceStateArgs{};
    static size_t encodeBufferSurfaceStateCalled{};
    savedSurfaceStateArgs = {};
    encodeBufferSurfaceStateCalled = {};
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        void encodeBufferSurfaceState(EncodeSurfaceStateArgs &args) const override {
            savedSurfaceStateArgs = args;
            ++encodeBufferSurfaceStateCalled;
        }
    };

    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*this->device->getNEODevice()->getExecutionEnvironment()->rootDeviceEnvironments[0]);

    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;

    auto surfaceStateHeap = std::make_unique<uint8_t[]>(2 * sizeof(RENDER_SURFACE_STATE));
    auto crossThreadDataArrayRef = ArrayRef<uint8_t>();
    auto surfaceStateHeapArrayRef = ArrayRef<uint8_t>(surfaceStateHeap.get(), 2 * sizeof(RENDER_SURFACE_STATE));
    uintptr_t ptrToPatchInCrossThreadData = 0;
    NEO::MockGraphicsAllocation globalBuffer;
    ArgDescPointer ptr;
    ASSERT_EQ(encodeBufferSurfaceStateCalled, 0u);
    {
        patchWithImplicitSurface(crossThreadDataArrayRef, surfaceStateHeapArrayRef,
                                 ptrToPatchInCrossThreadData,
                                 globalBuffer, ptr,
                                 *neoDevice, false);
        EXPECT_EQ(encodeBufferSurfaceStateCalled, 0u);
    }
    {
        ptr.bindful = 1;
        patchWithImplicitSurface(crossThreadDataArrayRef, surfaceStateHeapArrayRef,
                                 ptrToPatchInCrossThreadData,
                                 globalBuffer, ptr,
                                 *neoDevice, false);
        ASSERT_EQ(encodeBufferSurfaceStateCalled, 1u);
        EXPECT_FALSE(savedSurfaceStateArgs.isDebuggerActive);
    }
    {
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->initDebuggerL0(neoDevice);
        patchWithImplicitSurface(crossThreadDataArrayRef, surfaceStateHeapArrayRef,
                                 ptrToPatchInCrossThreadData,
                                 globalBuffer, ptr,
                                 *neoDevice, false);
        ASSERT_EQ(encodeBufferSurfaceStateCalled, 2u);
        EXPECT_TRUE(savedSurfaceStateArgs.isDebuggerActive);
    }
}

TEST(zeKernelGetProperties, WhenGettingKernelPropertiesThenSuccessIsReturned) {
    Mock<KernelImp> kernel;

    ze_kernel_properties_t properties = {};

    auto result = zeKernelGetProperties(kernel.toHandle(), &properties);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

using KernelImpSuggestMaxCooperativeGroupCountTests = Test<KernelImpSuggestMaxCooperativeGroupCountFixture>;

HWTEST2_F(KernelImpSuggestMaxCooperativeGroupCountTests, GivenNoBarriersOrSlmUsedWhenCalculatingMaxCooperativeGroupCountThenResultIsCalculatedWithSimd, IsAtLeastXe2HpgCore) {
    auto workGroupSize = lws[0] * lws[1] * lws[2];
    auto expected = availableThreadCount / Math::divideAndRoundUp(workGroupSize, simd);
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

HWTEST_F(KernelImpSuggestMaxCooperativeGroupCountTests, GivenMultiTileWhenCalculatingMaxCooperativeGroupCountThenResultIsCalculatedWithSimd) {
    DebugManagerStateRestore restore;

    neoDevice->deviceBitfield = 0b1;
    auto baseCount = getMaxWorkGroupCount();

    debugManager.flags.EnableImplicitScaling.set(1);
    neoDevice->deviceBitfield = 0b11;

    auto countWithSubDevices = getMaxWorkGroupCount();

    auto &helper = neoDevice->getGfxCoreHelper();

    if (helper.singleTileExecImplicitScalingRequired(true)) {
        EXPECT_EQ(baseCount, countWithSubDevices);
    } else {
        EXPECT_EQ(baseCount * 2, countWithSubDevices);
    }
}

HWTEST2_F(KernelImpSuggestMaxCooperativeGroupCountTests, GivenBarriersWhenCalculatingMaxCooperativeGroupCountThenResultIsCalculatedWithRegardToBarriersCount, IsAtLeastXe2HpgCore) {
    usesBarriers = 4;
    auto expected = dssCount * (maxBarrierCount / usesBarriers);
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

HWTEST2_F(KernelImpSuggestMaxCooperativeGroupCountTests, GivenUsedSlmSizeWhenCalculatingMaxCooperativeGroupCountThenResultIsCalculatedWithRegardToUsedSlmSize, IsAtLeastXe2HpgCore) {
    usedSlm = 128 * MemoryConstants::kiloByte;
    auto expected = availableSlm / usedSlm;
    EXPECT_EQ(expected, getMaxWorkGroupCount());
}

using KernelTest = Test<DeviceFixture>;
HWTEST2_F(KernelTest, GivenInlineSamplersWhenSettingInlineSamplerThenDshIsPatched, SupportsSampler) {
    using SamplerState = typename FamilyType::SAMPLER_STATE;
    WhiteBox<::L0::KernelImmutableData> kernelImmData = {};
    NEO::KernelDescriptor descriptor;
    kernelImmData.kernelDescriptor = &descriptor;

    auto &inlineSampler = descriptor.inlineSamplers.emplace_back();
    inlineSampler.addrMode = NEO::KernelDescriptor::InlineSampler::AddrMode::repeat;
    inlineSampler.filterMode = NEO::KernelDescriptor::InlineSampler::FilterMode::nearest;
    inlineSampler.isNormalized = false;

    Mock<Module> module(device, nullptr);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    kernel.sharedState->kernelImmData = &kernelImmData;
    kernel.privateState.dynamicStateHeapData.resize(64 + sizeof(SamplerState));

    kernel.setInlineSamplers();

    auto samplerState = reinterpret_cast<const SamplerState *>(&kernel.getDynamicStateHeapDataSpan()[64U]);
    EXPECT_TRUE(samplerState->getNonNormalizedCoordinateEnable());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_WRAP, samplerState->getTcxAddressControlMode());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_WRAP, samplerState->getTcyAddressControlMode());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_WRAP, samplerState->getTczAddressControlMode());
    EXPECT_EQ(SamplerState::MIN_MODE_FILTER_NEAREST, samplerState->getMinModeFilter());
    EXPECT_EQ(SamplerState::MAG_MODE_FILTER_NEAREST, samplerState->getMagModeFilter());
}

HWTEST2_F(KernelTest, givenTwoInlineSamplersWithBindlessAddressingWhenSettingInlineSamplerThenDshIsPatchedCorrectly, SupportsSampler) {
    using SamplerState = typename FamilyType::SAMPLER_STATE;
    constexpr auto borderColorStateSize = 64u;

    WhiteBox<::L0::KernelImmutableData> kernelImmData = {};
    NEO::KernelDescriptor descriptor;
    kernelImmData.kernelDescriptor = &descriptor;

    descriptor.inlineSamplers.resize(2);

    auto &inlineSampler = descriptor.inlineSamplers[0];
    inlineSampler.addrMode = NEO::KernelDescriptor::InlineSampler::AddrMode::clampEdge;
    inlineSampler.filterMode = NEO::KernelDescriptor::InlineSampler::FilterMode::nearest;
    inlineSampler.isNormalized = false;
    inlineSampler.bindless = 0x98u;
    inlineSampler.samplerIndex = 0u;

    ASSERT_TRUE(NEO::isValidOffset(inlineSampler.bindless));

    auto &inlineSampler2 = descriptor.inlineSamplers[1];
    inlineSampler2.addrMode = NEO::KernelDescriptor::InlineSampler::AddrMode::clampBorder;
    inlineSampler2.filterMode = NEO::KernelDescriptor::InlineSampler::FilterMode::linear;
    inlineSampler2.isNormalized = false;
    inlineSampler2.bindless = 0x90u;
    inlineSampler2.samplerIndex = 1u;

    ASSERT_TRUE(NEO::isValidOffset(inlineSampler.bindless));

    Mock<Module> module(device, nullptr);
    Mock<KernelImp> kernel;
    kernel.module = &module;
    kernel.sharedState->kernelImmData = &kernelImmData;
    kernel.privateState.dynamicStateHeapData.resize(borderColorStateSize + 2 * sizeof(SamplerState));

    kernel.setInlineSamplers();

    const SamplerState *samplerState = reinterpret_cast<const SamplerState *>(&kernel.getDynamicStateHeapDataSpan()[borderColorStateSize]);
    const SamplerState *samplerState2 = reinterpret_cast<const SamplerState *>(&kernel.getDynamicStateHeapDataSpan()[sizeof(SamplerState) + borderColorStateSize]);

    EXPECT_TRUE(samplerState->getNonNormalizedCoordinateEnable());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_CLAMP, samplerState->getTcxAddressControlMode());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_CLAMP, samplerState->getTcyAddressControlMode());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_CLAMP, samplerState->getTczAddressControlMode());
    EXPECT_EQ(SamplerState::MIN_MODE_FILTER_NEAREST, samplerState->getMinModeFilter());
    EXPECT_EQ(SamplerState::MAG_MODE_FILTER_NEAREST, samplerState->getMagModeFilter());

    EXPECT_TRUE(samplerState2->getNonNormalizedCoordinateEnable());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_CLAMP_BORDER, samplerState2->getTcxAddressControlMode());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_CLAMP_BORDER, samplerState2->getTcyAddressControlMode());
    EXPECT_EQ(SamplerState::TEXTURE_COORDINATE_MODE_CLAMP_BORDER, samplerState2->getTczAddressControlMode());
    EXPECT_EQ(SamplerState::MIN_MODE_FILTER_LINEAR, samplerState2->getMinModeFilter());
    EXPECT_EQ(SamplerState::MAG_MODE_FILTER_LINEAR, samplerState2->getMagModeFilter());
}

using KernelImmutableDataBindlessTest = Test<DeviceFixture>;

HWTEST2_F(KernelImmutableDataBindlessTest, givenGlobalConstBufferAndBindlessExplicitAndImplicitArgsAndNoBindlessHeapsHelperWhenInitializeKernelImmutableDataThenSurfaceStateIsSetAndImplicitArgBindlessOffsetIsPatched, IsAtLeastXeCore) {
    HardwareInfo hwInfo = *defaultHwInfo;

    auto device = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->bindlessHeapsHelper.reset();

    static EncodeSurfaceStateArgs savedSurfaceStateArgs{};
    static size_t encodeBufferSurfaceStateCalled{};
    savedSurfaceStateArgs = {};
    encodeBufferSurfaceStateCalled = {};
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        void encodeBufferSurfaceState(EncodeSurfaceStateArgs &args) const override {
            savedSurfaceStateArgs = args;
            ++encodeBufferSurfaceStateCalled;
        }
    };

    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);

    {
        device->incRefInternal();
        MockDeviceImp deviceImp(device.get());

        uint64_t gpuAddress = 0x1200;
        void *buffer = reinterpret_cast<void *>(gpuAddress);
        size_t allocSize = 0x1100;
        auto globalConstBufferMockGA = NEO::MockGraphicsAllocation(buffer, gpuAddress, allocSize);
        auto globalConstBuffer = std::make_unique<NEO::SharedPoolAllocation>(&globalConstBufferMockGA);

        auto kernelInfo = std::make_unique<KernelInfo>();

        kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram<>::kernelName;

        kernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;

        auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
        argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
        argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
        argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x40;
        kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

        kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindless = 0x80;

        kernelInfo->kernelDescriptor.kernelAttributes.numArgsStateful = 2;
        kernelInfo->kernelDescriptor.kernelAttributes.crossThreadDataSize = 4 * sizeof(uint64_t);

        kernelInfo->kernelDescriptor.initBindlessOffsetToSurfaceState();
        const auto globalConstantsSurfaceAddressSSIndex = 1;

        auto kernelImmutableData = std::make_unique<KernelImmutableData>(&deviceImp);
        kernelImmutableData->initialize(kernelInfo.get(), &deviceImp, 0, globalConstBuffer.get(), nullptr, false);

        auto &gfxCoreHelper = device->getGfxCoreHelper();
        auto surfaceStateSize = static_cast<uint32_t>(gfxCoreHelper.getRenderSurfaceStateSize());

        EXPECT_EQ(surfaceStateSize * kernelInfo->kernelDescriptor.kernelAttributes.numArgsStateful, kernelImmutableData->getSurfaceStateHeapSize());

        auto &residencyContainer = kernelImmutableData->getResidencyContainer();
        EXPECT_EQ(1u, residencyContainer.size());
        EXPECT_EQ(1, std::count(residencyContainer.begin(), residencyContainer.end(), globalConstBuffer->getGraphicsAllocation()));

        EXPECT_EQ(1u, encodeBufferSurfaceStateCalled);
        EXPECT_EQ(allocSize, savedSurfaceStateArgs.size);
        EXPECT_EQ(gpuAddress, savedSurfaceStateArgs.graphicsAddress);

        EXPECT_EQ(ptrOffset(kernelImmutableData->getSurfaceStateHeapTemplate(), globalConstantsSurfaceAddressSSIndex * surfaceStateSize), savedSurfaceStateArgs.outMemory);
        EXPECT_EQ(globalConstBuffer->getGraphicsAllocation(), savedSurfaceStateArgs.allocation);
    }
}

HWTEST2_F(KernelImmutableDataBindlessTest, givenGlobalVarBufferAndBindlessExplicitAndImplicitArgsAndNoBindlessHeapsHelperWhenInitializeKernelImmutableDataThenSurfaceStateIsSetAndImplicitArgBindlessOffsetIsPatched, IsAtLeastXeCore) {
    HardwareInfo hwInfo = *defaultHwInfo;

    auto device = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->bindlessHeapsHelper.reset();

    static EncodeSurfaceStateArgs savedSurfaceStateArgs{};
    static size_t encodeBufferSurfaceStateCalled{};
    savedSurfaceStateArgs = {};
    encodeBufferSurfaceStateCalled = {};
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        void encodeBufferSurfaceState(EncodeSurfaceStateArgs &args) const override {
            savedSurfaceStateArgs = args;
            ++encodeBufferSurfaceStateCalled;
            NEO::GfxCoreHelperHw<FamilyType>::encodeBufferSurfaceState(args);
        }
    };

    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);

    {
        device->incRefInternal();
        MockDeviceImp deviceImp(device.get());

        uint64_t gpuAddress = 0x1200;
        void *buffer = reinterpret_cast<void *>(gpuAddress);
        size_t allocSize = 0x1100;
        auto globalVarBufferMockGA = NEO::MockGraphicsAllocation(buffer, gpuAddress, allocSize);
        auto globalVarBuffer = std::make_unique<NEO::SharedPoolAllocation>(&globalVarBufferMockGA);

        auto kernelInfo = std::make_unique<KernelInfo>();

        kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram<>::kernelName;

        kernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;

        auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
        argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
        argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
        argDescriptor.as<NEO::ArgDescPointer>().bindless = 0x40;
        kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

        kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless = 0x80;

        kernelInfo->kernelDescriptor.kernelAttributes.numArgsStateful = 2;
        kernelInfo->kernelDescriptor.kernelAttributes.crossThreadDataSize = 4 * sizeof(uint64_t);

        kernelInfo->kernelDescriptor.initBindlessOffsetToSurfaceState();
        const auto globalVariablesSurfaceAddressSSIndex = 1;

        auto kernelImmutableData = std::make_unique<KernelImmutableData>(&deviceImp);
        kernelImmutableData->initialize(kernelInfo.get(), &deviceImp, 0, nullptr, globalVarBuffer.get(), false);

        auto &gfxCoreHelper = device->getGfxCoreHelper();
        auto surfaceStateSize = static_cast<uint32_t>(gfxCoreHelper.getRenderSurfaceStateSize());

        EXPECT_EQ(surfaceStateSize * kernelInfo->kernelDescriptor.kernelAttributes.numArgsStateful, kernelImmutableData->getSurfaceStateHeapSize());

        auto &residencyContainer = kernelImmutableData->getResidencyContainer();
        EXPECT_EQ(1u, residencyContainer.size());
        EXPECT_EQ(1, std::count(residencyContainer.begin(), residencyContainer.end(), globalVarBuffer->getGraphicsAllocation()));

        EXPECT_EQ(1u, encodeBufferSurfaceStateCalled);
        EXPECT_EQ(allocSize, savedSurfaceStateArgs.size);
        EXPECT_EQ(gpuAddress, savedSurfaceStateArgs.graphicsAddress);

        EXPECT_EQ(ptrOffset(kernelImmutableData->getSurfaceStateHeapTemplate(), globalVariablesSurfaceAddressSSIndex * surfaceStateSize), savedSurfaceStateArgs.outMemory);
        EXPECT_EQ(globalVarBuffer->getGraphicsAllocation(), savedSurfaceStateArgs.allocation);
    }
}

HWTEST2_F(KernelImmutableDataBindlessTest, givenGlobalConstBufferAndBindlessExplicitAndImplicitArgsAndBindlessHeapsHelperWhenInitializeKernelImmutableDataThenSurfaceStateIsSetAndImplicitArgBindlessOffsetIsPatched, IsAtLeastXeCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    HardwareInfo hwInfo = *defaultHwInfo;

    auto device = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
    auto mockHelper = std::make_unique<MockBindlesHeapsHelper>(device.get(),
                                                               device->getNumGenericSubDevices() > 1);
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->bindlessHeapsHelper.reset(mockHelper.release());

    static EncodeSurfaceStateArgs savedSurfaceStateArgs{};
    static size_t encodeBufferSurfaceStateCalled{};
    savedSurfaceStateArgs = {};
    encodeBufferSurfaceStateCalled = {};
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        void encodeBufferSurfaceState(EncodeSurfaceStateArgs &args) const override {
            savedSurfaceStateArgs = args;
            ++encodeBufferSurfaceStateCalled;
            NEO::GfxCoreHelperHw<FamilyType>::encodeBufferSurfaceState(args);
        }
    };

    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);

    {
        device->incRefInternal();
        MockDeviceImp deviceImp(device.get());

        uint64_t gpuAddress = 0x1200;
        void *buffer = reinterpret_cast<void *>(gpuAddress);
        size_t allocSize = 0x1100;
        auto globalConstBufferMockGA = NEO::MockGraphicsAllocation(buffer, gpuAddress, allocSize);
        auto globalConstBuffer = std::make_unique<NEO::SharedPoolAllocation>(&globalConstBufferMockGA);

        auto kernelInfo = std::make_unique<KernelInfo>();

        kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram<>::kernelName;

        kernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;

        auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
        argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
        argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
        argDescriptor.as<NEO::ArgDescPointer>().bindless = 4;
        kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

        NEO::CrossThreadDataOffset globalConstSurfaceAddressBindlessOffset = 8;
        kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.globalConstantsSurfaceAddress.bindless = globalConstSurfaceAddressBindlessOffset;

        kernelInfo->kernelDescriptor.kernelAttributes.numArgsStateful = 2;
        kernelInfo->kernelDescriptor.kernelAttributes.crossThreadDataSize = 4 * sizeof(uint64_t);

        kernelInfo->kernelDescriptor.initBindlessOffsetToSurfaceState();

        auto kernelImmutableData = std::make_unique<KernelImmutableData>(&deviceImp);
        kernelImmutableData->initialize(kernelInfo.get(), &deviceImp, 0, globalConstBuffer.get(), nullptr, false);

        auto &gfxCoreHelper = device->getGfxCoreHelper();
        auto surfaceStateSize = static_cast<uint32_t>(gfxCoreHelper.getRenderSurfaceStateSize());

        EXPECT_EQ(surfaceStateSize * kernelInfo->kernelDescriptor.kernelAttributes.numArgsStateful, kernelImmutableData->getSurfaceStateHeapSize());

        auto &residencyContainer = kernelImmutableData->getResidencyContainer();
        EXPECT_EQ(1u, residencyContainer.size());
        EXPECT_EQ(1, std::count(residencyContainer.begin(), residencyContainer.end(), globalConstBuffer->getGraphicsAllocation()));
        EXPECT_EQ(0, std::count(residencyContainer.begin(), residencyContainer.end(), globalConstBuffer->getGraphicsAllocation()->getBindlessInfo().heapAllocation));

        auto crossThreadData = kernelImmutableData->getCrossThreadDataTemplate();
        auto patchLocation = reinterpret_cast<const uint32_t *>(ptrOffset(crossThreadData, globalConstSurfaceAddressBindlessOffset));
        auto patchValue = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(globalConstBuffer->getGraphicsAllocation()->getBindlessInfo().surfaceStateOffset));

        EXPECT_EQ(patchValue, *patchLocation);

        EXPECT_EQ(1u, encodeBufferSurfaceStateCalled);
        EXPECT_EQ(allocSize, savedSurfaceStateArgs.size);
        EXPECT_EQ(gpuAddress, savedSurfaceStateArgs.graphicsAddress);

        EXPECT_NE(globalConstBuffer->getGraphicsAllocation()->getBindlessInfo().ssPtr, savedSurfaceStateArgs.outMemory);

        const auto surfState = reinterpret_cast<RENDER_SURFACE_STATE *>(globalConstBuffer->getGraphicsAllocation()->getBindlessInfo().ssPtr);
        ASSERT_NE(nullptr, surfState);
        EXPECT_EQ(gpuAddress, surfState->getSurfaceBaseAddress());
        EXPECT_EQ(globalConstBuffer->getGraphicsAllocation(), savedSurfaceStateArgs.allocation);
    }
}

HWTEST2_F(KernelImmutableDataBindlessTest, givenGlobalVarBufferAndBindlessExplicitAndImplicitArgsAndBindlessHeapsHelperWhenInitializeKernelImmutableDataThenSurfaceStateIsSetAndImplicitArgBindlessOffsetIsPatched, IsAtLeastXeCore) {
    using RENDER_SURFACE_STATE = typename FamilyType::RENDER_SURFACE_STATE;
    HardwareInfo hwInfo = *defaultHwInfo;

    auto device = std::unique_ptr<NEO::MockDevice>(NEO::MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, 0));
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->memoryOperationsInterface = std::make_unique<MockMemoryOperations>();
    auto mockHelper = std::make_unique<MockBindlesHeapsHelper>(device.get(),
                                                               device->getNumGenericSubDevices() > 1);
    device->getExecutionEnvironment()->rootDeviceEnvironments[device->getRootDeviceIndex()]->bindlessHeapsHelper.reset(mockHelper.release());

    static EncodeSurfaceStateArgs savedSurfaceStateArgs{};
    static size_t encodeBufferSurfaceStateCalled{};
    savedSurfaceStateArgs = {};
    encodeBufferSurfaceStateCalled = {};
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        void encodeBufferSurfaceState(EncodeSurfaceStateArgs &args) const override {
            savedSurfaceStateArgs = args;
            ++encodeBufferSurfaceStateCalled;
            NEO::GfxCoreHelperHw<FamilyType>::encodeBufferSurfaceState(args);
        }
    };

    RAIIGfxCoreHelperFactory<MockGfxCoreHelper> raii(*device->getExecutionEnvironment()->rootDeviceEnvironments[0]);

    {
        device->incRefInternal();
        MockDeviceImp deviceImp(device.get());

        uint64_t gpuAddress = 0x1200;
        void *buffer = reinterpret_cast<void *>(gpuAddress);
        size_t allocSize = 0x1100;
        auto globalVarBufferMockGA = NEO::MockGraphicsAllocation(buffer, gpuAddress, allocSize);
        auto globalVarBuffer = std::make_unique<NEO::SharedPoolAllocation>(&globalVarBufferMockGA);

        auto kernelInfo = std::make_unique<KernelInfo>();

        kernelInfo->kernelDescriptor.kernelMetadata.kernelName = ZebinTestData::ValidEmptyProgram<>::kernelName;

        kernelInfo->kernelDescriptor.kernelAttributes.bufferAddressingMode = NEO::KernelDescriptor::BindlessAndStateless;

        auto argDescriptor = NEO::ArgDescriptor(NEO::ArgDescriptor::argTPointer);
        argDescriptor.as<NEO::ArgDescPointer>() = NEO::ArgDescPointer();
        argDescriptor.as<NEO::ArgDescPointer>().bindful = NEO::undefined<NEO::SurfaceStateHeapOffset>;
        argDescriptor.as<NEO::ArgDescPointer>().bindless = 4;
        kernelInfo->kernelDescriptor.payloadMappings.explicitArgs.push_back(argDescriptor);

        NEO::CrossThreadDataOffset globalVariablesSurfaceAddressBindlessOffset = 8;
        kernelInfo->kernelDescriptor.payloadMappings.implicitArgs.globalVariablesSurfaceAddress.bindless = globalVariablesSurfaceAddressBindlessOffset;

        kernelInfo->kernelDescriptor.kernelAttributes.numArgsStateful = 2;
        kernelInfo->kernelDescriptor.kernelAttributes.crossThreadDataSize = 4 * sizeof(uint64_t);

        kernelInfo->kernelDescriptor.initBindlessOffsetToSurfaceState();

        auto kernelImmutableData = std::make_unique<KernelImmutableData>(&deviceImp);
        kernelImmutableData->initialize(kernelInfo.get(), &deviceImp, 0, nullptr, globalVarBuffer.get(), false);

        auto &gfxCoreHelper = device->getGfxCoreHelper();
        auto surfaceStateSize = static_cast<uint32_t>(gfxCoreHelper.getRenderSurfaceStateSize());

        EXPECT_EQ(surfaceStateSize * kernelInfo->kernelDescriptor.kernelAttributes.numArgsStateful, kernelImmutableData->getSurfaceStateHeapSize());

        auto &residencyContainer = kernelImmutableData->getResidencyContainer();
        EXPECT_EQ(1u, residencyContainer.size());
        EXPECT_EQ(1, std::count(residencyContainer.begin(), residencyContainer.end(), globalVarBuffer->getGraphicsAllocation()));
        EXPECT_EQ(0, std::count(residencyContainer.begin(), residencyContainer.end(), globalVarBuffer->getGraphicsAllocation()->getBindlessInfo().heapAllocation));

        auto crossThreadData = kernelImmutableData->getCrossThreadDataTemplate();
        auto patchLocation = reinterpret_cast<const uint32_t *>(ptrOffset(crossThreadData, globalVariablesSurfaceAddressBindlessOffset));
        auto patchValue = gfxCoreHelper.getBindlessSurfaceExtendedMessageDescriptorValue(static_cast<uint32_t>(globalVarBuffer->getGraphicsAllocation()->getBindlessInfo().surfaceStateOffset));

        EXPECT_EQ(patchValue, *patchLocation);

        EXPECT_EQ(1u, encodeBufferSurfaceStateCalled);
        EXPECT_EQ(allocSize, savedSurfaceStateArgs.size);
        EXPECT_EQ(gpuAddress, savedSurfaceStateArgs.graphicsAddress);

        EXPECT_NE(globalVarBuffer->getGraphicsAllocation()->getBindlessInfo().ssPtr, savedSurfaceStateArgs.outMemory);

        const auto surfState = reinterpret_cast<RENDER_SURFACE_STATE *>(globalVarBuffer->getGraphicsAllocation()->getBindlessInfo().ssPtr);
        ASSERT_NE(nullptr, surfState);
        EXPECT_EQ(gpuAddress, surfState->getSurfaceBaseAddress());
        EXPECT_EQ(globalVarBuffer->getGraphicsAllocation(), savedSurfaceStateArgs.allocation);
    }
}

TEST_F(KernelImpTest, GivenGroupSizeRequiresSwLocalIdsGenerationWhenNextGroupSizeDoesNotRequireSwLocalIdsGenerationThenResetPerThreadDataSizes) {
    Mock<Module> module(device, nullptr);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;

    WhiteBox<::L0::KernelImmutableData> kernelInfo = {};
    NEO::KernelDescriptor descriptor;
    kernelInfo.kernelDescriptor = &descriptor;
    kernelInfo.kernelDescriptor->kernelAttributes.numLocalIdChannels = 3;
    kernel.sharedState->kernelImmData = &kernelInfo;

    kernel.enableForcingOfGenerateLocalIdByHw = true;
    kernel.forceGenerateLocalIdByHw = false;

    kernel.KernelImp::setGroupSize(11, 11, 1);

    EXPECT_NE(0u, kernel.getPerThreadDataSizeForWholeThreadGroup());
    EXPECT_NE(0u, kernel.getPerThreadDataSize());

    kernel.forceGenerateLocalIdByHw = true;
    kernel.KernelImp::setGroupSize(8, 1, 1);

    EXPECT_EQ(0u, kernel.getPerThreadDataSizeForWholeThreadGroup());
    EXPECT_EQ(0u, kernel.getPerThreadDataSize());
}

TEST_F(KernelImpTest, GivenGroupSizeRequiresSwLocalIdsGenerationWhenKernelSpecifiesRequiredWalkOrderThenUseCorrectOrderToGenerateLocalIds) {
    Mock<Module> module(device, nullptr);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;

    auto grfSize = device->getHwInfo().capabilityTable.grfSize;

    WhiteBox<::L0::KernelImmutableData> kernelInfo = {};
    NEO::KernelDescriptor descriptor;
    kernelInfo.kernelDescriptor = &descriptor;
    kernelInfo.kernelDescriptor->kernelAttributes.numLocalIdChannels = 3;
    kernelInfo.kernelDescriptor->kernelAttributes.numGrfRequired = GrfConfig::defaultGrfNumber;
    kernelInfo.kernelDescriptor->kernelAttributes.flags.requiresWorkgroupWalkOrder = true;
    kernelInfo.kernelDescriptor->kernelAttributes.workgroupWalkOrder[0] = 2;
    kernelInfo.kernelDescriptor->kernelAttributes.workgroupWalkOrder[1] = 1;
    kernelInfo.kernelDescriptor->kernelAttributes.workgroupWalkOrder[2] = 0;
    kernelInfo.kernelDescriptor->kernelAttributes.simdSize = 32;

    kernel.sharedState->kernelImmData = &kernelInfo;

    kernel.enableForcingOfGenerateLocalIdByHw = true;
    kernel.forceGenerateLocalIdByHw = false;

    kernel.KernelImp::setGroupSize(12, 12, 1);

    uint32_t perThreadSizeNeeded = kernel.getPerThreadDataSizeForWholeThreadGroup();
    auto testPerThreadDataBuffer = static_cast<uint8_t *>(alignedMalloc(perThreadSizeNeeded, 32));

    std::array<uint8_t, 3> walkOrder{2, 1, 0};

    NEO::generateLocalIDs(
        testPerThreadDataBuffer,
        static_cast<uint16_t>(32),
        std::array<uint16_t, 3>{{static_cast<uint16_t>(12),
                                 static_cast<uint16_t>(12),
                                 static_cast<uint16_t>(1)}},
        walkOrder,
        false, grfSize, GrfConfig::defaultGrfNumber, device->getNEODevice()->getRootDeviceEnvironment());

    EXPECT_EQ(0, memcmp(testPerThreadDataBuffer, kernel.KernelImp::getPerThreadData(), perThreadSizeNeeded));

    alignedFree(testPerThreadDataBuffer);
}

TEST_F(KernelImpTest, givenHeaplessAndLocalDispatchEnabledWheSettingGroupSizeThenGetMaxWgCountPerTileCalculated) {
    Mock<Module> module(device, nullptr);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;

    kernel.sharedState->heaplessEnabled = false;
    kernel.sharedState->localDispatchSupport = false;
    kernel.setGroupSize(128, 1, 1);

    EXPECT_EQ(0u, kernel.sharedState->maxWgCountPerTileCcs);
    EXPECT_EQ(0u, kernel.sharedState->maxWgCountPerTileRcs);
    EXPECT_EQ(0u, kernel.sharedState->maxWgCountPerTileCooperative);

    kernel.sharedState->heaplessEnabled = true;
    kernel.setGroupSize(64, 2, 1);

    EXPECT_EQ(0u, kernel.sharedState->maxWgCountPerTileCcs);
    EXPECT_EQ(0u, kernel.sharedState->maxWgCountPerTileRcs);
    EXPECT_EQ(0u, kernel.sharedState->maxWgCountPerTileCooperative);

    kernel.sharedState->localDispatchSupport = true;
    kernel.setGroupSize(32, 4, 1);

    EXPECT_NE(0u, kernel.sharedState->maxWgCountPerTileCcs);
    EXPECT_EQ(0u, kernel.sharedState->maxWgCountPerTileRcs);
    EXPECT_EQ(0u, kernel.sharedState->maxWgCountPerTileCooperative);

    kernel.sharedState->rcsAvailable = true;
    kernel.setGroupSize(16, 8, 1);

    EXPECT_NE(0u, kernel.sharedState->maxWgCountPerTileCcs);
    EXPECT_NE(0u, kernel.sharedState->maxWgCountPerTileRcs);
    EXPECT_EQ(0u, kernel.sharedState->maxWgCountPerTileCooperative);

    kernel.sharedState->cooperativeSupport = true;
    kernel.setGroupSize(8, 8, 2);

    EXPECT_NE(0u, kernel.sharedState->maxWgCountPerTileCcs);
    EXPECT_NE(0u, kernel.sharedState->maxWgCountPerTileRcs);
    EXPECT_NE(0u, kernel.sharedState->maxWgCountPerTileCooperative);
}

TEST_F(KernelImpTest, givenCorrectEngineTypeWhenGettingMaxWgCountPerTileThenReturnActualValue) {
    Mock<Module> module(device, nullptr);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;

    kernel.sharedState->maxWgCountPerTileCcs = 4;
    kernel.sharedState->maxWgCountPerTileRcs = 2;
    kernel.sharedState->maxWgCountPerTileCooperative = 100;

    EXPECT_EQ(4u, kernel.getMaxWgCountPerTile(NEO::EngineGroupType::compute));
    EXPECT_EQ(2u, kernel.getMaxWgCountPerTile(NEO::EngineGroupType::renderCompute));
    EXPECT_EQ(100u, kernel.getMaxWgCountPerTile(NEO::EngineGroupType::cooperativeCompute));
}

using KernelArgumentInfoTests = Test<ModuleImmutableDataFixture>;

TEST_F(KernelArgumentInfoTests, givenKernelWhenGetArgumentSizeCalledWithInvalidArgsThenReturnFailure) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    mockKernelImmData->resizeExplicitArgs(1);
    kernel->initialize(&desc);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, kernel->getArgumentSize(0, nullptr));
    uint32_t argSize = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_INDEX, kernel->getArgumentSize(1, &argSize));
}

TEST_F(KernelArgumentInfoTests, givenKernelWhenGetArgumentSizeCalledThenReturnCorrectSizeAndStatus) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();

    auto ptrByValueArg = ArgDescriptor(ArgDescriptor::argTValue);
    ptrByValueArg.as<ArgDescValue>().elements.push_back(ArgDescValue::Element{0u, 100u});

    auto ptrArg = ArgDescriptor(ArgDescriptor::argTPointer);
    ptrArg.as<ArgDescPointer>().pointerSize = 8u;

    auto argDescriptorSampler = NEO::ArgDescriptor(NEO::ArgDescriptor::argTSampler);
    argDescriptorSampler.as<NEO::ArgDescSampler>().size = 10u;

    mockKernelImmData->mockKernelDescriptor->payloadMappings.explicitArgs.push_back(ptrByValueArg);
    mockKernelImmData->mockKernelDescriptor->payloadMappings.explicitArgs.push_back(ptrArg);
    mockKernelImmData->mockKernelDescriptor->payloadMappings.explicitArgs.push_back(ArgDescriptor(ArgDescriptor::argTImage));
    mockKernelImmData->mockKernelDescriptor->payloadMappings.explicitArgs.push_back(argDescriptorSampler);
    mockKernelImmData->mockKernelDescriptor->payloadMappings.explicitArgs.push_back(ArgDescriptor(ArgDescriptor::argTUnknown));
    mockKernelImmData->mockKernelDescriptor->payloadMappings.explicitArgs.push_back(ArgDescriptor(ArgDescriptor::argTValue));
    kernel->initialize(&desc);

    uint32_t argSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getArgumentSize(0, &argSize));
    EXPECT_EQ(100u, argSize);

    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getArgumentSize(1, &argSize));
    EXPECT_EQ(8u, argSize);

    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getArgumentSize(2, &argSize));
    EXPECT_EQ(sizeof(ze_image_handle_t), argSize);

    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getArgumentSize(3, &argSize));
    EXPECT_EQ(10u, argSize);

    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getArgumentSize(4, &argSize));
    EXPECT_EQ(0u, argSize);

    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getArgumentSize(5, &argSize));
    EXPECT_EQ(0u, argSize);
}

TEST_F(KernelArgumentInfoTests, givenKernelWhenGetArgumentTypeCalledWithInvalidArgsThenReturnFailure) {
    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    mockKernelImmData->resizeExplicitArgs(1);
    kernel->initialize(&desc);

    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_NULL_POINTER, kernel->getArgumentType(0, nullptr, nullptr));
    uint32_t argSize = 0;
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_KERNEL_ARGUMENT_INDEX, kernel->getArgumentType(1, &argSize, nullptr));
    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, kernel->getArgumentType(0, &argSize, nullptr));
}

TEST_F(KernelArgumentInfoTests, givenKernelWhenGetArgumentTypeCalledThenReturnCorrectTypeAndStatus) {
    constexpr ConstStringRef argType = "uint32_t";
    uint32_t perHwThreadPrivateMemorySizeRequested = 32u;

    std::unique_ptr<MockImmutableData> mockKernelImmData =
        std::make_unique<MockImmutableData>(perHwThreadPrivateMemorySizeRequested);

    createModuleFromMockBinary(perHwThreadPrivateMemorySizeRequested, false, mockKernelImmData.get());
    std::unique_ptr<ModuleImmutableDataFixture::MockKernel> kernel;
    kernel = std::make_unique<ModuleImmutableDataFixture::MockKernel>(module.get());
    ze_kernel_desc_t desc = {};
    desc.pKernelName = kernelName.c_str();
    mockKernelImmData->resizeExplicitArgs(1);

    ArgTypeMetadataExtended metadata;
    metadata.type = argType.data();
    mockKernelImmData->mockKernelDescriptor->explicitArgsExtendedMetadata.push_back(metadata);
    kernel->initialize(&desc);

    uint32_t argSize = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getArgumentType(0, &argSize, nullptr));
    EXPECT_EQ(argType.size() + 1, argSize);
    auto data = new char[argSize];
    memset(data, 0, argSize);

    // Do not copy if passed size is lower than required
    argSize = 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getArgumentType(0, &argSize, data));
    EXPECT_NE(0, memcmp(argType.data(), data, 1));

    EXPECT_EQ(ZE_RESULT_SUCCESS, kernel->getArgumentType(0, &argSize, data));
    EXPECT_EQ(0, memcmp(argType.data(), data, argSize));
    delete[] data;
}

TEST_F(KernelImpTest, givenDefaultGroupSizeWhenGetGroupSizeCalledThenReturnDefaultValues) {
    Mock<Module> module(device, nullptr);
    Mock<::L0::KernelImp> kernel;
    kernel.module = &module;
    ze_kernel_desc_t kernelDesc{ZE_STRUCTURE_TYPE_KERNEL_DESC};
    kernel.initialize(&kernelDesc);

    auto groupSize = kernel.getGroupSize();

    EXPECT_EQ(1u, groupSize[0]);
    EXPECT_EQ(1u, groupSize[1]);
    EXPECT_EQ(1u, groupSize[2]);
}

} // namespace ult
} // namespace L0
