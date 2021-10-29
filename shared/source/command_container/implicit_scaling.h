/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/vec.h"

namespace WalkerPartition {
struct WalkerPartitionArgs;
}

namespace NEO {
class LinearStream;

namespace ImplicitScaling {
extern bool apiSupport;
} // namespace ImplicitScaling

struct ImplicitScalingHelper {
    static bool isImplicitScalingEnabled(const DeviceBitfield &devices, bool preCondition);
    static bool isSemaphoreProgrammingRequired();
    static bool isCrossTileAtomicRequired(bool defaultCrossTileRequirement);
    static bool isSynchronizeBeforeExecutionRequired();
    static bool isAtomicsUsedForSelfCleanup();
    static bool isSelfCleanupRequired(const WalkerPartition::WalkerPartitionArgs &args, bool apiSelfCleanup);
    static bool isWparidRegisterInitializationRequired();
    static bool isPipeControlStallRequired(bool defaultEmitPipeControl);
};

template <typename GfxFamily>
struct ImplicitScalingDispatch {
    using WALKER_TYPE = typename GfxFamily::WALKER_TYPE;

    static size_t getSize(bool apiSelfCleanup,
                          bool preferStaticPartitioning,
                          const DeviceBitfield &devices,
                          const Vec3<size_t> &groupStart,
                          const Vec3<size_t> &groupCount);

    static void dispatchCommands(LinearStream &commandStream,
                                 WALKER_TYPE &walkerCmd,
                                 const DeviceBitfield &devices,
                                 uint32_t &partitionCount,
                                 bool useSecondaryBatchBuffer,
                                 bool apiSelfCleanup,
                                 bool usesImages,
                                 uint64_t workPartitionAllocationGpuVa);

    static bool &getPipeControlStallRequired();

  private:
    static bool pipeControlStallRequired;
};

template <typename GfxFamily>
struct PartitionRegisters {
    enum {
        wparidCCSOffset = 0x221C,
        addressOffsetCCSOffset = 0x23B4
    };
};

} // namespace NEO
