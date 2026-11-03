/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"

#include "hw_cmds_xe3p_core.h"

using Family = NEO::Xe3pCoreFamily;

#include "shared/source/command_stream/command_stream_receiver_hw_dg2_and_later.inl"
#include "shared/source/command_stream/command_stream_receiver_hw_heapless_addressing.inl"
#include "shared/source/command_stream/command_stream_receiver_hw_xehp_and_later.inl"
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/blit_commands_helper_from_gen12lp_to_xe3p.inl"
#include "shared/source/helpers/blit_commands_helper_pvc_and_later.inl"
#include "shared/source/helpers/blit_commands_helper_xe2_and_later.inl"
#include "shared/source/helpers/blit_commands_helper_xehp_and_later.inl"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/populate_factory.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_pool.h"

namespace NEO {
static auto gfxCore = IGFX_XE3P_CORE;

template <>
bool ImplicitFlushSettings<Family>::defaultSettingForNewResource = false;
template <>
bool ImplicitFlushSettings<Family>::defaultSettingForGpuIdle = false;
template class ImplicitFlushSettings<Family>;

template <>
void populateFactoryTable<CommandStreamReceiverHw<Family>>() {
    extern CommandStreamReceiverCreateFunc commandStreamReceiverFactory[2 * NEO::maxCoreEnumValue];
    commandStreamReceiverFactory[gfxCore] = DeviceCommandStreamReceiver<Family>::create;
}

template <>
bool CommandStreamReceiverHw<Family>::isPerQueuePrologueEnabled() const {
    return false;
}

template <>
void CommandStreamReceiverHw<Family>::programEnginePrologue(LinearStream &csr) {
    if (!this->isEnginePrologueSent) {

        bool skip = (this->primaryCsr != nullptr);

        if (!skip && getGlobalFenceAllocation()) {
            EncodeMemoryFence<Family>::encodeSystemMemoryFence(csr, getGlobalFenceAllocation());
        }
        this->isEnginePrologueSent = true;
    }
}

template <>
size_t CommandStreamReceiverHw<Family>::getCmdSizeForPrologue() const {
    if (!this->isEnginePrologueSent) {
        bool skip = (this->primaryCsr != nullptr);
        if (!skip && getGlobalFenceAllocation()) {
            return EncodeMemoryFence<Family>::getSystemMemoryFenceSize();
        }
    }
    return 0u;
}

template <>
void CommandStreamReceiverHw<Family>::programFrontEndPrologue(LinearStream &csr) {
}

template <>
size_t CommandStreamReceiverHw<Family>::getFrontEndPrologueSize() const {
    return 0;
}

template <>
void CommandStreamReceiverHw<Family>::programStateBaseAddressHeapless(Device &device, LinearStream &commandStream) {
    using STATE_BASE_ADDRESS = typename Family::STATE_BASE_ADDRESS;

    uint32_t statelessMocsIndex = device.getGmmHelper()->getL3EnabledMOCS() >> 1;

    bool areMultipleSubDevicesInContext = device.getNumGenericSubDevices() > 1;
    bool multiOsContextCapable = isMultiOsContextCapable();
    bool isDebuggerActive = device.getDebugger() != nullptr;

    STATE_BASE_ADDRESS stateBaseAddressCmd;
    StateBaseAddressHelperArgs<Family> args = {
        0,                                              // generalStateBaseAddress
        0,                                              // indirectObjectHeapBaseAddress
        0,                                              // instructionHeapBaseAddress
        0,                                              // globalHeapsBaseAddress
        0,                                              // surfaceStateBaseAddress
        0,                                              // bindlessSurfaceStateBaseAddress
        &stateBaseAddressCmd,                           // stateBaseAddressCmd
        nullptr,                                        // sbaProperties
        nullptr,                                        // dsh
        nullptr,                                        // ioh
        nullptr,                                        // ssh
        device.getGmmHelper(),                          // gmmHelper
        statelessMocsIndex,                             // statelessMocsIndex
        this->l1CachePolicyData.getL1CacheValue(false), // l1CachePolicy
        this->l1CachePolicyData.getL1CacheValue(true),  // l1CachePolicyDebuggerActive
        MemoryCompressionState::notApplicable,          // memoryCompressionState
        false,                                          // setInstructionStateBaseAddress
        false,                                          // setGeneralStateBaseAddress
        false,                                          // useGlobalHeapsBaseAddress
        multiOsContextCapable,                          // isMultiOsContextCapable
        areMultipleSubDevicesInContext,                 // areMultipleSubDevicesInContext
        false,                                          // overrideSurfaceStateBaseAddress
        isDebuggerActive,                               // isDebuggerActive
        false,                                          // doubleSbaWa
        true                                            // heaplessModeEnabled
    };

    StateBaseAddressHelper<Family>::programStateBaseAddressIntoCommandStream(args, commandStream);

    setGSBAStateDirty(false);
    this->streamProperties.stateBaseAddress.clearIsDirty();
}

#include "shared/source/helpers/blit_commands_helper_from_xe2_hpg_to_xe3p.inl"

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
void BlitCommandsHelper<Family>::encodeProfilingStartMmios(LinearStream &cmdStream, const TagNodeBase &timestampPacketNode) {
    auto timestampContextStartGpuAddress = TimestampPacketHelper::getContextStartGpuAddress(timestampPacketNode);
    auto timestampGlobalStartAddress = TimestampPacketHelper::getGlobalStartGpuAddress(timestampPacketNode);

    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::queueTimestampRegAddressOffsetHigh, timestampContextStartGpuAddress + sizeof(uint32_t), false, nullptr, true);
    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::globalTimestampUn, timestampGlobalStartAddress + sizeof(uint32_t), false, nullptr, true);

    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::queueTimestampRegAddressOffsetLow, timestampContextStartGpuAddress, false, nullptr, true);
    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::globalTimestampLdw, timestampGlobalStartAddress, false, nullptr, true);
}

template <>
void BlitCommandsHelper<Family>::encodeProfilingEndMmios(LinearStream &cmdStream, const TagNodeBase &timestampPacketNode) {
    auto timestampContextEndGpuAddress = TimestampPacketHelper::getContextEndGpuAddress(timestampPacketNode);
    auto timestampGlobalEndAddress = TimestampPacketHelper::getGlobalEndGpuAddress(timestampPacketNode);

    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::queueTimestampRegAddressOffsetHigh, timestampContextEndGpuAddress + sizeof(uint32_t), false, nullptr, true);
    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::globalTimestampUn, timestampGlobalEndAddress + sizeof(uint32_t), false, nullptr, true);

    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::queueTimestampRegAddressOffsetLow, timestampContextEndGpuAddress, false, nullptr, true);
    EncodeStoreMMIO<Family>::encode(cmdStream, RegisterOffsets::globalTimestampLdw, timestampGlobalEndAddress, false, nullptr, true);
}

template <>
size_t BlitCommandsHelper<Family>::getProfilingMmioCmdsSize() {
    return 8 * sizeof(typename Family::MI_STORE_REGISTER_MEM);
}

template <>
template <typename CommandType>
void BlitCommandsHelper<Family>::applyAdditionalBlitProperties(const BlitProperties &blitProperties, CommandType &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool isLast, bool isFirst) {
}

template <>
void setCompressionParamsForFillOperation<Family>(typename Family::XY_COLOR_BLT &xyColorBlt) {
}

template class CommandStreamReceiverHw<Family>;
template struct BlitCommandsHelper<Family>;
template void BlitCommandsHelper<Family>::appendColorDepth<typename Family::XY_BLOCK_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd);
template void BlitCommandsHelper<Family>::appendColorDepth<typename Family::XY_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_COPY_BLT &blitCmd);
template void BlitCommandsHelper<Family>::appendBlitCommandsForBuffer<typename Family::XY_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment);

template void BlitCommandsHelper<Family>::applyAdditionalBlitProperties<typename Family::XY_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool isLast, bool isFirst);
template void BlitCommandsHelper<Family>::applyAdditionalBlitProperties<typename Family::XY_BLOCK_COPY_BLT>(const BlitProperties &blitProperties, typename Family::XY_BLOCK_COPY_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool isLast, bool isFirst);
template void BlitCommandsHelper<Family>::applyAdditionalBlitProperties<typename Family::XY_COLOR_BLT>(const BlitProperties &blitProperties, typename Family::XY_COLOR_BLT &blitCmd, const RootDeviceEnvironment &rootDeviceEnvironment, bool isLast, bool isFirst);

const Family::COMPUTE_WALKER_2 Family::cmdInitGpgpuWalker2 = Family::COMPUTE_WALKER_2::sInit();
const Family::INTERFACE_DESCRIPTOR_DATA_2 Family::cmdInitInterfaceDescriptorData2 = Family::INTERFACE_DESCRIPTOR_DATA_2::sInit();
const Family::MI_BATCH_BUFFER_START Family::cmdInitBatchBufferStart = Family::MI_BATCH_BUFFER_START::sInit();
const Family::MI_BATCH_BUFFER_END Family::cmdInitBatchBufferEnd = Family::MI_BATCH_BUFFER_END::sInit();
const Family::PIPE_CONTROL Family::cmdInitPipeControl = Family::PIPE_CONTROL::sInit();
const Family::RESOURCE_BARRIER Family::cmdInitResourceBarrier = Family::RESOURCE_BARRIER::sInit();
const Family::STATE_COMPUTE_MODE Family::cmdInitStateComputeMode = Family::STATE_COMPUTE_MODE::sInit();
const Family::MI_SEMAPHORE_WAIT Family::cmdInitMiSemaphoreWait = Family::MI_SEMAPHORE_WAIT::sInit();
const Family::MI_SEMAPHORE_WAIT_LEGACY Family::cmdInitMiSemaphoreWaitLegacy = Family::MI_SEMAPHORE_WAIT_LEGACY::sInit();
const Family::RENDER_SURFACE_STATE Family::cmdInitRenderSurfaceState = Family::RENDER_SURFACE_STATE::sInit();
const Family::POSTSYNC_DATA_2 Family::cmdInitPostSyncData = Family::POSTSYNC_DATA_2::sInit();
const Family::MI_SET_PREDICATE Family::cmdInitSetPredicate = Family::MI_SET_PREDICATE::sInit();
const Family::MI_LOAD_REGISTER_IMM Family::cmdInitLoadRegisterImm = Family::MI_LOAD_REGISTER_IMM::sInit();
const Family::MI_LOAD_REGISTER_REG Family::cmdInitLoadRegisterReg = Family::MI_LOAD_REGISTER_REG::sInit();
const Family::MI_LOAD_REGISTER_MEM Family::cmdInitLoadRegisterMem = Family::MI_LOAD_REGISTER_MEM::sInit();
const Family::MI_STORE_DATA_IMM Family::cmdInitStoreDataImm = Family::MI_STORE_DATA_IMM::sInit();
const Family::MI_STORE_REGISTER_MEM Family::cmdInitStoreRegisterMem = Family::MI_STORE_REGISTER_MEM::sInit();
const Family::MI_NOOP Family::cmdInitNoop = Family::MI_NOOP::sInit();
const Family::MI_ATOMIC Family::cmdInitAtomic = Family::MI_ATOMIC::sInit();
const Family::PIPELINE_SELECT Family::cmdInitPipelineSelect = Family::PIPELINE_SELECT::sInit();
const Family::MI_ARB_CHECK Family::cmdInitArbCheck = Family::MI_ARB_CHECK::sInit();
const Family::STATE_BASE_ADDRESS Family::cmdInitStateBaseAddress = Family::STATE_BASE_ADDRESS::sInit();
const Family::SAMPLER_STATE Family::cmdInitSamplerState = Family::SAMPLER_STATE::sInit();
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
const Family::STATE_CONTEXT_DATA_BASE_ADDRESS Family::cmdInitStateContextDataBaseAddress = Family::STATE_CONTEXT_DATA_BASE_ADDRESS::sInit();
const Family::STATE_SYSTEM_MEM_FENCE_ADDRESS Family::cmdInitStateSystemMemFenceAddress = Family::STATE_SYSTEM_MEM_FENCE_ADDRESS::sInit();
} // namespace NEO
