/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/state_base_address_base.inl"

namespace NEO {

template <typename GfxFamily>
void setSbaStatelessCompressionParams(typename GfxFamily::STATE_BASE_ADDRESS *stateBaseAddress, MemoryCompressionState memoryCompressionState) {
    using STATE_BASE_ADDRESS = typename GfxFamily::STATE_BASE_ADDRESS;

    if (memoryCompressionState == MemoryCompressionState::enabled) {
        stateBaseAddress->setEnableMemoryCompressionForAllStatelessAccesses(STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_ENABLED);
    } else {
        stateBaseAddress->setEnableMemoryCompressionForAllStatelessAccesses(STATE_BASE_ADDRESS::ENABLE_MEMORY_COMPRESSION_FOR_ALL_STATELESS_ACCESSES_DISABLED);
    }
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendStateBaseAddressParameters(
    StateBaseAddressHelperArgs<GfxFamily> &args) {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;

    if (args.sbaProperties) {
        if (args.sbaProperties->indirectObjectBaseAddress.value != StreamProperty64::initValue) {
            auto baseAddress = static_cast<uint64_t>(args.sbaProperties->indirectObjectBaseAddress.value);
            args.stateBaseAddressCmd->setGeneralStateBaseAddress(args.gmmHelper->decanonize(baseAddress));
            args.stateBaseAddressCmd->setGeneralStateBaseAddressModifyEnable(true);
            args.stateBaseAddressCmd->setGeneralStateBufferSizeModifyEnable(true);
            args.stateBaseAddressCmd->setGeneralStateBufferSize(0xfffff);
        }

        if (args.sbaProperties->dynamicStateBaseAddress.value != StreamProperty64::initValue) {
            args.stateBaseAddressCmd->setBindlessSamplerStateBaseAddress(args.sbaProperties->dynamicStateBaseAddress.value);
            args.stateBaseAddressCmd->setBindlessSamplerStateBufferSize(static_cast<uint32_t>(args.sbaProperties->dynamicStateSize.value));
            args.stateBaseAddressCmd->setBindlessSamplerStateBaseAddressModifyEnable(true);
        }
    }
    if (args.setGeneralStateBaseAddress && is64bit) {
        args.stateBaseAddressCmd->setGeneralStateBaseAddress(args.gmmHelper->decanonize(args.indirectObjectHeapBaseAddress));
    }

    if (!args.useGlobalHeapsBaseAddress) {
        if (args.bindlessSurfaceStateBaseAddress != 0) {
            args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddress(args.bindlessSurfaceStateBaseAddress);
            args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddressModifyEnable(true);
            const auto surfaceStateCount = getMaxBindlessSurfaceStates();
            args.stateBaseAddressCmd->setBindlessSurfaceStateSize(surfaceStateCount);
        } else if (args.ssh) {
            args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddress(args.ssh->getHeapGpuBase());
            args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddressModifyEnable(true);
            const auto surfaceStateCount = args.ssh->getMaxAvailableSpace() / sizeof(RENDER_SURFACE_STATE);
            args.stateBaseAddressCmd->setBindlessSurfaceStateSize(static_cast<uint32_t>(surfaceStateCount - 1));
        }

        if (args.dsh) {
            args.stateBaseAddressCmd->setBindlessSamplerStateBaseAddress(args.dsh->getHeapGpuBase());
            args.stateBaseAddressCmd->setBindlessSamplerStateBufferSize(args.dsh->getHeapSizeInPages());
            args.stateBaseAddressCmd->setBindlessSamplerStateBaseAddressModifyEnable(true);
        }
    } else {
        args.stateBaseAddressCmd->setBindlessSamplerStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setBindlessSamplerStateBaseAddress(args.globalHeapsBaseAddress);
        args.stateBaseAddressCmd->setBindlessSamplerStateBufferSize(MemoryConstants::sizeOf4GBinPageEntities);
    }

    auto &productHelper = args.gmmHelper->getRootDeviceEnvironment().template getHelper<ProductHelper>();

    auto heapResourceUsage = CacheSettingsHelper::getGmmUsageType(AllocationType::internalHeap, debugManager.flags.DisableCachingForHeaps.get(), productHelper, args.gmmHelper->getHardwareInfo());
    auto heapMocsValue = args.gmmHelper->getMOCS(heapResourceUsage);

    args.stateBaseAddressCmd->setSurfaceStateMemoryObjectControlState(heapMocsValue);
    args.stateBaseAddressCmd->setDynamicStateMemoryObjectControlState(heapMocsValue);
    args.stateBaseAddressCmd->setGeneralStateMemoryObjectControlState(heapMocsValue);
    args.stateBaseAddressCmd->setBindlessSurfaceStateMemoryObjectControlState(heapMocsValue);
    args.stateBaseAddressCmd->setBindlessSamplerStateMemoryObjectControlState(heapMocsValue);

    if (args.memoryCompressionState != MemoryCompressionState::notApplicable) {
        setSbaStatelessCompressionParams<GfxFamily>(args.stateBaseAddressCmd, args.memoryCompressionState);
    }

    bool l3MocsEnabled = (args.stateBaseAddressCmd->getStatelessDataPortAccessMemoryObjectControlState() >> 1) == (args.gmmHelper->getL3EnabledMOCS() >> 1);
    bool constMocsAllowed = (l3MocsEnabled && (debugManager.flags.ForceL1Caching.get() != 0));

    if (constMocsAllowed) {
        auto constMocsIndex = args.gmmHelper->getL1EnabledMOCS();
        GmmHelper::applyMocsEncryptionBit(constMocsIndex);

        args.stateBaseAddressCmd->setStatelessDataPortAccessMemoryObjectControlState(constMocsIndex);
    }

    appendExtraCacheSettings(args);
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(LinearStream &commandStream, uint64_t baseAddress, uint32_t sizeInPages, GmmHelper *gmmHelper) {
    if constexpr (GfxFamily::isHeaplessRequired() == false) {
        using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename GfxFamily::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

        auto bindingTablePoolAlloc = commandStream.getSpaceForCmd<_3DSTATE_BINDING_TABLE_POOL_ALLOC>();
        _3DSTATE_BINDING_TABLE_POOL_ALLOC cmd = GfxFamily::cmdInitStateBindingTablePoolAlloc;
        cmd.setBindingTablePoolBaseAddress(baseAddress);
        cmd.setBindingTablePoolBufferSize(sizeInPages);
        cmd.setSurfaceObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
        if (debugManager.flags.DisableCachingForHeaps.get()) {
            cmd.setSurfaceObjectControlStateIndexToMocsTables(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_SYSTEM_MEMORY_BUFFER_CACHELINE_MISALIGNED));
        }

        *bindingTablePoolAlloc = cmd;
    }
}

template <typename GfxFamily>
uint32_t StateBaseAddressHelper<GfxFamily>::getMaxBindlessSurfaceStates() {
    return std::numeric_limits<uint32_t>::max();
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendExtraCacheSettings(StateBaseAddressHelperArgs<GfxFamily> &args) {
    auto cachePolicy = args.isDebuggerActive ? args.l1CachePolicyDebuggerActive : args.l1CachePolicy;
    args.stateBaseAddressCmd->setL1CacheControlCachePolicy(static_cast<typename STATE_BASE_ADDRESS::L1_CACHE_CONTROL>(cachePolicy));

    if (debugManager.flags.ForceStatelessL1CachingPolicy.get() != -1 &&
        debugManager.flags.ForceAllResourcesUncached.get() == false) {
        args.stateBaseAddressCmd->setL1CacheControlCachePolicy(static_cast<typename STATE_BASE_ADDRESS::L1_CACHE_CONTROL>(debugManager.flags.ForceStatelessL1CachingPolicy.get()));
    }
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendIohParameters(StateBaseAddressHelperArgs<GfxFamily> &args) {
}

} // namespace NEO
