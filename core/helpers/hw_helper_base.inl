/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm.h"
#include "core/gmm_helper/gmm_helper.h"
#include "core/helpers/aligned_memory.h"
#include "core/helpers/hw_helper.h"
#include "core/helpers/hw_info.h"
#include "core/helpers/preamble.h"
#include "core/memory_manager/graphics_allocation.h"
#include "core/memory_manager/memory_constants.h"
#include "runtime/aub_mem_dump/aub_mem_dump.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/dispatch_info.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/os_interface/os_interface.h"

namespace NEO {

template <typename Family>
const aub_stream::EngineType HwHelperHw<Family>::lowPriorityEngineType = aub_stream::EngineType::ENGINE_RCS;

template <typename Family>
const AuxTranslationMode HwHelperHw<Family>::defaultAuxTranslationMode = AuxTranslationMode::Builtin;

template <typename Family>
bool HwHelperHw<Family>::obtainRenderBufferCompressionPreference(const HardwareInfo &hwInfo, const size_t size) const {
    return size > KB;
}

template <typename Family>
void HwHelperHw<Family>::setupHardwareCapabilities(HardwareCapabilities *caps, const HardwareInfo &hwInfo) {
    caps->image3DMaxHeight = 16384;
    caps->image3DMaxWidth = 16384;
    //With statefull messages we have an allocation cap of 4GB
    //Reason to subtract 8KB is that driver may pad the buffer with addition pages for over fetching..
    caps->maxMemAllocSize = (4ULL * MemoryConstants::gigaByte) - (8ULL * MemoryConstants::kiloByte);
    caps->isStatelesToStatefullWithOffsetSupported = true;
}

template <typename Family>
bool HwHelperHw<Family>::isL3Configurable(const HardwareInfo &hwInfo) {
    return PreambleHelper<Family>::isL3Configurable(hwInfo);
}

template <typename Family>
SipKernelType HwHelperHw<Family>::getSipKernelType(bool debuggingActive) {
    if (!debuggingActive) {
        return SipKernelType::Csr;
    }
    return SipKernelType::DbgCsr;
}

template <typename Family>
size_t HwHelperHw<Family>::getMaxBarrierRegisterPerSlice() const {
    return 32;
}

template <typename Family>
uint32_t HwHelperHw<Family>::getPitchAlignmentForImage(const HardwareInfo *hwInfo) {
    return 4u;
}

template <typename Family>
const AubMemDump::LrcaHelper &HwHelperHw<Family>::getCsTraits(aub_stream::EngineType engineType) const {
    return *AUBFamilyMapper<Family>::csTraits[engineType];
}

template <typename Family>
bool HwHelperHw<Family>::isPageTableManagerSupported(const HardwareInfo &hwInfo) const {
    return false;
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::checkResourceCompatibility(GraphicsAllocation &graphicsAllocation) {
    return true;
}

template <typename Family>
void HwHelperHw<Family>::setRenderSurfaceStateForBuffer(ExecutionEnvironment &executionEnvironment,
                                                        void *surfaceStateBuffer,
                                                        size_t bufferSize,
                                                        uint64_t gpuVa,
                                                        size_t offset,
                                                        uint32_t pitch,
                                                        GraphicsAllocation *gfxAlloc,
                                                        bool isReadOnly,
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
        isReadOnly) {
        surfaceState->setMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER));
    } else {
        surfaceState->setMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));
    }

    surfaceState->setSurfaceBaseAddress(bufferStateAddress);

    Gmm *gmm = gfxAlloc ? gfxAlloc->getDefaultGmm() : nullptr;
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
bool HwHelperHw<Family>::getEnableLocalMemory(const HardwareInfo &hwInfo) const {
    if (DebugManager.flags.EnableLocalMemory.get() != -1) {
        return DebugManager.flags.EnableLocalMemory.get();
    } else if (DebugManager.flags.AUBDumpForceAllToLocalMemory.get()) {
        return true;
    }

    return OSInterface::osEnableLocalMemory && isLocalMemoryEnabled(hwInfo);
}

template <typename Family>
AuxTranslationMode HwHelperHw<Family>::getAuxTranslationMode() {
    if (DebugManager.flags.ForceAuxTranslationMode.get() != -1) {
        return static_cast<AuxTranslationMode>(DebugManager.flags.ForceAuxTranslationMode.get());
    }

    return HwHelperHw<Family>::defaultAuxTranslationMode;
}

template <typename Family>
bool HwHelperHw<Family>::isBlitAuxTranslationRequired(const HardwareInfo &hwInfo, const MultiDispatchInfo &multiDispatchInfo) {
    return (HwHelperHw<Family>::getAuxTranslationMode() == AuxTranslationMode::Blit) &&
           hwInfo.capabilityTable.blitterOperationsSupported &&
           multiDispatchInfo.getMemObjsForAuxTranslation() &&
           (multiDispatchInfo.getMemObjsForAuxTranslation()->size() > 0);
}

template <typename Family>
typename Family::PIPE_CONTROL *PipeControlHelper<Family>::obtainPipeControlAndProgramPostSyncOperation(LinearStream &commandStream,
                                                                                                       POST_SYNC_OPERATION operation,
                                                                                                       uint64_t gpuAddress,
                                                                                                       uint64_t immediateData,
                                                                                                       bool dcFlush,
                                                                                                       const HardwareInfo &hwInfo) {
    addPipeControlWA(commandStream, hwInfo);

    auto pipeControl = obtainPipeControl(commandStream, dcFlush);
    pipeControl->setPostSyncOperation(operation);
    pipeControl->setAddress(static_cast<uint32_t>(gpuAddress & 0x0000FFFFFFFFULL));
    pipeControl->setAddressHigh(static_cast<uint32_t>(gpuAddress >> 32));
    pipeControl->setDcFlushEnable(dcFlush);
    if (operation == POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA) {
        pipeControl->setImmediateData(immediateData);
    }
    return pipeControl;
}

template <typename GfxFamily>
typename GfxFamily::PIPE_CONTROL *PipeControlHelper<GfxFamily>::obtainPipeControl(LinearStream &commandStream, bool dcFlush) {
    auto pCmd = reinterpret_cast<PIPE_CONTROL *>(commandStream.getSpace(sizeof(PIPE_CONTROL)));
    *pCmd = GfxFamily::cmdInitPipeControl;
    pCmd->setCommandStreamerStallEnable(true);
    pCmd->setDcFlushEnable(dcFlush);

    if (DebugManager.flags.FlushAllCaches.get()) {
        pCmd->setDcFlushEnable(true);
        pCmd->setRenderTargetCacheFlushEnable(true);
        pCmd->setInstructionCacheInvalidateEnable(true);
        pCmd->setTextureCacheInvalidationEnable(true);
        pCmd->setPipeControlFlushEnable(true);
        pCmd->setVfCacheInvalidationEnable(true);
        pCmd->setConstantCacheInvalidationEnable(true);
        pCmd->setStateCacheInvalidationEnable(true);
    }
    return pCmd;
}

template <typename GfxFamily>
void PipeControlHelper<GfxFamily>::addPipeControlWA(LinearStream &commandStream, const HardwareInfo &hwInfo) {
}

template <typename GfxFamily>
typename GfxFamily::PIPE_CONTROL *PipeControlHelper<GfxFamily>::addPipeControl(LinearStream &commandStream, bool dcFlush) {
    return PipeControlHelper<GfxFamily>::obtainPipeControl(commandStream, dcFlush);
}

template <typename GfxFamily>
size_t PipeControlHelper<GfxFamily>::getSizeForSinglePipeControl() {
    return sizeof(typename GfxFamily::PIPE_CONTROL);
}

template <typename GfxFamily>
size_t PipeControlHelper<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(const HardwareInfo &hwInfo) {
    const auto pipeControlCount = HardwareCommandsHelper<GfxFamily>::isPipeControlWArequired(hwInfo) ? 2u : 1u;
    return pipeControlCount * getSizeForSinglePipeControl();
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::Gen9);
}

template <typename GfxFamily>
inline bool HwHelperHw<GfxFamily>::requiresAuxResolves() const {
    return true;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::tilingAllowed(bool isSharedContext, bool isImage1d, bool forceLinearStorage) {
    if (DebugManager.flags.ForceLinearImages.get() || forceLinearStorage || isSharedContext) {
        return false;
    }
    return !isImage1d;
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::alignSlmSize(uint32_t slmSize) {
    return HardwareCommandsHelper<GfxFamily>::alignSlmSize(slmSize);
}

template <typename GfxFamily>
uint32_t HwHelperHw<GfxFamily>::getBarriersCountFromHasBarriers(uint32_t hasBarriers) {
    return hasBarriers;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isOffsetToSkipSetFFIDGPWARequired(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isForceDefaultRCSEngineWARequired(const HardwareInfo &hwInfo) {
    return false;
}

template <typename GfxFamily>
bool HwHelperHw<GfxFamily>::isForceEmuInt32DivRemSPWARequired(const HardwareInfo &hwInfo) {
    return false;
}

} // namespace NEO
