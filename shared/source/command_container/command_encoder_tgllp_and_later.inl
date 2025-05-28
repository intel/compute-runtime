/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"

namespace NEO {

template <typename Family>
size_t EncodeComputeMode<Family>::getCmdSizeForComputeMode(const RootDeviceEnvironment &rootDeviceEnvironment, bool hasSharedHandles, bool isRcs) {
    size_t size = 0;
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto *releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);
    std::ignore = isExtendedWARequired;

    if (isBasicWARequired) {
        size += MemorySynchronizationCommands<Family>::getSizeForSingleBarrier();
    }
    size += sizeof(typename Family::STATE_COMPUTE_MODE);
    if (hasSharedHandles) {
        size += MemorySynchronizationCommands<Family>::getSizeForStallingBarrier();
    }
    if (productHelper.is3DPipelineSelectWARequired() && isRcs) {
        size += (2 * PreambleHelper<Family>::getCmdSizeForPipelineSelect(rootDeviceEnvironment));
    }
    return size;
}

template <typename GfxFamily>
inline size_t EncodeComputeMode<GfxFamily>::getSizeForComputeMode() {
    return sizeof(typename GfxFamily::STATE_COMPUTE_MODE);
}

template <typename Family>
void EncodeSetMMIO<Family>::remapOffset(MI_LOAD_REGISTER_MEM *pMiLoadReg) {
    if (isRemapApplicable(pMiLoadReg->getRegisterAddress())) {
        pMiLoadReg->setMmioRemapEnable(true);
    }
}

template <typename Family>
void EncodeSetMMIO<Family>::remapOffset(MI_LOAD_REGISTER_REG *pMiLoadReg) {
    if (isRemapApplicable(pMiLoadReg->getSourceRegisterAddress())) {
        pMiLoadReg->setMmioRemapEnableSource(true);
    }
    if (isRemapApplicable(pMiLoadReg->getDestinationRegisterAddress())) {
        pMiLoadReg->setMmioRemapEnableDestination(true);
    }
}

template <typename Family>
inline bool EncodeSetMMIO<Family>::isRemapApplicable(uint32_t offset) {
    return (0x2000 <= offset && offset <= 0x27ff) ||
           (0x4200 <= offset && offset <= 0x420f) ||
           (0x4400 <= offset && offset <= 0x441f);
}

template <typename Family>
void EncodeWA<Family>::addPipeControlBeforeStateBaseAddress(LinearStream &commandStream,
                                                            const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs, bool dcFlushRequired) {
    PipeControlArgs args;
    args.dcFlushEnable = dcFlushRequired;
    args.textureCacheInvalidationEnable = true;
    args.hdcPipelineFlush = true;

    NEO::EncodeWA<Family>::addPipeControlPriorToNonPipelinedStateCommand(commandStream, args, rootDeviceEnvironment, isRcs);
}

template <typename Family>
inline void EncodeMiArbCheck<Family>::adjust(MI_ARB_CHECK &miArbCheck, std::optional<bool> preParserDisable) {
    if (debugManager.flags.ForcePreParserEnabledForMiArbCheck.get() != -1) {
        preParserDisable = !debugManager.flags.ForcePreParserEnabledForMiArbCheck.get();
    }
    if (preParserDisable.has_value()) {
        miArbCheck.setPreParserDisable(preParserDisable.value());
    }
}

template <typename Family>
inline void EncodeStoreMemory<Family>::encodeForceCompletionCheck(MI_STORE_DATA_IMM &storeDataImmCmd) {
    storeDataImmCmd.setForceWriteCompletionCheck(true);
}

} // namespace NEO
