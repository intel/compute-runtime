/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/memory_compression_state.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/cache_settings_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/cache_policy.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/os_interface/product_helper.h"

namespace NEO {

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programStateBaseAddressIntoCommandStream(StateBaseAddressHelperArgs<GfxFamily> &args, NEO::LinearStream &commandStream) {
    StateBaseAddressHelper<GfxFamily>::programStateBaseAddress(args);

    if (args.heaplessModeEnabled) {
        programHeaplessStateBaseAddress(*args.stateBaseAddressCmd);
    }

    auto cmdSpace = StateBaseAddressHelper<GfxFamily>::getSpaceForSbaCmd(commandStream);
    *cmdSpace = *args.stateBaseAddressCmd;

    if (args.doubleSbaWa) {
        auto cmdSpace = StateBaseAddressHelper<GfxFamily>::getSpaceForSbaCmd(commandStream);
        *cmdSpace = *args.stateBaseAddressCmd;
    }
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programStateBaseAddress(
    StateBaseAddressHelperArgs<GfxFamily> &args) {
    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;

    *args.stateBaseAddressCmd = GfxFamily::cmdInitStateBaseAddress;
    const auto surfaceStateCount = getMaxBindlessSurfaceStates();
    args.stateBaseAddressCmd->setBindlessSurfaceStateSize(surfaceStateCount);

    if (args.sbaProperties) {
        if (args.sbaProperties->dynamicStateBaseAddress.value != StreamProperty64::initValue) {
            args.stateBaseAddressCmd->setDynamicStateBaseAddressModifyEnable(true);
            args.stateBaseAddressCmd->setDynamicStateBufferSizeModifyEnable(true);
            args.stateBaseAddressCmd->setDynamicStateBaseAddress(static_cast<uint64_t>(args.sbaProperties->dynamicStateBaseAddress.value));
            args.stateBaseAddressCmd->setDynamicStateBufferSize(static_cast<uint32_t>(args.sbaProperties->dynamicStateSize.value));
        }
        if (args.sbaProperties->surfaceStateBaseAddress.value != StreamProperty64::initValue) {
            args.stateBaseAddressCmd->setSurfaceStateBaseAddressModifyEnable(true);
            args.stateBaseAddressCmd->setSurfaceStateBaseAddress(static_cast<uint64_t>(args.sbaProperties->surfaceStateBaseAddress.value));
        }

        if (args.sbaProperties->surfaceStateBaseAddress.value != StreamProperty64::initValue) {
            args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddressModifyEnable(true);
            args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddress(static_cast<uint64_t>(args.sbaProperties->surfaceStateBaseAddress.value));
            args.stateBaseAddressCmd->setBindlessSurfaceStateSize(static_cast<uint32_t>(args.sbaProperties->surfaceStateSize.value * (MemoryConstants::pageSize / sizeof(RENDER_SURFACE_STATE))));
        }

        if (args.sbaProperties->statelessMocs.value != StreamProperty::initValue) {
            args.statelessMocsIndex = static_cast<uint32_t>(args.sbaProperties->statelessMocs.value);
        }
    }

    if (args.useGlobalHeapsBaseAddress) {
        args.stateBaseAddressCmd->setDynamicStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setDynamicStateBufferSizeModifyEnable(true);
        args.stateBaseAddressCmd->setDynamicStateBaseAddress(args.globalHeapsBaseAddress);
        args.stateBaseAddressCmd->setDynamicStateBufferSize(MemoryConstants::sizeOf4GBinPageEntities);

        args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setBindlessSurfaceStateBaseAddress(args.globalHeapsBaseAddress);
        args.stateBaseAddressCmd->setBindlessSurfaceStateSize(surfaceStateCount);
    } else if (args.dsh) {
        args.stateBaseAddressCmd->setDynamicStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setDynamicStateBufferSizeModifyEnable(true);
        args.stateBaseAddressCmd->setDynamicStateBaseAddress(args.dsh->getHeapGpuBase());
        args.stateBaseAddressCmd->setDynamicStateBufferSize(args.dsh->getHeapSizeInPages());
    }

    if (args.ssh) {
        args.stateBaseAddressCmd->setSurfaceStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setSurfaceStateBaseAddress(args.ssh->getHeapGpuBase());
    }

    if (args.setInstructionStateBaseAddress) {
        args.stateBaseAddressCmd->setInstructionBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setInstructionBaseAddress(args.instructionHeapBaseAddress);
        args.stateBaseAddressCmd->setInstructionBufferSizeModifyEnable(true);
        args.stateBaseAddressCmd->setInstructionBufferSize(MemoryConstants::sizeOf4GBinPageEntities);

        auto &productHelper = args.gmmHelper->getRootDeviceEnvironment().template getHelper<ProductHelper>();
        auto resourceUsage = CacheSettingsHelper::getGmmUsageType(AllocationType::internalHeap, debugManager.flags.DisableCachingForHeaps.get(), productHelper, args.gmmHelper->getHardwareInfo());

        args.stateBaseAddressCmd->setInstructionMemoryObjectControlState(args.gmmHelper->getMOCS(resourceUsage));
    }

    if (args.setGeneralStateBaseAddress) {
        args.stateBaseAddressCmd->setGeneralStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setGeneralStateBufferSizeModifyEnable(true);
        // GSH must be set to 0 for stateless
        args.stateBaseAddressCmd->setGeneralStateBaseAddress(args.gmmHelper->decanonize(args.generalStateBaseAddress));
        args.stateBaseAddressCmd->setGeneralStateBufferSize(0xfffff);
    }

    if (args.overrideSurfaceStateBaseAddress) {
        args.stateBaseAddressCmd->setSurfaceStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setSurfaceStateBaseAddress(args.surfaceStateBaseAddress);
    }

    if (debugManager.flags.OverrideStatelessMocsIndex.get() != -1) {
        args.statelessMocsIndex = debugManager.flags.OverrideStatelessMocsIndex.get();
    }

    args.statelessMocsIndex = args.statelessMocsIndex << 1;

    GmmHelper::applyMocsEncryptionBit(args.statelessMocsIndex);

    args.stateBaseAddressCmd->setStatelessDataPortAccessMemoryObjectControlState(args.statelessMocsIndex);

    appendStateBaseAddressParameters(args);
}

template <typename GfxFamily>
typename GfxFamily::STATE_BASE_ADDRESS *StateBaseAddressHelper<GfxFamily>::getSpaceForSbaCmd(LinearStream &cmdStream) {
    return cmdStream.getSpaceForCmd<typename GfxFamily::STATE_BASE_ADDRESS>();
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programHeaplessStateBaseAddress(STATE_BASE_ADDRESS &sba) {
}

template <typename GfxFamily>
void StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(LinearStream &commandStream, const IndirectHeap &ssh, GmmHelper *gmmHelper) {
    StateBaseAddressHelper<GfxFamily>::programBindingTableBaseAddress(commandStream, ssh.getHeapGpuBase(), ssh.getHeapSizeInPages(), gmmHelper);
}

template <typename GfxFamily>
inline size_t StateBaseAddressHelper<GfxFamily>::getSbaCmdSize() {
    return sizeof(typename GfxFamily::STATE_BASE_ADDRESS);
}

} // namespace NEO
