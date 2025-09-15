/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/gmm_helper/resource_info.h"

using Family = NEO::Gen12LpFamily;

#include "shared/source/command_stream/command_stream_receiver_hw_base.inl"
#include "shared/source/command_stream/command_stream_receiver_hw_heap_addressing.inl"
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/address_patch.h"
#include "shared/source/helpers/blit_commands_helper_base.inl"
#include "shared/source/helpers/blit_commands_helper_from_gen12lp_to_xe3.inl"
#include "shared/source/helpers/populate_factory.h"

namespace NEO {
static auto gfxCore = IGFX_GEN12LP_CORE;

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
    if (csrSizeRequestFlags.systolicPipelineSelectMode || !isPreambleSent) {
        if (!isPipelineSelectAlreadyProgrammed()) {
            PreambleHelper<GfxFamily>::programPipelineSelect(&commandStream, pipelineSelectArgs, peekRootDeviceEnvironment());
        }
        this->lastSystolicPipelineSelectMode = pipelineSelectArgs.systolicPipelineSelectMode;
        this->streamProperties.pipelineSelect.setPropertiesAll(true, this->lastSystolicPipelineSelectMode);
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
    return MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(peekRootDeviceEnvironment(), NEO::PostSyncMode::immediateData);
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

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitWidthOverride(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return 0;
}

template <typename GfxFamily>
uint64_t BlitCommandsHelper<GfxFamily>::getMaxBlitHeightOverride(const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed) {
    return 0;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendBlitCommandsBlockCopy(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    appendExtraMemoryProperties(blitCmd, rootDeviceEnvironment);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendSurfaceType(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd) {
}
template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendTilingEnable(typename GfxFamily::XY_COLOR_BLT &blitCmd) {
    using XY_COLOR_BLT = typename GfxFamily::XY_COLOR_BLT;
    blitCmd.setDestTilingEnable(XY_COLOR_BLT::DEST_TILING_ENABLE::DEST_TILING_ENABLE_TILING_ENABLED);
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendTilingType(const GMM_TILE_TYPE srcTilingType, const GMM_TILE_TYPE dstTilingType, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd) {
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::getBlitAllocationProperties(const GraphicsAllocation &allocation, uint32_t &pitch, uint32_t &qPitch,
                                                                GMM_TILE_TYPE &tileType, uint32_t &mipTailLod, uint32_t &compressionDetails,
                                                                const RootDeviceEnvironment &rootDeviceEnvironment, GMM_YUV_PLANE_ENUM plane) {
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::programGlobalSequencerFlush(LinearStream &commandStream) {
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getSizeForGlobalSequencerFlush() {
    return 0u;
}

template <typename GfxFamily>
bool BlitCommandsHelper<GfxFamily>::miArbCheckWaRequired() {
    return false;
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::appendClearColor(const BlitProperties &blitProperties, typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd) {
}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::printImageBlitBlockCopyCommand(const typename GfxFamily::XY_BLOCK_COPY_BLT &blitCmd, const uint32_t sliceIndex) {}

template <typename GfxFamily>
void BlitCommandsHelper<GfxFamily>::dispatchDummyBlit(LinearStream &linearStream, EncodeDummyBlitWaArgs &waArgs) {}

template <typename GfxFamily>
bool BlitCommandsHelper<GfxFamily>::isDummyBlitWaNeeded(const EncodeDummyBlitWaArgs &waArgs) {
    return false;
}

template <typename GfxFamily>
size_t BlitCommandsHelper<GfxFamily>::getDummyBlitSize(const EncodeDummyBlitWaArgs &waArgs) {
    return 0u;
}

template <>
void CommandStreamReceiverHw<Family>::programL3(LinearStream &csr, uint32_t &newL3Config, bool isBcs) {
}

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForL3Config() const {
    return 0;
}

template <>
void populateFactoryTable<CommandStreamReceiverHw<Family>>() {
    extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
    commandStreamReceiverFactory[gfxCore] = DeviceCommandStreamReceiver<Family>::create;
}

template <>
template <>
void BlitCommandsHelper<Family>::appendColorDepth(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd) {
    using XY_BLOCK_COPY_BLT = typename Family::XY_BLOCK_COPY_BLT;
    switch (blitProperties.bytesPerPixel) {
    default:
        UNRECOVERABLE_IF(true);
        break;
    case 1:
        blitCmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR);
        break;
    case 2:
        blitCmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR);
        break;
    case 4:
        blitCmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR);
        break;
    case 8:
        blitCmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_64_BIT_COLOR);
        break;
    case 16:
        blitCmd.setColorDepth(XY_BLOCK_COPY_BLT::COLOR_DEPTH::COLOR_DEPTH_128_BIT_COLOR);
        break;
    }
}

template <>
void BlitCommandsHelper<Family>::getBlitAllocationProperties(const GraphicsAllocation &allocation, uint32_t &pitch, uint32_t &qPitch,
                                                             GMM_TILE_TYPE &tileType, uint32_t &mipTailLod, uint32_t &compressionDetails,
                                                             const RootDeviceEnvironment &rootDeviceEnvironment, GMM_YUV_PLANE_ENUM plane) {
    if (allocation.getDefaultGmm()) {
        auto gmmResourceInfo = allocation.getDefaultGmm()->gmmResourceInfo.get();
        if (!gmmResourceInfo->getResourceFlags()->Info.Linear) {
            qPitch = gmmResourceInfo->getQPitch() ? static_cast<uint32_t>(gmmResourceInfo->getQPitch()) : qPitch;
            pitch = gmmResourceInfo->getRenderPitch() ? static_cast<uint32_t>(gmmResourceInfo->getRenderPitch()) : pitch;
        }
    }
}

template <>
void BlitCommandsHelper<Family>::appendSliceOffsets(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, uint32_t sliceIndex, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t srcSlicePitch, uint32_t dstSlicePitch) {
    auto srcAddress = blitProperties.srcGpuAddress;
    auto dstAddress = blitProperties.dstGpuAddress;

    blitCmd.setSourceBaseAddress(ptrOffset(srcAddress, srcSlicePitch * (sliceIndex + blitProperties.srcOffset.z)));
    blitCmd.setDestinationBaseAddress(ptrOffset(dstAddress, dstSlicePitch * (sliceIndex + blitProperties.dstOffset.z)));
}

template <>
void BlitCommandsHelper<Family>::appendBlitCommandsForImages(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t &srcSlicePitch, uint32_t &dstSlicePitch) {
    auto tileType = GMM_NOT_TILED;
    auto srcAllocation = blitProperties.srcAllocation;
    auto dstAllocation = blitProperties.dstAllocation;
    auto srcQPitch = static_cast<uint32_t>(blitProperties.srcSize.y);
    auto dstQPitch = static_cast<uint32_t>(blitProperties.dstSize.y);
    auto srcRowPitch = static_cast<uint32_t>(blitProperties.srcRowPitch);
    auto dstRowPitch = static_cast<uint32_t>(blitProperties.dstRowPitch);
    uint32_t mipTailLod = 0;
    auto compressionDetails = 0u;
    if (srcAllocation) {
        getBlitAllocationProperties(*srcAllocation, srcRowPitch, srcQPitch, tileType, mipTailLod, compressionDetails,
                                    rootDeviceEnvironment, blitProperties.srcPlane);
    }
    if (dstAllocation) {
        getBlitAllocationProperties(*dstAllocation, dstRowPitch, dstQPitch, tileType, mipTailLod, compressionDetails,
                                    rootDeviceEnvironment, blitProperties.dstPlane);
    }
    blitCmd.setSourcePitch(srcRowPitch);
    blitCmd.setDestinationPitch(dstRowPitch);

    srcSlicePitch = std::max(srcSlicePitch, srcRowPitch * srcQPitch);
    dstSlicePitch = std::max(dstSlicePitch, dstRowPitch * dstQPitch);
}

template <>
size_t BlitCommandsHelper<Family>::getNumberOfBlitsForByteFill(const Vec3<size_t> &copySize, size_t patternSize, const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed) {
    return NEO::BlitCommandsHelper<Family>::getNumberOfBlitsForFill(copySize, patternSize, rootDeviceEnvironment, isSystemMemoryPoolUsed);
}

template <>
BlitCommandsResult BlitCommandsHelper<Family>::dispatchBlitMemoryByteFill(const BlitProperties &blitProperties, LinearStream &linearStream, RootDeviceEnvironment &rootDeviceEnvironment) {
    return NEO::BlitCommandsHelper<Family>::dispatchBlitMemoryFill(blitProperties, linearStream, rootDeviceEnvironment);
}

template <>
void BlitCommandsHelper<Family>::appendBlitMemSetCompressionFormat(void *blitCmd, NEO::GraphicsAllocation *dstAlloc, uint32_t compressionFormat) {}

template <>
void BlitCommandsHelper<Family>::appendBlitMemoryOptionsForFillBuffer(NEO::GraphicsAllocation *dstAlloc, typename Family::XY_COLOR_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
}
template <>
void BlitCommandsHelper<Family>::appendTilingEnable(typename Family::XY_COLOR_BLT &blitCmd) {}

template <>
bool BlitCommandsHelper<Family>::preBlitCommandWARequired() {
    return true;
}

template class CommandStreamReceiverHw<Family>;
template struct BlitCommandsHelper<Family>;

template void BlitCommandsHelper<Family>::applyAdditionalBlitProperties<typename Family::XY_BLOCK_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool last);

const Family::GPGPU_WALKER Family::cmdInitGpgpuWalker = Family::GPGPU_WALKER::sInit();
const Family::INTERFACE_DESCRIPTOR_DATA Family::cmdInitInterfaceDescriptorData = Family::INTERFACE_DESCRIPTOR_DATA::sInit();
const Family::MEDIA_INTERFACE_DESCRIPTOR_LOAD Family::cmdInitMediaInterfaceDescriptorLoad = Family::MEDIA_INTERFACE_DESCRIPTOR_LOAD::sInit();
const Family::MEDIA_STATE_FLUSH Family::cmdInitMediaStateFlush = Family::MEDIA_STATE_FLUSH::sInit();
const Family::MI_BATCH_BUFFER_START Family::cmdInitBatchBufferStart = Family::MI_BATCH_BUFFER_START::sInit();
const Family::MI_BATCH_BUFFER_END Family::cmdInitBatchBufferEnd = Family::MI_BATCH_BUFFER_END::sInit();
const Family::PIPE_CONTROL Family::cmdInitPipeControl = Family::PIPE_CONTROL::sInit();
const Family::STATE_COMPUTE_MODE Family::cmdInitStateComputeMode = Family::STATE_COMPUTE_MODE::sInit();
const Family::MI_SEMAPHORE_WAIT Family::cmdInitMiSemaphoreWait = Family::MI_SEMAPHORE_WAIT::sInit();
const Family::RENDER_SURFACE_STATE Family::cmdInitRenderSurfaceState = Family::RENDER_SURFACE_STATE::sInit();
const Family::MI_LOAD_REGISTER_IMM Family::cmdInitLoadRegisterImm = Family::MI_LOAD_REGISTER_IMM::sInit();
const Family::MI_LOAD_REGISTER_REG Family::cmdInitLoadRegisterReg = Family::MI_LOAD_REGISTER_REG::sInit();
const Family::MI_LOAD_REGISTER_MEM Family::cmdInitLoadRegisterMem = Family::MI_LOAD_REGISTER_MEM::sInit();
const Family::MI_STORE_DATA_IMM Family::cmdInitStoreDataImm = Family::MI_STORE_DATA_IMM::sInit();
const Family::MI_STORE_REGISTER_MEM Family::cmdInitStoreRegisterMem = Family::MI_STORE_REGISTER_MEM::sInit();
const Family::MI_NOOP Family::cmdInitNoop = Family::MI_NOOP::sInit();
const Family::MI_REPORT_PERF_COUNT Family::cmdInitReportPerfCount = Family::MI_REPORT_PERF_COUNT::sInit();
const Family::MI_ATOMIC Family::cmdInitAtomic = Family::MI_ATOMIC::sInit();
const Family::PIPELINE_SELECT Family::cmdInitPipelineSelect = Family::PIPELINE_SELECT::sInit();
const Family::MI_ARB_CHECK Family::cmdInitArbCheck = Family::MI_ARB_CHECK::sInit();
const Family::MEDIA_VFE_STATE Family::cmdInitMediaVfeState = Family::MEDIA_VFE_STATE::sInit();
const Family::STATE_BASE_ADDRESS Family::cmdInitStateBaseAddress = Family::STATE_BASE_ADDRESS::sInit();
const Family::MEDIA_SURFACE_STATE Family::cmdInitMediaSurfaceState = Family::MEDIA_SURFACE_STATE::sInit();
const Family::SAMPLER_STATE Family::cmdInitSamplerState = Family::SAMPLER_STATE::sInit();
const Family::GPGPU_CSR_BASE_ADDRESS Family::cmdInitGpgpuCsrBaseAddress = Family::GPGPU_CSR_BASE_ADDRESS::sInit();
const Family::STATE_SIP Family::cmdInitStateSip = Family::STATE_SIP::sInit();
const Family::BINDING_TABLE_STATE Family::cmdInitBindingTableState = Family::BINDING_TABLE_STATE::sInit();
const Family::MI_USER_INTERRUPT Family::cmdInitUserInterrupt = Family::MI_USER_INTERRUPT::sInit();
const Family::L3_CONTROL Family::cmdInitL3ControlWithoutPostSync = Family::L3_CONTROL::sInit();
const Family::L3_CONTROL Family::cmdInitL3ControlWithPostSync = Family::L3_CONTROL::sInit();
const Family::XY_BLOCK_COPY_BLT Family::cmdInitXyBlockCopyBlt = Family::XY_BLOCK_COPY_BLT::sInit();
const Family::XY_COPY_BLT Family::cmdInitXyCopyBlt = Family::XY_COPY_BLT::sInit();
const Family::MI_FLUSH_DW Family::cmdInitMiFlushDw = Family::MI_FLUSH_DW::sInit();
const Family::XY_FAST_COLOR_BLT Family::cmdInitXyColorBlt = Family::XY_FAST_COLOR_BLT::sInit();
} // namespace NEO
