/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw_base.inl"
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/command_stream/scratch_space_controller_xehp_plus.h"
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
    if (isComputeModeNeeded()) {
        if (isAdditionalPipeControlNeeded()) {
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
    scratchSpaceController = std::make_unique<ScratchSpaceControllerXeHPPlus>(this->rootDeviceIndex, executionEnvironment, *internalAllocationStorage.get());
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

} // namespace NEO
