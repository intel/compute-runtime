/*
 * Copyright (C) 2019-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw_base.inl"
#include "shared/source/helpers/address_patch.h"
#include "shared/source/helpers/state_base_address_bdw_and_later.inl"

namespace NEO {

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::are4GbHeapsAvailable() const { return true; }

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getRequiredStateBaseAddressSize(const Device &device) const {
    size_t size = 0;
    const auto &productHelper = getProductHelper();
    if (productHelper.is3DPipelineSelectWARequired()) {
        size += (2 * PreambleHelper<GfxFamily>::getCmdSizeForPipelineSelect(peekRootDeviceEnvironment()));
    }
    size += sizeof(typename GfxFamily::STATE_BASE_ADDRESS) + sizeof(PIPE_CONTROL);
    return size;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programPipelineSelect(LinearStream &commandStream, PipelineSelectArgs &pipelineSelectArgs) {
    if (csrSizeRequestFlags.mediaSamplerConfigChanged || csrSizeRequestFlags.systolicPipelineSelectMode || !isPreambleSent) {
        if (!isPipelineSelectAlreadyProgrammed()) {
            PreambleHelper<GfxFamily>::programPipelineSelect(&commandStream, pipelineSelectArgs, peekRootDeviceEnvironment());
        }
        this->lastMediaSamplerConfig = pipelineSelectArgs.mediaSamplerRequired;
        this->lastSystolicPipelineSelectMode = pipelineSelectArgs.systolicPipelineSelectMode;
        this->streamProperties.pipelineSelect.setPropertiesAll(true, this->lastMediaSamplerConfig, this->lastSystolicPipelineSelectMode);
        this->streamProperties.pipelineSelect.clearIsDirty();
    }
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::createScratchSpaceController() {
    scratchSpaceController = std::make_unique<ScratchSpaceControllerBase>(rootDeviceIndex, executionEnvironment, *internalAllocationStorage.get());
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
    return false;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::addPipeControlBeforeStateSip(LinearStream &commandStream, Device &device) {}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::addPipeControlBefore3dState(LinearStream &commandStream, DispatchFlags &dispatchFlags) {}

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::checkPlatformSupportsNewResourceImplicitFlush() const {
    return false;
}

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::checkPlatformSupportsGpuIdleImplicitFlush() const {
    return false;
}

template <typename GfxFamily>
GraphicsAllocation *CommandStreamReceiverHw<GfxFamily>::getClearColorAllocation() {
    return nullptr;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programPerDssBackedBuffer(LinearStream &commandStream, Device &device, DispatchFlags &dispatchFlags) {
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForPerDssBackedBuffer(const HardwareInfo &hwInfo) {
    return 0;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::dispatchRayTracingStateCommand(LinearStream &cmdStream, Device &device) {
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::collectStateBaseAddresIohPatchInfo(uint64_t commandBufferAddress, uint64_t commandOffset, const LinearStream &ioh) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    PatchInfoData indirectObjectPatchInfo = {ioh.getGraphicsAllocation()->getGpuAddress(), 0u, PatchInfoAllocationType::indirectObjectHeap, commandBufferAddress,
                                             commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::INDIRECTOBJECTBASEADDRESS_BYTEOFFSET, PatchInfoAllocationType::defaultType};
    flatBatchBufferHelper->setPatchInfoData(indirectObjectPatchInfo);
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForActivePartitionConfig() const {
    return 0;
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programActivePartitionConfig(LinearStream &csr) {
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForStallingNoPostSyncCommands() const {
    return sizeof(typename GfxFamily::PIPE_CONTROL);
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForStallingPostSyncCommands() const {
    return MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(peekRootDeviceEnvironment(), false);
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStallingNoPostSyncCommandsForBarrier(LinearStream &cmdStream) {
    PipeControlArgs args;
    MemorySynchronizationCommands<GfxFamily>::addSingleBarrier(cmdStream, args);
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStallingPostSyncCommandsForBarrier(LinearStream &cmdStream, TagNodeBase &tagNode, bool dcFlushRequired) {
    auto barrierTimestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(tagNode);
    PipeControlArgs args;
    args.dcFlushEnable = this->dcFlushSupport && dcFlushRequired;
    MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
        cmdStream,
        PostSyncMode::immediateData,
        barrierTimestampPacketGpuAddress,
        0,
        peekRootDeviceEnvironment(),
        args);
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::configurePostSyncWriteOffset() {
}

} // namespace NEO
