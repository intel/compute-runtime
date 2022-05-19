/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"

using Family = NEO::TGLLPFamily;

#include "shared/source/command_stream/command_stream_receiver_hw_bdw_and_later.inl"
#include "shared/source/command_stream/device_command_stream.h"
#include "shared/source/helpers/blit_commands_helper_bdw_and_later.inl"
#include "shared/source/helpers/populate_factory.h"

namespace NEO {
static auto gfxCore = IGFX_GEN12LP_CORE;

template <>
void CommandStreamReceiverHw<Family>::programL3(LinearStream &csr, uint32_t &newL3Config) {
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
void BlitCommandsHelper<Family>::appendColorDepth(const BlitProperties &blitProperites, typename Family::XY_BLOCK_COPY_BLT &blitCmd) {
    using XY_BLOCK_COPY_BLT = typename Family::XY_BLOCK_COPY_BLT;
    switch (blitProperites.bytesPerPixel) {
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
                                                             uint32_t &compressionType, const RootDeviceEnvironment &rootDeviceEnvironment,
                                                             GMM_YUV_PLANE_ENUM plane) {
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
    using XY_BLOCK_COPY_BLT = typename Family::XY_BLOCK_COPY_BLT;
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
    auto compressionType = 0u;

    getBlitAllocationProperties(*srcAllocation, srcRowPitch, srcQPitch, tileType, mipTailLod, compressionDetails,
                                compressionType, rootDeviceEnvironment, blitProperties.srcPlane);
    getBlitAllocationProperties(*dstAllocation, dstRowPitch, dstQPitch, tileType, mipTailLod, compressionDetails,
                                compressionType, rootDeviceEnvironment, blitProperties.dstPlane);

    blitCmd.setSourcePitch(srcRowPitch);
    blitCmd.setDestinationPitch(dstRowPitch);

    srcSlicePitch = std::max(srcSlicePitch, srcRowPitch * srcQPitch);
    dstSlicePitch = std::max(dstSlicePitch, dstRowPitch * dstQPitch);
}

template <>
void BlitCommandsHelper<Family>::dispatchBlitMemoryColorFill(NEO::GraphicsAllocation *dstAlloc, uint64_t offset, uint32_t *pattern, size_t patternSize, LinearStream &linearStream, size_t size, const RootDeviceEnvironment &rootDeviceEnvironment) {
    switch (patternSize) {
    case 1:
        NEO::BlitCommandsHelper<Family>::dispatchBlitMemoryFill<1>(dstAlloc, offset, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_8_BIT_COLOR);
        break;
    case 2:
        NEO::BlitCommandsHelper<Family>::dispatchBlitMemoryFill<2>(dstAlloc, offset, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_16_BIT_COLOR);
        break;
    case 4:
        NEO::BlitCommandsHelper<Family>::dispatchBlitMemoryFill<4>(dstAlloc, offset, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_32_BIT_COLOR);
        break;
    case 8:
        NEO::BlitCommandsHelper<Family>::dispatchBlitMemoryFill<8>(dstAlloc, offset, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_64_BIT_COLOR);
        break;
    default:
        NEO::BlitCommandsHelper<Family>::dispatchBlitMemoryFill<16>(dstAlloc, offset, pattern, linearStream, size, rootDeviceEnvironment, COLOR_DEPTH::COLOR_DEPTH_128_BIT_COLOR);
    }
}

template <>
void BlitCommandsHelper<Family>::appendBlitCommandsForFillBuffer(NEO::GraphicsAllocation *dstAlloc, typename Family::XY_COLOR_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
}
template <>
void BlitCommandsHelper<Family>::appendTilingEnable(typename Family::XY_COLOR_BLT &blitCmd) {}

template <>
bool BlitCommandsHelper<Family>::preBlitCommandWARequired() {
    return true;
}

template class CommandStreamReceiverHw<Family>;
template struct BlitCommandsHelper<Family>;

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
