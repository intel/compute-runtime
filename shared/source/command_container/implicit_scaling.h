/*
 * Copyright (C) 2021-2022 Intel Corporation
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
struct HardwareInfo;
class LinearStream;
struct PipeControlArgs;

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
    static bool pipeControlBeforeCleanupAtomicSyncRequired();
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
                                 bool dcFlush,
                                 uint64_t workPartitionAllocationGpuVa,
                                 const HardwareInfo &hwInfo);

    static bool &getPipeControlStallRequired();

    static size_t getBarrierSize(const HardwareInfo &hwInfo,
                                 bool apiSelfCleanup,
                                 bool usePostSync);
    static void dispatchBarrierCommands(LinearStream &commandStream,
                                        const DeviceBitfield &devices,
                                        PipeControlArgs &flushArgs,
                                        const HardwareInfo &hwInfo,
                                        uint64_t gpuAddress,
                                        uint64_t immediateData,
                                        bool apiSelfCleanup,
                                        bool useSecondaryBatchBuffer);

    static size_t getRegisterConfigurationSize();
    static void dispatchRegisterConfiguration(LinearStream &commandStream,
                                              uint64_t workPartitionSurfaceAddress,
                                              uint32_t addressOffset);

    static size_t getOffsetRegisterSize();
    static void dispatchOffsetRegister(LinearStream &commandStream,
                                       uint32_t addressOffset);

    static uint32_t getPostSyncOffset();

    static bool platformSupportsImplicitScaling(const HardwareInfo &hwInfo);

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
