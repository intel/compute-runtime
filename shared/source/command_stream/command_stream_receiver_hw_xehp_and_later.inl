/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver_hw_base.inl"
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/command_stream/scratch_space_controller_xehp_and_later.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getSshHeapSize() { return 2 * MB; }

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::are4GbHeapsAvailable() const { return is64bit; }

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programL3(LinearStream &csr, DispatchFlags &dispatchFlags, uint32_t &newL3Config) {}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getRequiredStateBaseAddressSize(const Device &device) const {
    size_t size = sizeof(typename GfxFamily::STATE_BASE_ADDRESS);
    size += sizeof(typename GfxFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC);
    size += sizeof(PIPE_CONTROL);

    auto &hwInfo = *device.getRootDeviceEnvironment().getHardwareInfo();
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);
    if (hwInfoConfig.isAdditionalStateBaseAddressWARequired(hwInfo)) {
        size += sizeof(typename GfxFamily::STATE_BASE_ADDRESS);
    }

    return size;
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForL3Config() const { return 0; }

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForComputeMode() {
    if (!csrSizeRequestFlags.hasSharedHandles) {
        for (const auto &allocation : this->getResidencyAllocations()) {
            if (allocation->peekSharedHandle()) {
                csrSizeRequestFlags.hasSharedHandles = true;
                break;
            }
        }
    }

    size_t size = 0;
    auto hwInfo = peekHwInfo();
    if (isComputeModeNeeded()) {
        auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
        if (hwInfoConfig->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs())) {
            size += sizeof(typename GfxFamily::PIPE_CONTROL);
        }
        size += sizeof(typename GfxFamily::STATE_COMPUTE_MODE);
        if (csrSizeRequestFlags.hasSharedHandles) {
            size += sizeof(typename GfxFamily::PIPE_CONTROL);
        }
    }
    return size;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programPipelineSelect(LinearStream &commandStream, PipelineSelectArgs &pipelineSelectArgs) {
    if (csrSizeRequestFlags.mediaSamplerConfigChanged || csrSizeRequestFlags.specialPipelineSelectModeChanged || !isPreambleSent) {
        PreambleHelper<GfxFamily>::programPipelineSelect(&commandStream, pipelineSelectArgs, peekHwInfo());
        this->lastMediaSamplerConfig = pipelineSelectArgs.mediaSamplerRequired;
        this->lastSpecialPipelineSelectMode = pipelineSelectArgs.specialPipelineSelectMode;
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::createScratchSpaceController() {
    scratchSpaceController = std::make_unique<ScratchSpaceControllerXeHPAndLater>(this->rootDeviceIndex, executionEnvironment, *internalAllocationStorage.get());
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programEpliogueCommands(LinearStream &csr, const DispatchFlags &dispatchFlags) {
    this->programEngineModeEpliogue(csr, dispatchFlags);
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForEpilogueCommands(const DispatchFlags &dispatchFlags) const {
    return this->getCmdSizeForEngineMode(dispatchFlags);
}

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::isMultiOsContextCapable() const {
    return deviceBitfield.count() > 1u;
}

template <typename GfxFamily>
class ImplicitFlushSettings {
  public:
    static bool &getSettingForNewResource() {
        return defaultSettingForNewResource;
    }
    static bool &getSettingForGpuIdle() {
        return defaultSettingForGpuIdle;
    }

  private:
    static bool defaultSettingForNewResource;
    static bool defaultSettingForGpuIdle;
};

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::checkPlatformSupportsNewResourceImplicitFlush() const {
    if (this->isMultiOsContextCapable()) {
        return false;
    }
    return ImplicitFlushSettings<GfxFamily>::getSettingForNewResource()
               ? getOSInterface()->newResourceImplicitFlush
               : false;
}

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::checkPlatformSupportsGpuIdleImplicitFlush() const {
    if (this->isMultiOsContextCapable() && !this->osContext->isDirectSubmissionActive()) {
        return false;
    }
    return ImplicitFlushSettings<GfxFamily>::getSettingForGpuIdle()
               ? getOSInterface()->gpuIdleImplicitFlush
               : false;
}

template <typename GfxFamily>
GraphicsAllocation *CommandStreamReceiverHw<GfxFamily>::getClearColorAllocation() {
    return nullptr;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::collectStateBaseAddresIohPatchInfo(uint64_t commandBufferAddress, uint64_t commandOffset, const LinearStream &ioh) {
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForActivePartitionConfig() const {
    if (this->staticWorkPartitioningEnabled && csrSizeRequestFlags.activePartitionsChanged) {
        return EncodeSetMMIO<GfxFamily>::sizeMEM +
               EncodeSetMMIO<GfxFamily>::sizeIMM;
    }
    return 0;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programActivePartitionConfig() {
    if (this->staticWorkPartitioningEnabled && csrSizeRequestFlags.activePartitionsChanged) {
        uint64_t workPartitionAddress = getWorkPartitionAllocationGpuAddress();
        EncodeSetMMIO<GfxFamily>::encodeMEM(commandStream,
                                            PartitionRegisters<GfxFamily>::wparidCCSOffset,
                                            workPartitionAddress);
        EncodeSetMMIO<GfxFamily>::encodeIMM(commandStream,
                                            PartitionRegisters<GfxFamily>::addressOffsetCCSOffset,
                                            CommonConstants::partitionAddressOffset,
                                            true);
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::addPipeControlPriorToNonPipelinedStateCommand(LinearStream &commandStream, PipeControlArgs args) {
    auto hwInfo = peekHwInfo();
    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);

    if (hwInfoConfig->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs())) {
        args.textureCacheInvalidationEnable = true;
        args.hdcPipelineFlush = true;
        args.amfsFlushEnable = true;
        args.instructionCacheInvalidateEnable = true;
        args.constantCacheInvalidationEnable = true;
        args.stateCacheInvalidationEnable = true;

        args.dcFlushEnable = false;

        setPipeControlPriorToNonPipelinedStateCommandExtraProperties(args);
    }

    addPipeControlCmd(commandStream, args);
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::addPipeControlBeforeStateSip(LinearStream &commandStream, Device &device) {
    auto hwInfo = peekHwInfo();
    HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
    bool debuggingEnabled = device.getDebugger() != nullptr;
    PipeControlArgs args(true);

    if (hwInfoConfig->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs()) && debuggingEnabled &&
        !hwHelper.isSipWANeeded(hwInfo)) {
        addPipeControlPriorToNonPipelinedStateCommand(commandStream, args);
    }
}

} // namespace NEO
