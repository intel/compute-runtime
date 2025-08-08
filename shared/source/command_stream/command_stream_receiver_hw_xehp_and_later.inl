/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/command_stream_receiver_hw_base.inl"
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/command_stream/scratch_space_controller_xehp_and_later.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::are4GbHeapsAvailable() const { return is64bit; }

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programL3(LinearStream &csr, uint32_t &newL3Config, bool isBcs) {}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getRequiredStateBaseAddressSize(const Device &device) const {
    size_t size = sizeof(typename GfxFamily::STATE_BASE_ADDRESS);
    if (this->globalStatelessHeapAllocation == nullptr) {
        if constexpr (!GfxFamily::isHeaplessRequired()) {
            size += sizeof(typename GfxFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC);
        }
    }
    size += MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();

    if (this->doubleSbaWa) {
        size += sizeof(typename GfxFamily::STATE_BASE_ADDRESS);
    }

    return size;
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForL3Config() const { return 0; }

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programPipelineSelect(LinearStream &commandStream, PipelineSelectArgs &pipelineSelectArgs) {
    if (csrSizeRequestFlags.systolicPipelineSelectMode || !isPreambleSent) {
        PreambleHelper<GfxFamily>::programPipelineSelect(&commandStream, pipelineSelectArgs, peekRootDeviceEnvironment());
        this->lastSystolicPipelineSelectMode = pipelineSelectArgs.systolicPipelineSelectMode;
        this->streamProperties.pipelineSelect.setPropertiesAll(true, this->lastSystolicPipelineSelectMode);
        this->streamProperties.pipelineSelect.clearIsDirty();
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
        ImplicitScalingDispatch<GfxFamily>::dispatchRegisterConfiguration(csr, workPartitionAddress,
                                                                          this->immWritePostSyncWriteOffset, EngineHelpers::isBcs(osContext->getEngineType()));
    }
    this->activePartitionsConfig = this->activePartitions;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::addPipeControlBeforeStateSip(LinearStream &commandStream, Device &device) {
    auto &hwInfo = peekHwInfo();
    auto &productHelper = getProductHelper();
    auto *releaseHelper = getReleaseHelper();
    bool debuggingEnabled = device.getDebugger() != nullptr;
    PipeControlArgs args;
    args.dcFlushEnable = this->dcFlushSupport;
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs(), releaseHelper);
    std::ignore = isExtendedWARequired;

    if (isBasicWARequired && debuggingEnabled) {
        NEO::EncodeWA<GfxFamily>::addPipeControlPriorToNonPipelinedStateCommand(commandStream, args, this->peekRootDeviceEnvironment(), isRcs());
    }
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForStallingNoPostSyncCommands() const {
    if (isMultiTileOperationEnabled()) {
        return ImplicitScalingDispatch<GfxFamily>::getBarrierSize(peekRootDeviceEnvironment(),
                                                                  false,
                                                                  false);
    } else {
        return MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
    }
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForStallingPostSyncCommands() const {
    if (isMultiTileOperationEnabled()) {
        return ImplicitScalingDispatch<GfxFamily>::getBarrierSize(peekRootDeviceEnvironment(),
                                                                  false,
                                                                  true);
    } else {
        return MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(peekRootDeviceEnvironment(), NEO::PostSyncMode::immediateData);
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStallingNoPostSyncCommandsForBarrier(LinearStream &cmdStream) {
    PipeControlArgs args;
    args.hdcPipelineFlush = true;
    args.unTypedDataPortCacheFlush = true;
    if (isMultiTileOperationEnabled()) {
        ImplicitScalingDispatch<GfxFamily>::dispatchBarrierCommands(cmdStream,
                                                                    this->deviceBitfield,
                                                                    args,
                                                                    peekRootDeviceEnvironment(),
                                                                    0,
                                                                    0,
                                                                    false,
                                                                    false);
    } else {
        MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(cmdStream, args);
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStallingPostSyncCommandsForBarrier(LinearStream &cmdStream, TagNodeBase &tagNode, bool dcFlushRequired) {
    auto barrierTimestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(tagNode);
    PipeControlArgs args;
    args.dcFlushEnable = this->dcFlushSupport && dcFlushRequired;
    args.hdcPipelineFlush = true;
    args.unTypedDataPortCacheFlush = true;
    args.isWalkerWithProfilingEnqueued |= this->getAndClearIsWalkerWithProfilingEnqueued();
    if (isMultiTileOperationEnabled()) {
        args.workloadPartitionOffset = true;
        ImplicitScalingDispatch<GfxFamily>::dispatchBarrierCommands(cmdStream,
                                                                    this->deviceBitfield,
                                                                    args,
                                                                    peekRootDeviceEnvironment(),
                                                                    barrierTimestampPacketGpuAddress,
                                                                    0,
                                                                    false,
                                                                    false);
        tagNode.setPacketsUsed(this->activePartitions);
    } else {
        MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
            cmdStream,
            PostSyncMode::immediateData,
            barrierTimestampPacketGpuAddress,
            0,
            peekRootDeviceEnvironment(),
            args);
    }
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::configurePostSyncWriteOffset() {
    this->immWritePostSyncWriteOffset = ImplicitScalingDispatch<GfxFamily>::getImmediateWritePostSyncOffset();
    this->timeStampPostSyncWriteOffset = ImplicitScalingDispatch<GfxFamily>::getTimeStampPostSyncOffset();
}

} // namespace NEO
