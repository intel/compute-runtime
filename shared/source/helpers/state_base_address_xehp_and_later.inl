/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/state_base_address_base.inl"

namespace NEO {

template <typename GfxFamily>
void setSbaStatelessCompressionParams(typename GfxFamily::STATE_BASE_ADDRESS *stateBaseAddress, MemoryCompressionState memoryCompressionState) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    if (memoryCompressionState == MemoryCompressionState::Enabled) {
        stateBaseAddress->setEnableMemoryCompressionForAllStatelessAccesses(STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_ENABLED);
    } else {
        stateBaseAddress->setEnableMemoryCompressionForAllStatelessAccesses(STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_DISABLED);
    }
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendStateBaseAddressParameters(
    StateBaseAddressHelperArgs<GfxFamily> &args,
    bool overrideBindlessSurfaceStateBase) {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    if (args.setGeneralStateBaseAddress && is64bit) {
        args.stateBaseAddressCmd->setGeneralStateBaseAddress(args.gmmHelper->decanonize(args.indirectObjectHeapBaseAddress));
    }

    if (overrideBindlessSurfaceStateBase && args.ssh) {
        args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddress(args.ssh->getHeapGpuBase());
        args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddressModifyEnable(true);
        const auto surfaceStateCount = args.ssh->getMaxAvailableSpace() / sizeof(RENDER_SURFACE_STATE);
        args.stateBaseAddressCmd->setBindlessSurfaceStateSize(static_cast<uint32_t>(surfaceStateCount - 1));
    }

    args.stateBaseAddressCmd->setBindlessSamplerStateBaseAddressModifyEnable(true);

    auto heapResourceUsage = CacheSettingsHelper::getGmmUsageType(AllocationType::INTERNAL_HEAP, DebugManager.flags.DisableCachingForHeaps.get(), *args.gmmHelper->getHardwareInfo());
    auto heapMocsValue = args.gmmHelper->getMOCS(heapResourceUsage);

    args.stateBaseAddressCmd->setSurfaceStateMemoryObjectControlState(heapMocsValue);
    args.stateBaseAddressCmd->setDynamicStateMemoryObjectControlState(heapMocsValue);
    args.stateBaseAddressCmd->setGeneralStateMemoryObjectControlState(heapMocsValue);
    args.stateBaseAddressCmd->setBindlessSurfaceStateMemoryObjectControlState(heapMocsValue);
    args.stateBaseAddressCmd->setBindlessSamplerStateMemoryObjectControlState(heapMocsValue);

    bool enableMultiGpuAtomics = args.isMultiOsContextCapable;
    if (DebugManager.flags.EnableMultiGpuAtomicsOptimization.get()) {
        enableMultiGpuAtomics = args.useGlobalAtomics && (args.isMultiOsContextCapable || args.areMultipleSubDevicesInContext);
    }
    args.stateBaseAddressCmd->setDisableSupportForMultiGpuAtomicsForStatelessAccesses(!enableMultiGpuAtomics);

    args.stateBaseAddressCmd->setDisableSupportForMultiGpuPartialWritesForStatelessMessages(!args.isMultiOsContextCapable);

    if (DebugManager.flags.ForceMultiGpuAtomics.get() != -1) {
        args.stateBaseAddressCmd->setDisableSupportForMultiGpuAtomicsForStatelessAccesses(!!DebugManager.flags.ForceMultiGpuAtomics.get());
    }

    if (DebugManager.flags.ForceMultiGpuPartialWrites.get() != -1) {
        args.stateBaseAddressCmd->setDisableSupportForMultiGpuPartialWritesForStatelessMessages(!!DebugManager.flags.ForceMultiGpuPartialWrites.get());
    }

    if (args.memoryCompressionState != MemoryCompressionState::NotApplicable) {
        setSbaStatelessCompressionParams<GfxFamily>(args.stateBaseAddressCmd, args.memoryCompressionState);
    }

    if (args.stateBaseAddressCmd->getStatelessDataPortAccessMemoryObjectControlState() == args.gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) && DebugManager.flags.ForceL1Caching.get() != 0) {
        args.stateBaseAddressCmd->setStatelessDataPortAccessMemoryObjectControlState(args.gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST));
    }

    appendExtraCacheSettings(args.stateBaseAddressCmd, args.gmmHelper->getHardwareInfo());
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(LinearStream &commandStream, uint64_t baseAddress, uint32_t sizeInPages, GmmHelper *gmmHelper) {
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename GfxFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto bindingTablePoolAlloc = commandStream.getSpaceForCmd<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    _3DSTATE_BINDING_TABLE_POOL_ALLOC cmd = GfxFamily::cmdInitStateBindingTablePoolAlloc;
    cmd.setBindingTablePoolBaseAddress(baseAddress);
    cmd.setBindingTablePoolBufferSize(sizeInPages);
    cmd.setSurfaceObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    if (DebugManager.flags.DisableCachingForHeaps.get()) {
        cmd.setSurfaceObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED));
    }

    *bindingTablePoolAlloc = cmd;
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendIohParameters(StateBaseAddressHelperArgs<GfxFamily> &args) {
}

template <typename GfxFamily>
uint32_t StateBaseAddressHelper<GfxFamily>::getMaxBindlessSurfaceStates() {
    return std::numeric_limits<uint32_t>::max();
}
} // namespace NEO
