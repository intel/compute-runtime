/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/pipe_control_args.h"

namespace NEO {

template <typename Family>
inline void EncodeComputeMode<Family>::programComputeModeCommandWithSynchronization(
    LinearStream &csr, StateComputeModeProperties &properties, const PipelineSelectArgs &args,
    bool hasSharedHandles, const RootDeviceEnvironment &rootDeviceEnvironment, bool isRcs, bool dcFlush) {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    NEO::EncodeWA<Family>::encodeAdditionalPipelineSelect(csr, args, true, rootDeviceEnvironment, isRcs);
    auto *releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, isRcs, releaseHelper);
    std::ignore = isExtendedWARequired;

    if (isBasicWARequired) {
        PipeControlArgs args;
        NEO::EncodeWA<Family>::addPipeControlPriorToNonPipelinedStateCommand(csr, args, rootDeviceEnvironment, isRcs);
    }

    EncodeComputeMode<Family>::programComputeModeCommand(csr, properties, rootDeviceEnvironment);

    if (hasSharedHandles) {
        PipeControlArgs args;
        args.csStallOnly = true;
        MemorySynchronizationCommands<Family>::addSingleBarrier(csr, args);
    }

    NEO::EncodeWA<Family>::encodeAdditionalPipelineSelect(csr, args, false, rootDeviceEnvironment, isRcs);
}

} // namespace NEO
