/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/hw_walk_order.h"
#include "shared/source/helpers/per_thread_data.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/vec.h"
#include "shared/source/kernel/implicit_args.h"
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

uint32_t getGrfSize(uint32_t simd, uint32_t grfSize) {
    if (simd == 1u) {
        return 3 * sizeof(uint16_t);
    }
    return grfSize;
}

uint32_t getSizeForImplicitArgsPatching(const ImplicitArgs *pImplicitArgs, const KernelDescriptor &kernelDescriptor, const HardwareInfo &hardwareInfo) {
    if (!pImplicitArgs) {
        return 0;
    }
    auto implicitArgsSize = static_cast<uint32_t>(sizeof(NEO::ImplicitArgs));
    auto patchImplicitArgsBufferInCrossThread = NEO::isValidOffset<>(kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer);
    if (patchImplicitArgsBufferInCrossThread) {
        return alignUp(implicitArgsSize, MemoryConstants::cacheLineSize);
    } else {
        auto simdSize = pImplicitArgs->simdWidth;
        auto grfSize = NEO::ImplicitArgsHelper::getGrfSize(simdSize, hardwareInfo.capabilityTable.grfSize);
        Vec3<size_t> localWorkSize = {pImplicitArgs->localSizeX, pImplicitArgs->localSizeY, pImplicitArgs->localSizeZ};
        auto itemsInGroup = Math::computeTotalElementsCount(localWorkSize);
        uint32_t localIdsSizeNeeded =
            alignUp(static_cast<uint32_t>(NEO::PerThreadDataHelper::getPerThreadDataSizeTotal(
                        simdSize, grfSize, 3u, itemsInGroup)),
                    MemoryConstants::cacheLineSize);
        return implicitArgsSize + localIdsSizeNeeded;
    }
}

void *patchImplicitArgs(void *ptrToPatch, const ImplicitArgs &implicitArgs, const KernelDescriptor &kernelDescriptor, const HardwareInfo &hardwareInfo, std::optional<std::pair<bool, uint32_t>> hwGenerationOfLocalIdsParams) {

    auto totalSizeToProgram = getSizeForImplicitArgsPatching(&implicitArgs, kernelDescriptor, hardwareInfo);
    auto retVal = ptrOffset(ptrToPatch, totalSizeToProgram);

    auto patchImplicitArgsBufferInCrossThread = NEO::isValidOffset<>(kernelDescriptor.payloadMappings.implicitArgs.implicitArgsBuffer);
    if (!patchImplicitArgsBufferInCrossThread) {
        auto simdSize = implicitArgs.simdWidth;
        auto grfSize = getGrfSize(simdSize, hardwareInfo.capabilityTable.grfSize);
        auto dimensionOrder = getDimensionOrderForLocalIds(kernelDescriptor.kernelAttributes.workgroupDimensionsOrder, hwGenerationOfLocalIdsParams);

        NEO::generateLocalIDs(
            ptrToPatch,
            simdSize,
            std::array<uint16_t, 3>{{static_cast<uint16_t>(implicitArgs.localSizeX),
                                     static_cast<uint16_t>(implicitArgs.localSizeY),
                                     static_cast<uint16_t>(implicitArgs.localSizeZ)}},
            dimensionOrder,
            false, grfSize);
        auto sizeForLocalIdsProgramming = totalSizeToProgram - sizeof(NEO::ImplicitArgs);
        ptrToPatch = ptrOffset(ptrToPatch, sizeForLocalIdsProgramming);
    }
    memcpy_s(ptrToPatch, sizeof(NEO::ImplicitArgs), &implicitArgs, sizeof(NEO::ImplicitArgs));

    return retVal;
}
} // namespace ImplicitArgsHelper
} // namespace NEO
