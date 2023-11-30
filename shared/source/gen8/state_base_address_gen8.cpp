/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds_base.h"
#include "shared/source/helpers/state_base_address.h"
#include "shared/source/helpers/state_base_address_bdw.inl"
#include "shared/source/helpers/state_base_address_bdw_and_later.inl"

namespace NEO {
using Family = Gen8Family;

template <>
void StateBaseAddressHelper<Family>::programStateBaseAddress(
    StateBaseAddressHelperArgs<Family> &args) {

    *args.stateBaseAddressCmd = Family::cmdInitStateBaseAddress;

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
        if (args.sbaProperties->statelessMocs.value != StreamProperty::initValue) {
            args.statelessMocsIndex = static_cast<uint32_t>(args.sbaProperties->statelessMocs.value);
        }
    }

    if (args.dsh) {
        args.stateBaseAddressCmd->setDynamicStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setDynamicStateBufferSizeModifyEnable(true);
        args.stateBaseAddressCmd->setDynamicStateBaseAddress(args.dsh->getHeapGpuBase());
        args.stateBaseAddressCmd->setDynamicStateBufferSize(args.dsh->getHeapSizeInPages());
    }

    StateBaseAddressHelper<Family>::appendIohParameters(args);

    if (args.ssh) {
        args.stateBaseAddressCmd->setSurfaceStateBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setSurfaceStateBaseAddress(args.ssh->getHeapGpuBase());
    }

    if (args.setInstructionStateBaseAddress) {
        args.stateBaseAddressCmd->setInstructionBaseAddressModifyEnable(true);
        args.stateBaseAddressCmd->setInstructionBaseAddress(args.instructionHeapBaseAddress);
        args.stateBaseAddressCmd->setInstructionBufferSizeModifyEnable(true);
        args.stateBaseAddressCmd->setInstructionBufferSize(MemoryConstants::sizeOf4GBinPageEntities);
        args.stateBaseAddressCmd->setInstructionMemoryObjectControlState(args.gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER));
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
template struct StateBaseAddressHelper<Family>;
} // namespace NEO
