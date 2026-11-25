/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/state_base_address_base.inl"

namespace NEO {

template <typename GfxFamily>
void setSbaStatelessCompressionParams(typename GfxFamily::STATE_BASE_ADDRESS *stateBaseAddress, MemoryCompressionState memoryCompressionState) {
    stateBaseAddress->setEnableMemoryCompressionForAllStatelessAccesses(memoryCompressionState == MemoryCompressionState::enabled);
}

template <typename GfxFamily>
void setSbaBindlessSamplerState(typename GfxFamily::STATE_BASE_ADDRESS &stateBaseAddress, uint64_t baseAddress, uint32_t bufferSize) {
    if constexpr (requires { stateBaseAddress.setBindlessSamplerStateBaseAddress(baseAddress); }) {
        stateBaseAddress.setBindlessSamplerStateBaseAddress(baseAddress);
        stateBaseAddress.setBindlessSamplerStateBufferSize(bufferSize);
        stateBaseAddress.setBindlessSamplerStateBaseAddressModifyEnable(true);
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
            setSbaBindlessSamplerState<GfxFamily>(*args.stateBaseAddressCmd, args.sbaProperties->dynamicStateBaseAddress.value, static_cast<uint32_t>(args.sbaProperties->dynamicStateSize.value));
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
            setSbaBindlessSamplerState<GfxFamily>(*args.stateBaseAddressCmd, args.dsh->getHeapGpuBase(), args.dsh->getHeapSizeInPages());
        }
    } else {
        setSbaBindlessSamplerState<GfxFamily>(*args.stateBaseAddressCmd, args.globalHeapsBaseAddress, MemoryConstants::sizeOf4GBinPageEntities);
    }

    auto heapMocsValue = args.gmmHelper->getHeapMOCS();

    args.stateBaseAddressCmd->setSurfaceStateMemoryObjectControlState(heapMocsValue);
    args.stateBaseAddressCmd->setDynamicStateMemoryObjectControlState(heapMocsValue);
    args.stateBaseAddressCmd->setGeneralStateMemoryObjectControlState(heapMocsValue);
    args.stateBaseAddressCmd->setBindlessSurfaceStateMemoryObjectControlState(heapMocsValue);
    if constexpr (requires { args.stateBaseAddressCmd->setBindlessSamplerStateMemoryObjectControlState(heapMocsValue); }) {
        args.stateBaseAddressCmd->setBindlessSamplerStateMemoryObjectControlState(heapMocsValue);
    }

    if constexpr (GfxFamily::samplerArbitrationControl) {
        if (debugManager.flags.OverrideSamplerArbitrationControl.get() != -1) {
            auto overrideValue = static_cast<STATE_BASE_ADDRESS::SAMPLER_ARBITRATION_CONTROL>(debugManager.flags.OverrideSamplerArbitrationControl.get());
            args.stateBaseAddressCmd->setSamplerArbitrationControl(overrideValue);
        }
    }

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
        cmd.setSurfaceObjectControlStateIndexToMocsTables(gmmHelper->getHeapMOCS());

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
    args.stateBaseAddressCmd->setL1CacheControl(static_cast<typename STATE_BASE_ADDRESS::L1_CACHE_CONTROL>(cachePolicy));

    if (debugManager.flags.ForceStatelessL1CachingPolicy.get() != -1 &&
        debugManager.flags.ForceAllResourcesUncached.get() == false) {
        args.stateBaseAddressCmd->setL1CacheControl(static_cast<typename STATE_BASE_ADDRESS::L1_CACHE_CONTROL>(debugManager.flags.ForceStatelessL1CachingPolicy.get()));
    }
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::appendIohParameters(StateBaseAddressHelperArgs<GfxFamily> &args) {
}

} // namespace NEO
