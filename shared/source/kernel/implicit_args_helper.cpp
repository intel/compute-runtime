/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/implicit_args_helper.h"

#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_walk_order.h"
#include "shared/source/helpers/per_thread_data.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/vec.h"
#include "shared/source/kernel/kernel_descriptor.h"

namespace NEO {
namespace ImplicitArgsHelper {
std::array<uint8_t, 3> getDimensionOrderForLocalIds(const uint8_t *workgroupDimensionsOrder, std::optional<std::pair<bool, uint32_t>> hwGenerationOfLocalIdsParams) {
    auto localIdsGeneratedByRuntime = !hwGenerationOfLocalIdsParams.has_value() || hwGenerationOfLocalIdsParams.value().first;

    if (localIdsGeneratedByRuntime) {
        UNRECOVERABLE_IF(!workgroupDimensionsOrder);
        return {{
            workgroupDimensionsOrder[0],
            workgroupDimensionsOrder[1],
            workgroupDimensionsOrder[2],
        }};
    }

    auto walkOrderForHwGenerationOfLocalIds = hwGenerationOfLocalIdsParams.value().second;
    UNRECOVERABLE_IF(walkOrderForHwGenerationOfLocalIds >= HwWalkOrderHelper::walkOrderPossibilties);
    return HwWalkOrderHelper::compatibleDimensionOrders[walkOrderForHwGenerationOfLocalIds];
}

uint32_t getGrfSize(uint32_t simd) {
    if (isSimd1(simd)) {
        return 3 * sizeof(uint16_t);
    }
    return 32u;
}

uint32_t getSizeForImplicitArgsStruct(const ImplicitArgs *pImplicitArgs, const KernelDescriptor &kernelDescriptor, bool isHwLocalIdGeneration, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (!pImplicitArgs) {
        return 0;
    }
    auto implicitArgsSize = static_cast<uint32_t>(ImplicitArgs::getSize());
    auto patchImplicitArgsBufferInCrossThread = NEO::isValidOffset<>(kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer);
    if (patchImplicitArgsBufferInCrossThread) {
        return alignUp(implicitArgsSize, MemoryConstants::cacheLineSize);
    } else {
        return implicitArgsSize;
    }
}

uint32_t getSizeForImplicitArgsPatching(const ImplicitArgs *pImplicitArgs, const KernelDescriptor &kernelDescriptor, bool isHwLocalIdGeneration, const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (!pImplicitArgs) {
        return 0;
    }
    auto implicitArgsStructSize = getSizeForImplicitArgsStruct(pImplicitArgs, kernelDescriptor, isHwLocalIdGeneration, rootDeviceEnvironment);
    auto patchImplicitArgsBufferInCrossThread = NEO::isValidOffset<>(kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer);
    uint32_t localIdsSize = 0;
    if (false == patchImplicitArgsBufferInCrossThread) {
        auto simdSize = pImplicitArgs->simdWidth;
        auto grfCount = kernelDescriptor.kernelAttributes.numGrfRequired;
        auto grfSize = NEO::ImplicitArgsHelper::getGrfSize(simdSize);
        Vec3<size_t> localWorkSize = {pImplicitArgs->localSizeX, pImplicitArgs->localSizeY, pImplicitArgs->localSizeZ};
        auto itemsInGroup = Math::computeTotalElementsCount(localWorkSize);
        localIdsSize = static_cast<uint32_t>(NEO::PerThreadDataHelper::getPerThreadDataSizeTotal(simdSize, grfSize, grfCount, 3u, itemsInGroup, isHwLocalIdGeneration, rootDeviceEnvironment));
        localIdsSize = alignUp(localIdsSize, MemoryConstants::cacheLineSize);
    }
    return implicitArgsStructSize + localIdsSize;
}

void *patchImplicitArgs(void *ptrToPatch, const ImplicitArgs &implicitArgs, const KernelDescriptor &kernelDescriptor, std::optional<std::pair<bool, uint32_t>> hwGenerationOfLocalIdsParams, const RootDeviceEnvironment &rootDeviceEnvironment, void **outImplicitArgsAddress) {
    auto localIdsGeneratedByHw = hwGenerationOfLocalIdsParams.has_value() ? hwGenerationOfLocalIdsParams.value().first : false;
    auto totalSizeToProgram = getSizeForImplicitArgsPatching(&implicitArgs, kernelDescriptor, localIdsGeneratedByHw, rootDeviceEnvironment);
    auto retVal = ptrOffset(ptrToPatch, totalSizeToProgram);

    auto patchImplicitArgsBufferInCrossThread = NEO::isValidOffset<>(kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer);
    if (!patchImplicitArgsBufferInCrossThread) {
        auto simdSize = implicitArgs.simdWidth;
        auto grfSize = getGrfSize(simdSize);
        auto grfCount = kernelDescriptor.kernelAttributes.numGrfRequired;
        auto dimensionOrder = getDimensionOrderForLocalIds(kernelDescriptor.kernelAttributes.workgroupDimensionsOrder, hwGenerationOfLocalIdsParams);

        NEO::generateLocalIDs(
            ptrToPatch,
            simdSize,
            std::array<uint16_t, 3>{{static_cast<uint16_t>(implicitArgs.localSizeX),
                                     static_cast<uint16_t>(implicitArgs.localSizeY),
                                     static_cast<uint16_t>(implicitArgs.localSizeZ)}},
            dimensionOrder,
            false, grfSize, grfCount, rootDeviceEnvironment);
        auto sizeForLocalIdsProgramming = totalSizeToProgram - ImplicitArgs::getSize();
        ptrToPatch = ptrOffset(ptrToPatch, sizeForLocalIdsProgramming);
    }

    if (outImplicitArgsAddress) {
        *outImplicitArgsAddress = ptrToPatch;
    }

    memcpy_s(ptrToPatch, ImplicitArgs::getSize(), &implicitArgs, ImplicitArgs::getSize());

    return retVal;
}
} // namespace ImplicitArgsHelper
} // namespace NEO
