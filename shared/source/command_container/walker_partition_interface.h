/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>

namespace WalkerPartition {

struct WalkerPartitionArgs {
    uint64_t workPartitionAllocationGpuVa = 0;
    uint64_t postSyncGpuAddress = 0;
    uint64_t postSyncImmediateValue = 0;
    uint32_t partitionCount = 0;
    uint32_t tileCount = 0;
    bool emitBatchBufferEnd = false;
    bool secondaryBatchBuffer = false;
    bool synchronizeBeforeExecution = false;
    bool crossTileAtomicSynchronization = false;
    bool semaphoreProgrammingRequired = false;
    bool staticPartitioning = false;
    bool emitSelfCleanup = false;
    bool useAtomicsForSelfCleanup = false;
    bool initializeWparidRegister = false;
    bool emitPipeControlStall = false;
    bool preferredStaticPartitioning = false;
    bool usePostSync = false;
};

constexpr uint32_t wparidCCSOffset = 0x221C;
constexpr uint32_t addressOffsetCCSOffset = 0x23B4;
constexpr uint32_t predicationMaskCCSOffset = 0x21FC;

constexpr uint32_t generalPurposeRegister0 = 0x2600;
constexpr uint32_t generalPurposeRegister1 = 0x2608;
constexpr uint32_t generalPurposeRegister2 = 0x2610;
constexpr uint32_t generalPurposeRegister3 = 0x2618;
constexpr uint32_t generalPurposeRegister4 = 0x2620;
constexpr uint32_t generalPurposeRegister5 = 0x2628;
constexpr uint32_t generalPurposeRegister6 = 0x2630;

struct BatchBufferControlData {
    uint32_t partitionCount = 0u;
    uint32_t tileCount = 0u;
    uint32_t inTileCount = 0u;
    uint32_t finalSyncTileCount = 0u;
};
constexpr size_t dynamicPartitioningFieldsForCleanupCount = sizeof(BatchBufferControlData) / sizeof(uint32_t) - 1;

struct StaticPartitioningControlSection {
    uint32_t synchronizeBeforeWalkerCounter = 0;
    uint32_t synchronizeAfterWalkerCounter = 0;
    uint32_t finalSyncTileCounter = 0;
};
constexpr size_t staticPartitioningFieldsForCleanupCount = sizeof(StaticPartitioningControlSection) / sizeof(uint32_t) - 1;

struct BarrierControlSection {
    uint32_t crossTileSyncCount = 0u;
    uint32_t finalSyncTileCount = 0;
};
constexpr size_t barrierControlSectionFieldsForCleanupCount = sizeof(BarrierControlSection) / sizeof(uint32_t) - 1;
} // namespace WalkerPartition
