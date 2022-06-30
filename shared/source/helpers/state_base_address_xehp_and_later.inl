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
    STATE_BASE_ADDRESS *stateBaseAddress,
    const IndirectHeap *ssh,
    bool setGeneralStateBaseAddress,
    uint64_t internalHeapBase,
    GmmHelper *gmmHelper,
    bool isMultiOsContextCapable,
    MemoryCompressionState memoryCompressionState,
    bool overrideBindlessSurfaceStateBase,
    bool useGlobalAtomics,
    bool areMultipleSubDevicesInContext) {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    if (setGeneralStateBaseAddress && is64bit) {
        stateBaseAddress->setGeneralStateBaseAddress(gmmHelper->decanonize(internalHeapBase));
    }

    if (overrideBindlessSurfaceStateBase && ssh) {
        stateBaseAddress->setBindlessSurfaceStateBaseAddress(ssh->getHeapGpuBase());
        stateBaseAddress->setBindlessSurfaceStateBaseAddressModifyEnable(true);
        const auto surfaceStateCount = ssh->getMaxAvailableSpace() / sizeof(RENDER_SURFACE_STATE);
        stateBaseAddress->setBindlessSurfaceStateSize(static_cast<uint32_t>(surfaceStateCount - 1));
    }

    stateBaseAddress->setBindlessSamplerStateBaseAddressModifyEnable(true);

    auto heapResourceUsage = CacheSettingsHelper::getGmmUsageType(AllocationType::INTERNAL_HEAP, DebugManager.flags.DisableCachingForHeaps.get(), *gmmHelper->getHardwareInfo());
    auto heapMocsValue = gmmHelper->getMOCS(heapResourceUsage);

    stateBaseAddress->setSurfaceStateMemoryObjectControlState(heapMocsValue);
    stateBaseAddress->setDynamicStateMemoryObjectControlState(heapMocsValue);
    stateBaseAddress->setGeneralStateMemoryObjectControlState(heapMocsValue);
    stateBaseAddress->setBindlessSurfaceStateMemoryObjectControlState(heapMocsValue);
    stateBaseAddress->setBindlessSamplerStateMemoryObjectControlState(heapMocsValue);

    bool enableMultiGpuAtomics = isMultiOsContextCapable;
    if (DebugManager.flags.EnableMultiGpuAtomicsOptimization.get()) {
        enableMultiGpuAtomics = useGlobalAtomics && (isMultiOsContextCapable || areMultipleSubDevicesInContext);
    }
    stateBaseAddress->setDisableSupportForMultiGpuAtomicsForStatelessAccesses(!enableMultiGpuAtomics);

    stateBaseAddress->setDisableSupportForMultiGpuPartialWritesForStatelessMessages(!isMultiOsContextCapable);

    if (DebugManager.flags.ForceMultiGpuAtomics.get() != -1) {
        stateBaseAddress->setDisableSupportForMultiGpuAtomicsForStatelessAccesses(!!DebugManager.flags.ForceMultiGpuAtomics.get());
    }

    if (DebugManager.flags.ForceMultiGpuPartialWrites.get() != -1) {
        stateBaseAddress->setDisableSupportForMultiGpuPartialWritesForStatelessMessages(!!DebugManager.flags.ForceMultiGpuPartialWrites.get());
    }

    if (memoryCompressionState != MemoryCompressionState::NotApplicable) {
        setSbaStatelessCompressionParams<GfxFamily>(stateBaseAddress, memoryCompressionState);
    }

    if (stateBaseAddress->getStatelessDataPortAccessMemoryObjectControlState() == gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) && DebugManager.flags.ForceL1Caching.get() != 0) {
        stateBaseAddress->setStatelessDataPortAccessMemoryObjectControlState(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CONST));
    }

    appendExtraCacheSettings(stateBaseAddress, gmmHelper->getHardwareInfo());
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(LinearStream &commandStream, const IndirectHeap &ssh, GmmHelper *gmmHelper) {
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename GfxFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    auto bindingTablePoolAlloc = commandStream.getSpaceForCmd<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
    _3DSTATE_BINDING_TABLE_POOL_ALLOC cmd = GfxFamily::cmdInitStateBindingTablePoolAlloc;
    cmd.setBindingTablePoolBaseAddress(ssh.getHeapGpuBase());
    cmd.setBindingTablePoolBufferSize(ssh.getHeapSizeInPages());
    cmd.setSurfaceObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
    if (DebugManager.flags.DisableCachingForHeaps.get()) {
        cmd.setSurfaceObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED));
    }

    *bindingTablePoolAlloc = cmd;
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendIohParameters(STATE_BASE_ADDRESS *stateBaseAddress, const IndirectHeap *ioh, bool useGlobalHeapsBaseAddress, uint64_t indirectObjectHeapBaseAddress) {
}

template <typename GfxFamily>
uint32_t StateBaseAddressHelper<GfxFamily>::getMaxBindlessSurfaceStates() {
    return std::numeric_limits<uint32_t>::max();
}
} // namespace NEO
