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
extern bool semaphoreProgrammingRequired;
extern bool crossTileAtomicSynchronization;

constexpr uint32_t partitionAddressOffsetDwords = 2u;
constexpr uint32_t partitionAddressOffset = sizeof(uint32_t) * partitionAddressOffsetDwords;
} // namespace ImplicitScaling

struct ImplicitScalingHelper {
    static bool isImplicitScalingEnabled(const DeviceBitfield &devices, bool preCondition);
    static bool isSemaphoreProgrammingRequired();
    static bool isCrossTileAtomicRequired();
    static bool isSynchronizeBeforeExecutionRequired();
    static bool isAtomicsUsedForSelfCleanup();
    static bool isSelfCleanupRequired(bool defaultSelfCleanup);
    static bool isWparidRegisterInitializationRequired();
    static bool isPipeControlStallRequired();
};

template <typename GfxFamily>
struct ImplicitScalingDispatch {
    using WALKER_TYPE = typename GfxFamily::WALKER_TYPE;

    static size_t getSize(bool emitSelfCleanup,
                          bool preferStaticPartitioning,
                          const DeviceBitfield &devices,
                          const Vec3<size_t> &groupStart,
                          const Vec3<size_t> &groupCount);
    static void dispatchCommands(LinearStream &commandStream,
                                 WALKER_TYPE &walkerCmd,
                                 const DeviceBitfield &devices,
                                 uint32_t &partitionCount,
                                 bool useSecondaryBatchBuffer,
                                 bool emitSelfCleanup,
                                 bool usesImages,
                                 uint64_t workPartitionAllocationGpuVa);
};

template <typename GfxFamily>
struct PartitionRegisters {
    enum {
        wparidCCSOffset = 0x221C,
        addressOffsetCCSOffset = 0x23B4
    };
};

} // namespace NEO
