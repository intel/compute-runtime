/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void MemorySynchronizationCommands<Family>::setAdditionalSynchronization(void *&commandsBuffer, uint64_t gpuAddress, NEO::FenceType fenceType, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using MI_MEM_FENCE = typename Family::MI_MEM_FENCE;
    using MI_SEMAPHORE_WAIT = typename Family::MI_SEMAPHORE_WAIT;
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const bool globalFenceRequired = (fenceType == FenceType::acquire)
                                         ? productHelper.isAcquireGlobalFenceInDirectSubmissionRequired(hwInfo)
                                         : productHelper.isReleaseGlobalFenceInCommandStreamRequired(hwInfo);
    auto programGlobalFenceAsMiMemFenceCommandInCommandStream = globalFenceRequired ? AdditionalSynchronizationType::fence : AdditionalSynchronizationType::none;
    if (debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() != -1) {
        programGlobalFenceAsMiMemFenceCommandInCommandStream = static_cast<AdditionalSynchronizationType>(debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get());
    }
    if (programGlobalFenceAsMiMemFenceCommandInCommandStream == AdditionalSynchronizationType::fence) {
        MI_MEM_FENCE miMemFence = Family::cmdInitMemFence;
        if (fenceType == NEO::FenceType::acquire) {
            miMemFence.setFenceType(Family::MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_ACQUIRE_FENCE);
        } else {
            miMemFence.setFenceType(Family::MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE_FENCE);
        }
        *reinterpret_cast<MI_MEM_FENCE *>(commandsBuffer) = miMemFence;
        commandsBuffer = ptrOffset(commandsBuffer, sizeof(MI_MEM_FENCE));
    } else if (programGlobalFenceAsMiMemFenceCommandInCommandStream == AdditionalSynchronizationType::semaphore) {
        EncodeSemaphore<Family>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(commandsBuffer),
                                                        gpuAddress,
                                                        EncodeSemaphore<Family>::invalidHardwareTag,
                                                        MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                        false, true, false, false, false, false);
        commandsBuffer = ptrOffset(commandsBuffer, EncodeSemaphore<Family>::getSizeMiSemaphoreWait());
    }
}
