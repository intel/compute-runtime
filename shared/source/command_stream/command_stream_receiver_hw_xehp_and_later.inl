/*
 * Copyright (C) 2021-2022 Intel Corporation
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
void CommandStreamReceiverHw<GfxFamily>::programL3(LinearStream &csr, uint32_t &newL3Config) {}

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
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForActivePartitionConfig() const {
    if (this->staticWorkPartitioningEnabled) {
        return ImplicitScalingDispatch<GfxFamily>::getRegisterConfigurationSize();
    }
    return 0;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programActivePartitionConfig(LinearStream &csr) {
    if (this->staticWorkPartitioningEnabled) {
        uint64_t workPartitionAddress = getWorkPartitionAllocationGpuAddress();
        ImplicitScalingDispatch<GfxFamily>::dispatchRegisterConfiguration(csr, workPartitionAddress, this->postSyncWriteOffset);
    }
    this->activePartitionsConfig = this->activePartitions;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::addPipeControlBeforeStateSip(LinearStream &commandStream, Device &device) {
    auto &hwInfo = peekHwInfo();
    HwHelper &hwHelper = HwHelper::get(hwInfo.platform.eRenderCoreFamily);
    auto hwInfoConfig = HwInfoConfig::get(hwInfo.platform.eProductFamily);
    bool debuggingEnabled = device.getDebugger() != nullptr;
    PipeControlArgs args;
    args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo);
    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig->isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs());
    std::ignore = isExtendedWARequired;

    if (isBasicWARequired && debuggingEnabled && !hwHelper.isSipWANeeded(hwInfo)) {
        NEO::EncodeWA<GfxFamily>::addPipeControlPriorToNonPipelinedStateCommand(commandStream, args, hwInfo, isRcs());
    }
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForStallingNoPostSyncCommands() const {
    if (isMultiTileOperationEnabled()) {
        return ImplicitScalingDispatch<GfxFamily>::getBarrierSize(peekHwInfo(),
                                                                  false,
                                                                  false);
    } else {
        return sizeof(typename GfxFamily::PIPE_CONTROL);
    }
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForStallingPostSyncCommands() const {
    if (isMultiTileOperationEnabled()) {
        return ImplicitScalingDispatch<GfxFamily>::getBarrierSize(peekHwInfo(),
                                                                  false,
                                                                  true);
    } else {
        return MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(peekHwInfo());
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStallingNoPostSyncCommandsForBarrier(LinearStream &cmdStream) {
    const auto &hwInfo = peekHwInfo();
    PipeControlArgs args;
    if (isMultiTileOperationEnabled()) {
        ImplicitScalingDispatch<GfxFamily>::dispatchBarrierCommands(cmdStream,
                                                                    this->deviceBitfield,
                                                                    args,
                                                                    hwInfo,
                                                                    0,
                                                                    0,
                                                                    false,
                                                                    false);
    } else {
        MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(cmdStream, args);
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStallingPostSyncCommandsForBarrier(LinearStream &cmdStream, TagNodeBase &tagNode) {
    auto barrierTimestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(tagNode);
    const auto &hwInfo = peekHwInfo();
    PipeControlArgs args;
    args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo);
    if (isMultiTileOperationEnabled()) {
        args.workloadPartitionOffset = true;
        ImplicitScalingDispatch<GfxFamily>::dispatchBarrierCommands(cmdStream,
                                                                    this->deviceBitfield,
                                                                    args,
                                                                    hwInfo,
                                                                    barrierTimestampPacketGpuAddress,
                                                                    0,
                                                                    false,
                                                                    false);
        tagNode.setPacketsUsed(this->activePartitions);
    } else {
        MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
            cmdStream,
            PostSyncMode::ImmediateData,
            barrierTimestampPacketGpuAddress,
            0,
            hwInfo,
            args);
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::configurePostSyncWriteOffset() {
    this->postSyncWriteOffset = ImplicitScalingDispatch<GfxFamily>::getPostSyncOffset();
}

} // namespace NEO
