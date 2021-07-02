/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/vec.h"

namespace NEO {
class LinearStream;

namespace ImplicitScaling {
extern bool apiSupport;
}

struct ImplicitScalingHelper {
    static bool isImplicitScalingEnabled(const DeviceBitfield &devices, bool preCondition);
    static bool isSynchronizeBeforeExecutionRequired();
    static bool useAtomicsForNativeCleanup();
};

template <typename GfxFamily>
struct ImplicitScalingDispatch {
    using WALKER_TYPE = typename GfxFamily::WALKER_TYPE;

    static size_t getSize(bool nativeCrossTileAtomicSync,
                          bool preferStaticPartitioning,
                          const DeviceBitfield &devices,
                          Vec3<size_t> groupStart,
                          Vec3<size_t> groupCount);
    static void dispatchCommands(LinearStream &commandStream,
                                 WALKER_TYPE &walkerCmd,
                                 const DeviceBitfield &devices,
                                 uint32_t &partitionCount,
                                 bool useSecondaryBatchBuffer,
                                 bool nativeCrossTileAtomicSync,
                                 bool usesImages,
                                 uint64_t workPartitionAllocationGpuVa);
};

} // namespace NEO
