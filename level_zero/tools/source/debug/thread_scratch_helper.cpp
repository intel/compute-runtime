/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/debug/debug_session_imp.h"

#include "implicit_args.h"

namespace L0 {

ze_result_t DebugSessionImp::getScratchRenderSurfaceStateAddressV1(EuThread::ThreadId threadId, uint64_t *result) {
    const NEO::HardwareInfo &hwInfo = connectedDevice->getHwInfo();
    const uint32_t regSize = std::max(getRegisterSize(ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU), hwInfo.capabilityTable.grfSize);
    std::vector<uint32_t> r0(regSize / sizeof(uint32_t));
    ze_result_t ret = readRegistersImp(threadId, ZET_DEBUG_REGSET_TYPE_GRF_INTEL_GPU, 0, 1, r0.data());
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    uint64_t implicitArgsGpuVaHigh = (static_cast<uint64_t>(r0[15]) << 32);
    uint64_t implicitArgsGpuVaLow = static_cast<uint64_t>(r0[14]);
    auto implicitArgsGpuVa = implicitArgsGpuVaLow | implicitArgsGpuVaHigh;
    if (!implicitArgsGpuVa) {
        return ZE_RESULT_SUCCESS;
    }

    NEO::ImplicitArgs implicitArgs;
    ret = readGpuMemory(allThreads[threadId]->getMemoryHandle(), reinterpret_cast<char *>(&implicitArgs), sizeof(NEO::ImplicitArgs), implicitArgsGpuVa);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    if (implicitArgs.v0.header.structVersion != 1) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    *result = implicitArgs.v1.scratchPtr;

    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionImp::getScratchRenderSurfaceStateAddressV2(EuThread::ThreadId threadId, uint64_t *result) {
    const uint32_t regSize = getRegisterSize(ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU);
    UNRECOVERABLE_IF(regSize != sizeof(uint32_t));
    uint32_t s18and19[2] = {0};
    ze_result_t ret = readRegistersImp(threadId, ZET_DEBUG_REGSET_TYPE_SCALAR_INTEL_GPU, 18, 2, s18and19);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    *result = (static_cast<uint64_t>(s18and19[0]) << 32) | s18and19[1];
    return ZE_RESULT_SUCCESS;
}

ze_result_t DebugSessionImp::getScratchRenderSurfaceStateAddress(EuThread::ThreadId threadId, uint64_t *result) {
    if (getProductHelper().isScratchSpaceBasePointerInGrf()) {
        return getScratchRenderSurfaceStateAddressV1(threadId, result);
    } else {
        return getScratchRenderSurfaceStateAddressV2(threadId, result);
    }
}

ze_result_t DebugSessionImp::readThreadScratchRegisters(EuThread::ThreadId threadId, uint32_t start, uint32_t count, void *pRegisterValues) {
    const SIP::regset_desc *threadScratchRegDesc = DebugSessionImp::getThreadScratchRegsetDesc();
    if (start >= threadScratchRegDesc->num) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    if (start + count > threadScratchRegDesc->num) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    const size_t size = count * threadScratchRegDesc->bytes;
    memset(pRegisterValues, 0, size);

    uint64_t renderSurfaceStateAddress = 0;
    ze_result_t ret = getScratchRenderSurfaceStateAddress(threadId, &renderSurfaceStateAddress);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    if (renderSurfaceStateAddress == 0) {
        return ZE_RESULT_SUCCESS;
    }

    const NEO::GfxCoreHelper &gfxCoreHelper = connectedDevice->getGfxCoreHelper();
    const size_t renderSurfaceStateSize = gfxCoreHelper.getRenderSurfaceStateSize();
    const size_t topScratchAreaToRead = (count == 2) || (start == 1) ? 2 : 1;
    std::vector<char> renderSurfaceState(renderSurfaceStateSize * topScratchAreaToRead, 0);

    ret = readGpuMemory(allThreads[threadId]->getMemoryHandle(), renderSurfaceState.data(), renderSurfaceStateSize * topScratchAreaToRead, renderSurfaceStateAddress);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    std::vector<uint64_t> packed;
    for (size_t i = 0; i < topScratchAreaToRead; i++) {
        auto scratchSpacePTSize = gfxCoreHelper.getRenderSurfaceStatePitch(renderSurfaceState.data() + (i * renderSurfaceStateSize), connectedDevice->getProductHelper());
        auto threadOffset = getPerThreadScratchOffset(scratchSpacePTSize, threadId);
        auto gmmHelper = connectedDevice->getNEODevice()->getGmmHelper();
        auto scratchAllocationBase = gmmHelper->decanonize(gfxCoreHelper.getRenderSurfaceStateBaseAddress(renderSurfaceState.data() + (i * renderSurfaceStateSize)));
        auto scratchSpaceBaseAddress = threadOffset + scratchAllocationBase;

        packed.push_back(scratchSpaceBaseAddress);
    }

    memcpy_s(pRegisterValues, size, &packed[start], size);
    return ZE_RESULT_SUCCESS;
}

} // namespace L0
