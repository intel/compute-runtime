/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw_base.inl"
#include "shared/source/helpers/address_patch.h"

#include "hw_cmds.h"

namespace NEO {

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getSshHeapSize() {
    return getDefaultHeapSize();
}

template <typename GfxFamily>
bool CommandStreamReceiverHw<GfxFamily>::are4GbHeapsAvailable() const { return true; }

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programL3(LinearStream &csr, uint32_t &newL3Config) {
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;
    if (csrSizeRequestFlags.l3ConfigChanged && this->isPreambleSent) {
        // Add a PIPE_CONTROL w/ CS_stall
        auto pCmd = (PIPE_CONTROL *)csr.getSpace(sizeof(PIPE_CONTROL));
        PIPE_CONTROL cmd = GfxFamily::cmdInitPipeControl;
        cmd.setCommandStreamerStallEnable(true);
        cmd.setDcFlushEnable(true);
        addClearSLMWorkAround(&cmd);
        *pCmd = cmd;

        PreambleHelper<GfxFamily>::programL3(&csr, newL3Config);
        this->lastSentL3Config = newL3Config;
    }
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getRequiredStateBaseAddressSize(const Device &device) const {
    using PIPELINE_SELECT = typename GfxFamily::PIPELINE_SELECT;

    size_t size = 0;
    const auto &hwInfoConfig = *HwInfoConfig::get(peekHwInfo().platform.eProductFamily);
    if (hwInfoConfig.is3DPipelineSelectWARequired()) {
        size += (2 * PreambleHelper<GfxFamily>::getCmdSizeForPipelineSelect(peekHwInfo()));
    }
    size += sizeof(typename GfxFamily::STATE_BASE_ADDRESS) + sizeof(PIPE_CONTROL);
    return size;
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForL3Config() const {
    if (!this->isPreambleSent) {
        return sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
    } else if (csrSizeRequestFlags.l3ConfigChanged) {
        return sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM) + sizeof(typename GfxFamily::PIPE_CONTROL);
    }
    return 0;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programPipelineSelect(LinearStream &commandStream, PipelineSelectArgs &pipelineSelectArgs) {
    if (csrSizeRequestFlags.mediaSamplerConfigChanged || !isPreambleSent) {
        if (!isPipelineSelectAlreadyProgrammed()) {
            PreambleHelper<GfxFamily>::programPipelineSelect(&commandStream, pipelineSelectArgs, peekHwInfo());
        }
        this->lastMediaSamplerConfig = pipelineSelectArgs.mediaSamplerRequired;
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
inline void CommandStreamReceiverHw<GfxFamily>::addPipeControlBeforeStateBaseAddress(LinearStream &commandStream) {
    PipeControlArgs args;
    args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, peekHwInfo());
    args.textureCacheInvalidationEnable = true;

    NEO::EncodeWA<GfxFamily>::addPipeControlPriorToNonPipelinedStateCommand(commandStream, args, peekHwInfo(), isRcs());
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
void CommandStreamReceiverHw<GfxFamily>::collectStateBaseAddresIohPatchInfo(uint64_t commandBufferAddress, uint64_t commandOffset, const LinearStream &ioh) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    PatchInfoData indirectObjectPatchInfo = {ioh.getGraphicsAllocation()->getGpuAddress(), 0u, PatchInfoAllocationType::IndirectObjectHeap, commandBufferAddress,
                                             commandOffset + STATE_BASE_ADDRESS::PATCH_CONSTANTS::INDIRECTOBJECTBASEADDRESS_BYTEOFFSET, PatchInfoAllocationType::Default};
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
    return MemorySynchronizationCommands<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(peekHwInfo());
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStallingNoPostSyncCommandsForBarrier(LinearStream &cmdStream) {
    PipeControlArgs args;
    MemorySynchronizationCommands<GfxFamily>::addPipeControl(cmdStream, args);
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::programStallingPostSyncCommandsForBarrier(LinearStream &cmdStream, TagNodeBase &tagNode) {
    auto barrierTimestampPacketGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(tagNode);
    const auto &hwInfo = peekHwInfo();
    PipeControlArgs args;
    args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo);
    MemorySynchronizationCommands<GfxFamily>::addPipeControlAndProgramPostSyncOperation(
        cmdStream,
        PIPE_CONTROL::POST_SYNC_OPERATION::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA,
        barrierTimestampPacketGpuAddress,
        0,
        hwInfo,
        args);
}

template <typename GfxFamily>
inline void CommandStreamReceiverHw<GfxFamily>::configurePostSyncWriteOffset() {
}

} // namespace NEO
