/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub_mem_dump/aub_mem_dump.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/memory_manager/memory_constants.h"

namespace OCLRT {

template <typename Family>
void HwHelperHw<Family>::setCapabilityCoherencyFlag(const HardwareInfo *pHwInfo, bool &coherencyFlag) {
    coherencyFlag = true;
}

template <typename Family>
void HwHelperHw<Family>::adjustDefaultEngineType(HardwareInfo *pHwInfo) {
}

template <typename Family>
bool HwHelperHw<Family>::isLocalMemoryEnabled(const HardwareInfo &hwInfo) {
    return false;
}

template <typename Family>
void HwHelperHw<Family>::setupHardwareCapabilities(HardwareCapabilities *caps, const HardwareInfo &hwInfo) {
    caps->image3DMaxHeight = 16384;
    caps->image3DMaxWidth = 16384;
    //With statefull messages we have an allocation cap of 4GB
    //Reason to subtract 8KB is that driver may pad the buffer with addition pages for over fetching..
    caps->maxMemAllocSize = (4ULL * MemoryConstants::gigaByte) - (8ULL * MemoryConstants::kiloByte);
    caps->isStatelesToStatefullWithOffsetSupported = true;
    caps->localMemorySupported = isLocalMemoryEnabled(hwInfo);
}

template <typename Family>
uint32_t HwHelperHw<Family>::getComputeUnitsUsedForScratch(const HardwareInfo *pHwInfo) const {
    return pHwInfo->pSysInfo->MaxSubSlicesSupported * pHwInfo->pSysInfo->MaxEuPerSubSlice *
           pHwInfo->pSysInfo->ThreadCount / pHwInfo->pSysInfo->EUCount;
}

template <typename Family>
SipKernelType HwHelperHw<Family>::getSipKernelType(bool debuggingActive) {
    if (!debuggingActive) {
        return SipKernelType::Csr;
    }
    return SipKernelType::DbgCsr;
}

template <typename Family>
uint32_t HwHelperHw<Family>::getConfigureAddressSpaceMode() {
    return 0u;
}

template <typename Family>
bool HwHelperHw<Family>::setupPreemptionRegisters(HardwareInfo *pHwInfo, bool enable) {
    return false;
}

template <typename Family>
size_t HwHelperHw<Family>::getMaxBarrierRegisterPerSlice() const {
    return 32;
}

template <typename Family>
const AubMemDump::LrcaHelper &HwHelperHw<Family>::getCsTraits(EngineInstanceT engineInstance) const {
    return *AUBFamilyMapper<Family>::csTraits[engineInstance.type];
}

template <typename Family>
bool HwHelperHw<Family>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename Family>
bool HwHelperHw<Family>::supportsYTiling() const {
    return true;
}

template <typename Family>
bool HwHelperHw<Family>::timestampPacketWriteSupported() const {
    return false;
}

template <typename Family>
void HwHelperHw<Family>::setRenderSurfaceStateForBuffer(ExecutionEnvironment &executionEnvironment,
                                                        void *surfaceStateBuffer,
                                                        size_t bufferSize,
                                                        uint64_t gpuVa,
                                                        size_t offset,
                                                        uint32_t pitch,
                                                        GraphicsAllocation *gfxAlloc,
                                                        cl_mem_flags flags,
                                                        uint32_t surfaceType,
                                                        bool forceNonAuxMode) {
    using RENDER_SURFACE_STATE = typename Family::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
    using AUXILIARY_SURFACE_MODE = typename RENDER_SURFACE_STATE::AUXILIARY_SURFACE_MODE;

    auto gmmHelper = executionEnvironment.getGmmHelper();
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(surfaceStateBuffer);
    *surfaceState = Family::cmdInitRenderSurfaceState;
    auto surfaceSize = alignUp(bufferSize, 4);

    SURFACE_STATE_BUFFER_LENGTH Length = {0};
    Length.Length = static_cast<uint32_t>(surfaceSize - 1);

    surfaceState->setWidth(Length.SurfaceState.Width + 1);
    surfaceState->setHeight(Length.SurfaceState.Height + 1);
    surfaceState->setDepth(Length.SurfaceState.Depth + 1);
    if (pitch) {
        surfaceState->setSurfacePitch(pitch);
    }

    // The graphics allocation for Host Ptr surface will be created in makeResident call and GPU address is expected to be the same as CPU address
    auto bufferStateAddress = (gfxAlloc != nullptr) ? gfxAlloc->getGpuAddress() : gpuVa;
    bufferStateAddress += offset;

    auto bufferStateSize = (gfxAlloc != nullptr) ? gfxAlloc->getUnderlyingBufferSize() : bufferSize;

    surfaceState->setSurfaceType(static_cast<typename RENDER_SURFACE_STATE::SURFACE_TYPE>(surfaceType));

    surfaceState->setSurfaceFormat(SURFACE_FORMAT::SURFACE_FORMAT_RAW);
    surfaceState->setSurfaceVerticalAlignment(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4);
    surfaceState->setSurfaceHorizontalAlignment(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_4);

    surfaceState->setTileMode(RENDER_SURFACE_STATE::TILE_MODE_LINEAR);
    surfaceState->setVerticalLineStride(0);
    surfaceState->setVerticalLineStrideOffset(0);
    if ((isAligned<MemoryConstants::cacheLineSize>(bufferStateAddress) && isAligned<MemoryConstants::cacheLineSize>(bufferStateSize)) ||
        ((flags & CL_MEM_READ_ONLY)) != 0) {
        surfaceState->setMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER));
    } else {
        surfaceState->setMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));
    }

    surfaceState->setSurfaceBaseAddress(bufferStateAddress);

    Gmm *gmm = gfxAlloc ? gfxAlloc->gmm : nullptr;
    if (gmm && gmm->isRenderCompressed && !forceNonAuxMode &&
        GraphicsAllocation::AllocationType::BUFFER_COMPRESSED == gfxAlloc->getAllocationType()) {
        // Its expected to not program pitch/qpitch/baseAddress for Aux surface in CCS scenarios
        surfaceState->setCoherencyType(RENDER_SURFACE_STATE::COHERENCY_TYPE_GPU_COHERENT);
        surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_CCS_E);
    } else {
        surfaceState->setCoherencyType(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT);
        surfaceState->setAuxiliarySurfaceMode(AUXILIARY_SURFACE_MODE::AUXILIARY_SURFACE_MODE_AUX_NONE);
    }
}

template <typename Family>
size_t HwHelperHw<Family>::getScratchSpaceOffsetFor64bit() {
    return 4096;
}

template <typename Family>
const std::vector<EngineInstanceT> HwHelperHw<Family>::getGpgpuEngineInstances() const {
    constexpr std::array<EngineInstanceT, 2> gpgpuEngineInstances = {{{ENGINE_RCS, 0},
                                                                      lowPriorityGpgpuEngine}};
    return std::vector<EngineInstanceT>(gpgpuEngineInstances.begin(), gpgpuEngineInstances.end());
};

} // namespace OCLRT
