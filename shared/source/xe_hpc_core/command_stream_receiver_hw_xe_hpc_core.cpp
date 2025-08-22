/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/resource_info.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/source/xe_hpc_core/hw_info.h"

using Family = NEO::XeHpcCoreFamily;

#include "shared/source/command_stream/command_stream_receiver_hw_dg2_and_later.inl"
#include "shared/source/command_stream/command_stream_receiver_hw_heap_addressing.inl"
#include "shared/source/command_stream/command_stream_receiver_hw_xehp_and_later.inl"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/blit_commands_helper_from_gen12lp_to_xe3.inl"
#include "shared/source/helpers/blit_commands_helper_pvc_and_later.inl"
#include "shared/source/helpers/blit_commands_helper_xehp_and_later.inl"
#include "shared/source/helpers/blit_properties.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/helpers/state_base_address_xehp_and_later.inl"

namespace NEO {
static auto gfxCore = IGFX_XE_HPC_CORE;

template <>
bool ImplicitFlushSettings<Family>::defaultSettingForNewResource = false;
template <>
bool ImplicitFlushSettings<Family>::defaultSettingForGpuIdle = false;
template class ImplicitFlushSettings<Family>;

template <>
void populateFactoryTable<CommandStreamReceiverHw<Family>>() {
    extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * IGFX_MAX_CORE];
    commandStreamReceiverFactory[gfxCore] = DeviceCommandStreamReceiver<Family>::create;
}

template <>
void CommandStreamReceiverHw<Family>::programEnginePrologue(LinearStream &csr) {
    if (!this->isEnginePrologueSent) {
        if (getGlobalFenceAllocation()) {
            EncodeMemoryFence<Family>::encodeSystemMemoryFence(csr, getGlobalFenceAllocation());
        }
        this->isEnginePrologueSent = true;
    }
}

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForPrologue() const {
    if (!this->isEnginePrologueSent) {
        if (getGlobalFenceAllocation()) {
            return EncodeMemoryFence<Family>::getSystemMemoryFenceSize();
        }
    }
    return 0u;
}

template <>
void BlitCommandsHelper<Family>::appendBlitCommandsBlockCopy(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
}

template <>
template <typename T>
void BlitCommandsHelper<Family>::appendBlitCommandsForBuffer(const BlitProperties &blitProperties, T &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment) {
    appendBlitCommandsMemCopy(blitProperties, blitCmd, rootDeviceEnvironment);
}

template <>
uint32_t BlitCommandsHelper<Family>::getAvailableBytesPerPixel(size_t copySize, uint32_t srcOrigin, uint32_t dstOrigin, size_t srcSize, size_t dstSize) {
    return 1;
}

template <>
void BlitCommandsHelper<Family>::appendBlitCommandsMemCopy(const BlitProperties &blitProperties, typename Family::XY_COPY_BLT &blitCmd,
                                                           const RootDeviceEnvironment &rootDeviceEnvironment) {
    using MEM_COPY = typename Family::MEM_COPY;

    auto dstAllocation = blitProperties.dstAllocation;
    auto srcAllocation = blitProperties.srcAllocation;

    if (blitCmd.getDestinationY2CoordinateBottom() > 1) {
        blitCmd.setCopyType(MEM_COPY::COPY_TYPE::COPY_TYPE_MATRIX_COPY);
    } else {
        blitCmd.setCopyType(MEM_COPY::COPY_TYPE::COPY_TYPE_LINEAR_COPY);
    }

    auto cachePolicy = GMM_RESOURCE_USAGE_OCL_BUFFER;
    // if transfer size bigger then L3 size, copy with L3 disabled
    if (blitProperties.copySize.x * blitProperties.copySize.y * blitProperties.copySize.z * blitProperties.bytesPerPixel >= (rootDeviceEnvironment.getHardwareInfo()->gtSystemInfo.L3CacheSizeInKb * MemoryConstants::kiloByte / 2)) {
        cachePolicy = GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED;
    }

    auto mocs = rootDeviceEnvironment.getGmmHelper()->getMOCS(cachePolicy);

    if (debugManager.flags.OverrideBlitterMocs.get() != -1) {
        mocs = static_cast<uint32_t>(debugManager.flags.OverrideBlitterMocs.get());
    }

    blitCmd.setDestinationMOCS(mocs);
    blitCmd.setSourceMOCS(mocs);

    if (dstAllocation) {
        if (dstAllocation->isCompressionEnabled()) {
            auto resourceFormat = dstAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
            auto compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
            blitCmd.setDestinationCompressible(MEM_COPY::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
            blitCmd.setCompressionFormat(compressionFormat);
        }
    }
    if (srcAllocation) {
        if (srcAllocation->isCompressionEnabled()) {
            auto resourceFormat = srcAllocation->getDefaultGmm()->gmmResourceInfo->getResourceFormat();
            auto compressionFormat = rootDeviceEnvironment.getGmmClientContext()->getSurfaceStateCompressionFormat(resourceFormat);
            blitCmd.setSourceCompressible(MEM_COPY::SOURCE_COMPRESSIBLE::SOURCE_COMPRESSIBLE_COMPRESSIBLE);
            blitCmd.setCompressionFormat(compressionFormat);
        }
    }

    if (debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.get()) {
        if (srcAllocation) {
            if (!MemoryPoolHelper::isSystemMemoryPool(srcAllocation->getMemoryPool())) {
                blitCmd.setSourceCompressible(MEM_COPY::SOURCE_COMPRESSIBLE::SOURCE_COMPRESSIBLE_COMPRESSIBLE);
                blitCmd.setCompressionFormat(debugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get());
            }
        }
        if (dstAllocation) {
            if (!MemoryPoolHelper::isSystemMemoryPool(dstAllocation->getMemoryPool())) {
                blitCmd.setDestinationCompressible(MEM_COPY::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
                blitCmd.setCompressionFormat(debugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get());
            }
        }
    }

    if (blitCmd.getDestinationCompressible() == MEM_COPY::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE &&
        AuxTranslationDirection::auxToNonAux != blitProperties.auxTranslationDirection) {
        blitCmd.setDestinationCompressionEnable(MEM_COPY::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_ENABLE);
    } else {
        blitCmd.setDestinationCompressionEnable(MEM_COPY::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_DISABLE);
    }

    DEBUG_BREAK_IF((AuxTranslationDirection::none != blitProperties.auxTranslationDirection) &&
                   (dstAllocation != srcAllocation || !dstAllocation->isCompressionEnabled()));
}

template <>
void BlitCommandsHelper<Family>::appendBlitMemSetCompressionFormat(void *blitCmd, NEO::GraphicsAllocation *dstAlloc, uint32_t compressionFormat) {
    using MEM_SET = typename Family::MEM_SET;

    auto memSetCmd = reinterpret_cast<MEM_SET *>(blitCmd);

    if (dstAlloc->isCompressionEnabled()) {
        memSetCmd->setDestinationCompressible(MEM_SET::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
        memSetCmd->setCompressionFormat40(compressionFormat);
    }

    if (debugManager.flags.EnableStatelessCompressionWithUnifiedMemory.get()) {
        if (!MemoryPoolHelper::isSystemMemoryPool(dstAlloc->getMemoryPool())) {
            memSetCmd->setDestinationCompressible(MEM_SET::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE);
            memSetCmd->setCompressionFormat40(debugManager.flags.FormatForStatelessCompressionWithUnifiedMemory.get());
        }
    }

    if (memSetCmd->getDestinationCompressible() == MEM_SET::DESTINATION_COMPRESSIBLE::DESTINATION_COMPRESSIBLE_COMPRESSIBLE) {
        memSetCmd->setDestinationCompressionEnable(MEM_SET::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_ENABLE);
    } else {
        memSetCmd->setDestinationCompressionEnable(MEM_SET::DESTINATION_COMPRESSION_ENABLE::DESTINATION_COMPRESSION_ENABLE_DISABLE);
    }
}

template <>
void BlitCommandsHelper<Family>::appendBlitCommandsForImages(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t &srcSlicePitch, uint32_t &dstSlicePitch) {}

template <>
void BlitCommandsHelper<Family>::appendTilingType(const GMM_TILE_TYPE srcTilingType, const GMM_TILE_TYPE dstTilingType, typename Family::XY_BLOCK_COPY_BLT &blitCmd) {}

template <>
void BlitCommandsHelper<Family>::appendSliceOffsets(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, uint32_t sliceIndex, const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t srcSlicePitch, uint32_t dstSlicePitch) {}

template <>
void BlitCommandsHelper<Family>::appendSurfaceType(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd) {
}

template <>
template <>
void BlitCommandsHelper<Family>::appendColorDepth(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd) {}

template <>
template <>
void BlitCommandsHelper<Family>::appendColorDepth(const BlitProperties &blitProperties, typename Family::XY_COPY_BLT &blitCmd) {}

template <>
void BlitCommandsHelper<Family>::encodeWa(LinearStream &cmdStream, const BlitProperties &blitProperties, uint32_t &latestSentBcsWaValue) {
    using MI_LOAD_REGISTER_IMM = typename Family::MI_LOAD_REGISTER_IMM;

    if (debugManager.flags.EnableBcsSwControlWa.get() <= 0) {
        return;
    }

    constexpr int32_t srcInSystemMemOnly = 1;
    constexpr int32_t dstInSystemMemOnly = 2;
    constexpr int32_t enableAlways = 4;
    constexpr uint32_t waEnabledMMioValue = 0x40004;
    constexpr uint32_t waDisabledMMioValue = 0x40000;

    const bool applyForSrc = (debugManager.flags.EnableBcsSwControlWa.get() & srcInSystemMemOnly);
    const bool applyForDst = (debugManager.flags.EnableBcsSwControlWa.get() & dstInSystemMemOnly);
    const bool applyAlways = (debugManager.flags.EnableBcsSwControlWa.get() == enableAlways);

    const bool enableWa = (!blitProperties.srcAllocation->isAllocatedInLocalMemoryPool() && applyForSrc) ||
                          (!blitProperties.dstAllocation->isAllocatedInLocalMemoryPool() && applyForDst) ||
                          applyAlways;

    uint32_t newValue = enableWa ? waEnabledMMioValue : waDisabledMMioValue;

    if (newValue == latestSentBcsWaValue) {
        return;
    }

    latestSentBcsWaValue = newValue;

    MI_LOAD_REGISTER_IMM cmd = Family::cmdInitLoadRegisterImm;
    cmd.setRegisterOffset(0x22200);
    cmd.setDataDword(newValue);
    cmd.setMmioRemapEnable(true);

    auto lri = cmdStream.getSpaceForCmd<MI_LOAD_REGISTER_IMM>();
    *lri = cmd;
}

template <>
size_t BlitCommandsHelper<Family>::getWaCmdsSize(const BlitPropertiesContainer &blitPropertiesContainer) {
    using MI_LOAD_REGISTER_IMM = typename Family::MI_LOAD_REGISTER_IMM;

    if (debugManager.flags.EnableBcsSwControlWa.get() <= 0) {
        return 0;
    }

    return (blitPropertiesContainer.size() * sizeof(MI_LOAD_REGISTER_IMM));
}

template <>
uint64_t BlitCommandsHelper<Family>::getMaxBlitHeightOverride(const RootDeviceEnvironment &rootDeviceEnvironment, bool isSystemMemoryPoolUsed) {
    if (isSystemMemoryPoolUsed) {
        return 512;
    }
    return 0;
}

template <>
void BlitCommandsHelper<Family>::dispatchDummyBlit(LinearStream &linearStream, EncodeDummyBlitWaArgs &waArgs) {
    using MEM_SET = typename Family::MEM_SET;

    if (BlitCommandsHelper<Family>::isDummyBlitWaNeeded(waArgs)) {
        auto blitCmd = Family::cmdInitMemSet;
        auto &rootDeviceEnvironment = waArgs.rootDeviceEnvironment;

        rootDeviceEnvironment->initDummyAllocation();
        auto dummyAllocation = rootDeviceEnvironment->getDummyAllocation();
        blitCmd.setDestinationStartAddress(dummyAllocation->getGpuAddress());

        constexpr uint32_t memSetSize = 32 * MemoryConstants::kiloByte;
        blitCmd.setFillWidth(memSetSize);
        blitCmd.setDestinationPitch(memSetSize);

        auto cmd = linearStream.getSpaceForCmd<MEM_SET>();
        *cmd = blitCmd;
    }
}

template <>
size_t BlitCommandsHelper<Family>::getDummyBlitSize(const EncodeDummyBlitWaArgs &waArgs) {
    if (BlitCommandsHelper<Family>::isDummyBlitWaNeeded(waArgs)) {
        return sizeof(typename Family::MEM_SET);
    }
    return 0u;
}

template class CommandStreamReceiverHw<Family>;
template struct BlitCommandsHelper<Family>;
template void BlitCommandsHelper<Family>::appendBlitCommandsForBuffer<typename Family::XY_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment);

template void BlitCommandsHelper<Family>::applyAdditionalBlitProperties<typename Family::XY_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool last);

const Family::COMPUTE_WALKER Family::cmdInitGpgpuWalker = Family::COMPUTE_WALKER::sInit();
const Family::CFE_STATE Family::cmdInitCfeState = Family::CFE_STATE::sInit();
const Family::INTERFACE_DESCRIPTOR_DATA Family::cmdInitInterfaceDescriptorData = Family::INTERFACE_DESCRIPTOR_DATA::sInit();
const Family::MI_BATCH_BUFFER_START Family::cmdInitBatchBufferStart = Family::MI_BATCH_BUFFER_START::sInit();
const Family::MI_BATCH_BUFFER_END Family::cmdInitBatchBufferEnd = Family::MI_BATCH_BUFFER_END::sInit();
const Family::PIPE_CONTROL Family::cmdInitPipeControl = Family::PIPE_CONTROL::sInit();
const Family::STATE_COMPUTE_MODE Family::cmdInitStateComputeMode = Family::STATE_COMPUTE_MODE::sInit();
const Family::_3DSTATE_BINDING_TABLE_POOL_ALLOC Family::cmdInitStateBindingTablePoolAlloc =
    Family::_3DSTATE_BINDING_TABLE_POOL_ALLOC::sInit();
const Family::MI_SEMAPHORE_WAIT Family::cmdInitMiSemaphoreWait = Family::MI_SEMAPHORE_WAIT::sInit();
const Family::RENDER_SURFACE_STATE Family::cmdInitRenderSurfaceState = Family::RENDER_SURFACE_STATE::sInit();
const Family::POSTSYNC_DATA Family::cmdInitPostSyncData = Family::POSTSYNC_DATA::sInit();
const Family::MI_SET_PREDICATE Family::cmdInitSetPredicate = Family::MI_SET_PREDICATE::sInit();
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
const Family::STATE_BASE_ADDRESS Family::cmdInitStateBaseAddress = Family::STATE_BASE_ADDRESS::sInit();
const Family::MEDIA_SURFACE_STATE Family::cmdInitMediaSurfaceState = Family::MEDIA_SURFACE_STATE::sInit();
const Family::SAMPLER_STATE Family::cmdInitSamplerState = Family::SAMPLER_STATE::sInit();
const Family::BINDING_TABLE_STATE Family::cmdInitBindingTableState = Family::BINDING_TABLE_STATE::sInit();
const Family::MI_USER_INTERRUPT Family::cmdInitUserInterrupt = Family::MI_USER_INTERRUPT::sInit();
const Family::MI_CONDITIONAL_BATCH_BUFFER_END cmdInitConditionalBatchBufferEnd = Family::MI_CONDITIONAL_BATCH_BUFFER_END::sInit();
const Family::MI_FLUSH_DW Family::cmdInitMiFlushDw = Family::MI_FLUSH_DW::sInit();
const Family::XY_BLOCK_COPY_BLT Family::cmdInitXyBlockCopyBlt = Family::XY_BLOCK_COPY_BLT::sInit();
const Family::MEM_COPY Family::cmdInitXyCopyBlt = Family::MEM_COPY::sInit();
const Family::XY_FAST_COLOR_BLT Family::cmdInitXyColorBlt = Family::XY_FAST_COLOR_BLT::sInit();
const Family::STATE_PREFETCH Family::cmdInitStatePrefetch = Family::STATE_PREFETCH::sInit();
const Family::_3DSTATE_BTD Family::cmd3dStateBtd = Family::_3DSTATE_BTD::sInit();
const Family::MI_MEM_FENCE Family::cmdInitMemFence = Family::MI_MEM_FENCE::sInit();
const Family::MEM_SET Family::cmdInitMemSet = Family::MEM_SET::sInit();
const Family::STATE_SIP Family::cmdInitStateSip = Family::STATE_SIP::sInit();
const Family::STATE_SYSTEM_MEM_FENCE_ADDRESS Family::cmdInitStateSystemMemFenceAddress = Family::STATE_SYSTEM_MEM_FENCE_ADDRESS::sInit();
} // namespace NEO
