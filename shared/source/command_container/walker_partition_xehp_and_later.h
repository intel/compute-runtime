/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"

#include <cassert>
#include <optional>

namespace WalkerPartition {

template <typename GfxFamily>
using COMPUTE_WALKER = typename GfxFamily::COMPUTE_WALKER;
template <typename GfxFamily>
using POSTSYNC_DATA = typename GfxFamily::POSTSYNC_DATA;
template <typename GfxFamily>
using BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
template <typename GfxFamily>
using BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;
template <typename GfxFamily>
using LOAD_REGISTER_IMM = typename GfxFamily::MI_LOAD_REGISTER_IMM;
template <typename GfxFamily>
using LOAD_REGISTER_MEM = typename GfxFamily::MI_LOAD_REGISTER_MEM;
template <typename GfxFamily>
using MI_SET_PREDICATE = typename GfxFamily::MI_SET_PREDICATE;
template <typename GfxFamily>
using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
template <typename GfxFamily>
using MI_ATOMIC = typename GfxFamily::MI_ATOMIC;
template <typename GfxFamily>
using DATA_SIZE = typename GfxFamily::MI_ATOMIC::DATA_SIZE;
template <typename GfxFamily>
using LOAD_REGISTER_REG = typename GfxFamily::MI_LOAD_REGISTER_REG;
template <typename GfxFamily>
using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;
template <typename GfxFamily>
using MI_STORE_DATA_IMM = typename GfxFamily::MI_STORE_DATA_IMM;

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
static constexpr inline size_t dynamicPartitioningFieldsForCleanupCount = sizeof(BatchBufferControlData) / sizeof(uint32_t) - 1;

template <typename Command>
Command *putCommand(void *&inputAddress, uint32_t &totalBytesProgrammed) {
    totalBytesProgrammed += sizeof(Command);
    auto commandToReturn = reinterpret_cast<Command *>(inputAddress);
    inputAddress = ptrOffset(inputAddress, sizeof(Command));
    return commandToReturn;
}

bool inline isSemaphoreProgrammingRequired() {
    auto semaphoreProgrammingRequired = false;
    if (NEO::DebugManager.flags.ExperimentalSynchronizeWithSemaphores.get() == 1) {
        semaphoreProgrammingRequired = true;
    }
    return semaphoreProgrammingRequired;
}

bool inline isCrossTileAtomicRequired() {
    auto crossTileAtomicSynchronization = true;
    if (NEO::DebugManager.flags.ExperimentalForceCrossAtomicSynchronization.get() == 0) {
        crossTileAtomicSynchronization = false;
    }
    return crossTileAtomicSynchronization;
}

template <typename GfxFamily>
uint32_t computePartitionCountAndPartitionType(uint32_t preferredMinimalPartitionCount,
                                               bool preferStaticPartitioning,
                                               Vec3<size_t> groupStart,
                                               Vec3<size_t> groupCount,
                                               std::optional<typename COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE> requestedPartitionType,
                                               typename COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE *outSelectedPartitionType,
                                               bool *outSelectStaticPartitioning) {
    // For non uniform starting point, there is no support for partition in Hardware. Disable partitioning and select dynamic algorithm
    if (groupStart.x || groupStart.y || groupStart.z) {
        *outSelectedPartitionType = COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_DISABLED;
        *outSelectStaticPartitioning = false;
        return 1u;
    }

    size_t workgroupCount = 0u;
    bool disablePartitionForPartitionCountOne{};

    if (NEO::DebugManager.flags.ExperimentalSetWalkerPartitionType.get() != -1) {
        requestedPartitionType = static_cast<typename COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE>(NEO::DebugManager.flags.ExperimentalSetWalkerPartitionType.get());
    }

    if (requestedPartitionType.has_value()) {
        switch (requestedPartitionType.value()) {
        case COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_X:
            workgroupCount = groupCount.x;
            break;
        case COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_Y:
            workgroupCount = groupCount.y;
            break;
        case COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_Z:
            workgroupCount = groupCount.z;
            break;
        default:
            UNRECOVERABLE_IF(true);
        }
        *outSelectedPartitionType = requestedPartitionType.value();
        disablePartitionForPartitionCountOne = false;
    } else {
        const size_t maxDimension = std::max({groupCount.z, groupCount.y, groupCount.x});

        auto goWithMaxAlgorithm = !preferStaticPartitioning;
        if (NEO::DebugManager.flags.WalkerPartitionPreferHighestDimension.get() != -1) {
            goWithMaxAlgorithm = !!!NEO::DebugManager.flags.WalkerPartitionPreferHighestDimension.get();
        }

        //compute misaligned %, accept imbalance below threshold in favor of Z/Y/X distribution.
        const float minimalThreshold = 0.05f;
        float zImbalance = static_cast<float>(groupCount.z - alignDown(groupCount.z, preferredMinimalPartitionCount)) / static_cast<float>(groupCount.z);
        float yImbalance = static_cast<float>(groupCount.y - alignDown(groupCount.y, preferredMinimalPartitionCount)) / static_cast<float>(groupCount.y);

        //we first try with deepest dimension to see if we can partition there
        if (groupCount.z > 1 && (zImbalance <= minimalThreshold)) {
            *outSelectedPartitionType = COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_Z;
        } else if (groupCount.y > 1 && (yImbalance < minimalThreshold)) {
            *outSelectedPartitionType = COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_Y;
        } else if (groupCount.x % preferredMinimalPartitionCount == 0) {
            *outSelectedPartitionType = COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_X;
        }
        //if we are here then there is no dimension that results in even distribution, choose max dimension to minimize impact
        else {
            goWithMaxAlgorithm = true;
        }

        if (goWithMaxAlgorithm) {
            // default mode, select greatest dimension
            if (maxDimension == groupCount.x) {
                *outSelectedPartitionType = COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_X;
            } else if (maxDimension == groupCount.y) {
                *outSelectedPartitionType = COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_Y;
            } else {
                *outSelectedPartitionType = COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_Z;
            }
        }

        workgroupCount = maxDimension;
        disablePartitionForPartitionCountOne = true;
    }

    // Static partitioning - partition count == tile count
    *outSelectStaticPartitioning = preferStaticPartitioning;
    if (preferStaticPartitioning) {
        return preferredMinimalPartitionCount;
    }

    // Dynamic partitioning - compute optimal partition count
    size_t partitionCount = std::min(static_cast<size_t>(16u), workgroupCount);
    partitionCount = Math::prevPowerOfTwo(partitionCount);
    if (NEO::DebugManager.flags.SetMinimalPartitionSize.get() != 0) {
        const auto workgroupPerPartitionThreshold = NEO::DebugManager.flags.SetMinimalPartitionSize.get() == -1
                                                        ? 512u
                                                        : static_cast<unsigned>(NEO::DebugManager.flags.SetMinimalPartitionSize.get());
        preferredMinimalPartitionCount = std::max(2u, preferredMinimalPartitionCount);

        while (partitionCount > preferredMinimalPartitionCount) {
            auto workgroupsPerPartition = workgroupCount / partitionCount;
            if (workgroupsPerPartition >= workgroupPerPartitionThreshold) {
                break;
            }
            partitionCount = partitionCount / 2;
        }
    }

    if (partitionCount == 1u && disablePartitionForPartitionCountOne) {
        *outSelectedPartitionType = COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_DISABLED;
    }

    return static_cast<uint32_t>(partitionCount);
}

template <typename GfxFamily>
uint32_t computePartitionCountAndSetPartitionType(COMPUTE_WALKER<GfxFamily> *walker,
                                                  uint32_t preferredMinimalPartitionCount,
                                                  bool preferStaticPartitioning,
                                                  bool usesImages,
                                                  bool *outSelectStaticPartitioning) {
    const Vec3<size_t> groupStart = {walker->getThreadGroupIdStartingX(), walker->getThreadGroupIdStartingY(), walker->getThreadGroupIdStartingZ()};
    const Vec3<size_t> groupCount = {walker->getThreadGroupIdXDimension(), walker->getThreadGroupIdYDimension(), walker->getThreadGroupIdZDimension()};
    std::optional<typename COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE> requestedPartitionType{};
    if (usesImages) {
        requestedPartitionType = COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_X;
    }
    typename COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE partitionType{};
    const auto partitionCount = computePartitionCountAndPartitionType<GfxFamily>(preferredMinimalPartitionCount,
                                                                                 preferStaticPartitioning,
                                                                                 groupStart,
                                                                                 groupCount,
                                                                                 requestedPartitionType,
                                                                                 &partitionType,
                                                                                 outSelectStaticPartitioning);
    walker->setPartitionType(partitionType);
    return partitionCount;
}

template <typename GfxFamily>
void programRegisterWithValue(void *&inputAddress, uint32_t registerOffset, uint32_t &totalBytesProgrammed, uint32_t registerValue) {
    auto loadRegisterImmediate = putCommand<LOAD_REGISTER_IMM<GfxFamily>>(inputAddress, totalBytesProgrammed);
    LOAD_REGISTER_IMM<GfxFamily> cmd = GfxFamily::cmdInitLoadRegisterImm;

    cmd.setRegisterOffset(registerOffset);
    cmd.setDataDword(registerValue);
    cmd.setMmioRemapEnable(true);
    *loadRegisterImmediate = cmd;
}

template <typename GfxFamily>
void programWaitForSemaphore(void *&inputAddress, uint32_t &totalBytesProgrammed, uint64_t gpuAddress, uint32_t semaphoreCompareValue, typename MI_SEMAPHORE_WAIT<GfxFamily>::COMPARE_OPERATION compareOperation) {
    auto semaphoreWait = putCommand<MI_SEMAPHORE_WAIT<GfxFamily>>(inputAddress, totalBytesProgrammed);
    MI_SEMAPHORE_WAIT<GfxFamily> cmd = GfxFamily::cmdInitMiSemaphoreWait;

    cmd.setSemaphoreDataDword(semaphoreCompareValue);
    cmd.setSemaphoreGraphicsAddress(gpuAddress);
    cmd.setWaitMode(MI_SEMAPHORE_WAIT<GfxFamily>::WAIT_MODE::WAIT_MODE_POLLING_MODE);
    cmd.setCompareOperation(compareOperation);
    *semaphoreWait = cmd;
}

template <typename GfxFamily>
bool programWparidMask(void *&inputAddress, uint32_t &totalBytesProgrammed, uint32_t partitionCount) {
    //currently only power of 2 values of partitionCount are being supported
    if (!Math::isPow2(partitionCount) || partitionCount > 16) {
        return false;
    }

    auto mask = 0xFFE0;
    auto fillValue = 0x10;
    auto count = partitionCount;
    while (count < 16) {
        fillValue |= (fillValue >> 1);
        count *= 2;
    }
    mask |= (mask | fillValue);

    programRegisterWithValue<GfxFamily>(inputAddress, predicationMaskCCSOffset, totalBytesProgrammed, mask);
    return true;
}

template <typename GfxFamily>
void programWparidPredication(void *&inputAddress, uint32_t &totalBytesProgrammed, bool predicationEnabled) {
    auto miSetPredicate = putCommand<MI_SET_PREDICATE<GfxFamily>>(inputAddress, totalBytesProgrammed);
    MI_SET_PREDICATE<GfxFamily> cmd = GfxFamily::cmdInitSetPredicate;

    if (predicationEnabled) {
        cmd.setPredicateEnableWparid(MI_SET_PREDICATE<GfxFamily>::PREDICATE_ENABLE_WPARID::PREDICATE_ENABLE_WPARID_NOOP_ON_NON_ZERO_VALUE);
    } else {
        cmd.setPredicateEnable(MI_SET_PREDICATE<GfxFamily>::PREDICATE_ENABLE::PREDICATE_ENABLE_PREDICATE_DISABLE);
    }
    *miSetPredicate = cmd;
}

template <typename GfxFamily>
void programMiAtomic(void *&inputAddress, uint32_t &totalBytesProgrammed, uint64_t gpuAddress, bool requireReturnValue, typename MI_ATOMIC<GfxFamily>::ATOMIC_OPCODES atomicOpcode) {
    auto miAtomic = putCommand<MI_ATOMIC<GfxFamily>>(inputAddress, totalBytesProgrammed);
    NEO::EncodeAtomic<GfxFamily>::programMiAtomic(miAtomic, gpuAddress, atomicOpcode, DATA_SIZE<GfxFamily>::DATA_SIZE_DWORD,
                                                  requireReturnValue, requireReturnValue, 0x0u, 0x0u);
}

template <typename GfxFamily>
void programMiBatchBufferStart(void *&inputAddress, uint32_t &totalBytesProgrammed,
                               uint64_t gpuAddress, bool predicationEnabled, bool secondary) {
    auto batchBufferStart = putCommand<BATCH_BUFFER_START<GfxFamily>>(inputAddress, totalBytesProgrammed);
    BATCH_BUFFER_START<GfxFamily> cmd = GfxFamily::cmdInitBatchBufferStart;

    cmd.setSecondLevelBatchBuffer(static_cast<typename BATCH_BUFFER_START<GfxFamily>::SECOND_LEVEL_BATCH_BUFFER>(secondary));
    cmd.setAddressSpaceIndicator(BATCH_BUFFER_START<GfxFamily>::ADDRESS_SPACE_INDICATOR::ADDRESS_SPACE_INDICATOR_PPGTT);
    cmd.setPredicationEnable(predicationEnabled);
    cmd.setBatchBufferStartAddress(gpuAddress);
    *batchBufferStart = cmd;
}

template <typename GfxFamily>
void programMiLoadRegisterReg(void *&inputAddress, uint32_t &totalBytesProgrammed, uint32_t sourceRegisterOffset, uint32_t destinationRegisterOffset) {
    auto loadRegisterReg = putCommand<LOAD_REGISTER_REG<GfxFamily>>(inputAddress, totalBytesProgrammed);
    LOAD_REGISTER_REG<GfxFamily> cmd = GfxFamily::cmdInitLoadRegisterReg;

    cmd.setMmioRemapEnableSource(true);
    cmd.setMmioRemapEnableDestination(true);
    cmd.setSourceRegisterAddress(sourceRegisterOffset);
    cmd.setDestinationRegisterAddress(destinationRegisterOffset);
    *loadRegisterReg = cmd;
}

template <typename GfxFamily>
void programMiLoadRegisterMem(void *&inputAddress, uint32_t &totalBytesProgrammed, uint64_t gpuAddressToLoad, uint32_t destinationRegisterOffset) {
    auto loadRegisterReg = putCommand<LOAD_REGISTER_MEM<GfxFamily>>(inputAddress, totalBytesProgrammed);
    LOAD_REGISTER_MEM<GfxFamily> cmd = GfxFamily::cmdInitLoadRegisterMem;

    cmd.setMmioRemapEnable(true);
    cmd.setMemoryAddress(gpuAddressToLoad);
    cmd.setRegisterAddress(destinationRegisterOffset);
    *loadRegisterReg = cmd;
}

template <typename GfxFamily>
void programPipeControlCommand(void *&inputAddress, uint32_t &totalBytesProgrammed, bool dcFlush) {
    auto pipeControl = putCommand<PIPE_CONTROL<GfxFamily>>(inputAddress, totalBytesProgrammed);
    PIPE_CONTROL<GfxFamily> cmd = GfxFamily::cmdInitPipeControl;

    if (NEO::MemorySynchronizationCommands<GfxFamily>::isDcFlushAllowed()) {
        cmd.setDcFlushEnable(dcFlush);
    }
    if (NEO::DebugManager.flags.DoNotFlushCaches.get()) {
        cmd.setDcFlushEnable(false);
    }
    *pipeControl = cmd;
}

template <typename GfxFamily>
void programStoreMemImmediateDword(void *&inputAddress, uint32_t &totalBytesProgrammed, uint64_t gpuAddress, uint32_t data) {
    auto storeDataImmediate = putCommand<MI_STORE_DATA_IMM<GfxFamily>>(inputAddress, totalBytesProgrammed);
    MI_STORE_DATA_IMM<GfxFamily> cmd = GfxFamily::cmdInitStoreDataImm;

    cmd.setAddress(gpuAddress);
    cmd.setStoreQword(false);
    cmd.setDwordLength(MI_STORE_DATA_IMM<GfxFamily>::DWORD_LENGTH::DWORD_LENGTH_STORE_DWORD);
    cmd.setDataDword0(static_cast<uint32_t>(data));

    *storeDataImmediate = cmd;
}

template <typename GfxFamily>
void programNativeCrossTileSyncControl(void *&inputAddress,
                                       uint32_t &totalBytesProgrammed,
                                       uint64_t finalSyncTileCountField,
                                       bool useAtomicsForNativeCleanup) {
    if (useAtomicsForNativeCleanup) {
        programMiAtomic<GfxFamily>(inputAddress,
                                   totalBytesProgrammed,
                                   finalSyncTileCountField,
                                   false,
                                   MI_ATOMIC<GfxFamily>::ATOMIC_OPCODES::ATOMIC_4B_MOVE);
    } else {
        programStoreMemImmediateDword<GfxFamily>(inputAddress,
                                                 totalBytesProgrammed,
                                                 finalSyncTileCountField,
                                                 0u);
    }
}

template <typename GfxFamily>
void programNativeCrossTileSyncCleanup(void *&inputAddress,
                                       uint32_t &totalBytesProgrammed,
                                       uint64_t finalSyncTileCountAddress,
                                       uint64_t baseAddressForCleanup,
                                       size_t fieldsForCleanupCount,
                                       uint32_t tileCount,
                                       bool useAtomicsForNativeCleanup) {
    // Synchronize tiles, so the fields are not cleared while still in use
    programMiAtomic<GfxFamily>(inputAddress, totalBytesProgrammed, finalSyncTileCountAddress, false, MI_ATOMIC<GfxFamily>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT);
    programWaitForSemaphore<GfxFamily>(inputAddress, totalBytesProgrammed, finalSyncTileCountAddress, tileCount, MI_SEMAPHORE_WAIT<GfxFamily>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);

    for (auto fieldIndex = 0u; fieldIndex < fieldsForCleanupCount; fieldIndex++) {
        const uint64_t addressForCleanup = baseAddressForCleanup + fieldIndex * sizeof(uint32_t);
        if (useAtomicsForNativeCleanup) {
            programMiAtomic<GfxFamily>(inputAddress,
                                       totalBytesProgrammed,
                                       addressForCleanup,
                                       false,
                                       MI_ATOMIC<GfxFamily>::ATOMIC_OPCODES::ATOMIC_4B_MOVE);
        } else {
            programStoreMemImmediateDword<GfxFamily>(inputAddress,
                                                     totalBytesProgrammed,
                                                     addressForCleanup,
                                                     0u);
        }
    }

    //this synchronization point ensures that all tiles finished zeroing and will fairly access control section atomic variables
    programMiAtomic<GfxFamily>(inputAddress, totalBytesProgrammed, finalSyncTileCountAddress, false, MI_ATOMIC<GfxFamily>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT);
    programWaitForSemaphore<GfxFamily>(inputAddress, totalBytesProgrammed, finalSyncTileCountAddress, 2 * tileCount, MI_SEMAPHORE_WAIT<GfxFamily>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
}

template <typename GfxFamily>
void programTilesSynchronizationWithPostSyncs(void *&currentBatchBufferPointer,
                                              uint32_t &totalBytesProgrammed,
                                              COMPUTE_WALKER<GfxFamily> *inputWalker,
                                              uint32_t partitionCount) {
    const auto postSyncAddress = inputWalker->getPostSync().getDestinationAddress() + 8llu;
    for (uint32_t partitionId = 0u; partitionId < partitionCount; partitionId++) {
        programWaitForSemaphore<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, postSyncAddress + partitionId * 16llu, 1u, MI_SEMAPHORE_WAIT<GfxFamily>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
    }
}

template <typename GfxFamily>
void programTilesSynchronizationWithAtomics(void *&currentBatchBufferPointer,
                                            uint32_t &totalBytesProgrammed,
                                            uint64_t atomicAddress,
                                            uint32_t tileCount) {
    programMiAtomic<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, atomicAddress, false, MI_ATOMIC<GfxFamily>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT);
    programWaitForSemaphore<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, atomicAddress, tileCount, MI_SEMAPHORE_WAIT<GfxFamily>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
}

template <typename GfxFamily>
uint64_t computeWalkerSectionSize() {
    return sizeof(BATCH_BUFFER_START<GfxFamily>) +
           sizeof(COMPUTE_WALKER<GfxFamily>);
}

template <typename GfxFamily>
uint64_t computeNativeCrossTileSyncControlSectionSize(bool useAtomicsForNativeCleanup) {
    if (useAtomicsForNativeCleanup) {
        return sizeof(MI_ATOMIC<GfxFamily>);
    } else {
        return sizeof(MI_STORE_DATA_IMM<GfxFamily>);
    }
}

template <typename GfxFamily>
uint64_t computeNativeCrossTileSyncCleanupSectionSize(size_t fieldsForCleanupCount, bool useAtomicsForNativeCleanup) {
    return fieldsForCleanupCount * computeNativeCrossTileSyncControlSectionSize<GfxFamily>(useAtomicsForNativeCleanup) +
           2 * sizeof(MI_ATOMIC<GfxFamily>) +
           2 * sizeof(MI_SEMAPHORE_WAIT<GfxFamily>);
}

template <typename GfxFamily>
uint64_t computeControlSectionOffset(uint32_t partitionCount, bool synchronizeBeforeExecution, bool nativeCrossTileAtomicSync, bool useAtomicsForNativeCleanup) {
    auto synchronizationCount = (synchronizeBeforeExecution) ? 2u : 1u;
    if (!isCrossTileAtomicRequired() && !nativeCrossTileAtomicSync) {
        synchronizationCount--;
    }

    return sizeof(LOAD_REGISTER_IMM<GfxFamily>) +
           sizeof(MI_ATOMIC<GfxFamily>) * (1u + synchronizationCount) +
           sizeof(LOAD_REGISTER_REG<GfxFamily>) +
           sizeof(MI_SET_PREDICATE<GfxFamily>) * 2 +
           sizeof(BATCH_BUFFER_START<GfxFamily>) * 2 +
           sizeof(PIPE_CONTROL<GfxFamily>) +
           sizeof(MI_SEMAPHORE_WAIT<GfxFamily>) * synchronizationCount +
           (isSemaphoreProgrammingRequired() ? sizeof(MI_SEMAPHORE_WAIT<GfxFamily>) * partitionCount : 0u) +
           computeWalkerSectionSize<GfxFamily>() +
           (nativeCrossTileAtomicSync ? computeNativeCrossTileSyncControlSectionSize<GfxFamily>(useAtomicsForNativeCleanup) : 0u);
}

template <typename GfxFamily>
uint64_t computeWalkerSectionStart(uint32_t partitionCount,
                                   bool synchronizeBeforeExecution,
                                   bool nativeCrossTileAtomicSync,
                                   bool useAtomicsForNativeCleanup) {
    return computeControlSectionOffset<GfxFamily>(partitionCount, synchronizeBeforeExecution, nativeCrossTileAtomicSync, useAtomicsForNativeCleanup) -
           computeWalkerSectionSize<GfxFamily>();
}

template <typename GfxFamily>
void programPartitionedWalker(void *&inputAddress, uint32_t &totalBytesProgrammed,
                              COMPUTE_WALKER<GfxFamily> *inputWalker,
                              uint32_t partitionCount) {
    auto computeWalker = putCommand<COMPUTE_WALKER<GfxFamily>>(inputAddress, totalBytesProgrammed);
    COMPUTE_WALKER<GfxFamily> cmd = *inputWalker;

    if (partitionCount > 1) {
        auto partitionType = inputWalker->getPartitionType();

        assert(inputWalker->getThreadGroupIdStartingX() == 0u);
        assert(inputWalker->getThreadGroupIdStartingY() == 0u);
        assert(inputWalker->getThreadGroupIdStartingZ() == 0u);
        assert(partitionType != COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_DISABLED);

        cmd.setWorkloadPartitionEnable(true);

        auto workgroupCount = 0u;
        if (partitionType == COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_X) {
            workgroupCount = inputWalker->getThreadGroupIdXDimension();
        } else if (partitionType == COMPUTE_WALKER<GfxFamily>::PARTITION_TYPE::PARTITION_TYPE_Y) {
            workgroupCount = inputWalker->getThreadGroupIdYDimension();
        } else {
            workgroupCount = inputWalker->getThreadGroupIdZDimension();
        }

        cmd.setPartitionSize((workgroupCount + partitionCount - 1u) / partitionCount);
    }
    *computeWalker = cmd;
}

/* SAMPLE COMMAND BUFFER STRUCTURE, birds eye view for 16 partitions, 4 tiles
//inital setup section
1. MI_LOAD_REGISTER(PREDICATION_MASK, active partition mask )
//loop 1 - loop as long as there are partitions to be serviced
2. MI_ATOMIC_INC( ATOMIC LOCATION #31 within CMD buffer ) 
3. MI_LOAD_REGISTER_REG ( ATOMIC RESULT -> WPARID )
4. MI_SET_PREDICATE( WPARID MODE ) 
5. BATCH_BUFFER_START( LOCATION #28 ) // this will not be executed if partition outside of active virtual partitions
//loop 1 ends here, if we are here it means there are no more partitions
6. MI_SET_PREDICATE ( OFF )
//Walker synchronization section starts here, make sure that Walker is done
7, PIPE_CONTROL ( DC_FLUSH )
//wait for all post syncs to make sure whole work is done, caller needs to set them to 1.
//now epilogue starts synchro all engines prior to coming back to RING, this will be once per command buffer to make sure that all engines actually passed via cmd buffer.
//epilogue section, make sure every tile completed prior to continuing
//This is cross-tile synchronization
24. ATOMIC_INC( LOCATION #31)
25. WAIT_FOR_SEMAPHORE ( LOCATION #31, LOWER THEN 4 ) // wait till all tiles hit atomic
26. PIPE_CONTROL ( TAG UPDATE ) (not implemented)
27. BATCH_BUFFER_STAT (LOCATION #32) // go to the very end
//Walker section
28. COMPUTE_WALKER
29. BATCH BUFFER_START ( GO BACK TO #2)
//Batch Buffer Control Data section, there are no real commands here but we have memory here
//That will be updated via atomic operations.
30. uint32_t virtualPartitionID //atomic location
31. uint32_t completionTileID //all tiles needs to report completion
32. BATCH_BUFFER_END ( optional )
*/

template <typename GfxFamily>
void constructDynamicallyPartitionedCommandBuffer(void *cpuPointer,
                                                  uint64_t gpuAddressOfAllocation,
                                                  COMPUTE_WALKER<GfxFamily> *inputWalker,
                                                  uint32_t &totalBytesProgrammed,
                                                  uint32_t partitionCount,
                                                  uint32_t tileCount,
                                                  bool emitBatchBufferEnd,
                                                  bool synchronizeBeforeExecution,
                                                  bool secondaryBatchBuffer,
                                                  bool nativeCrossTileAtomicSync,
                                                  bool useAtomicsForNativeCleanup) {
    totalBytesProgrammed = 0u;
    void *currentBatchBufferPointer = cpuPointer;

    auto controlSectionOffset = computeControlSectionOffset<GfxFamily>(partitionCount, synchronizeBeforeExecution, nativeCrossTileAtomicSync, useAtomicsForNativeCleanup);
    if (synchronizeBeforeExecution) {
        auto tileAtomicAddress = gpuAddressOfAllocation + controlSectionOffset + offsetof(BatchBufferControlData, inTileCount);
        programMiAtomic<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, tileAtomicAddress, false, MI_ATOMIC<GfxFamily>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT);

        //if all tiles hit the atomic, it means we may go further
        programWaitForSemaphore<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, tileAtomicAddress, tileCount, MI_SEMAPHORE_WAIT<GfxFamily>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    }

    programWparidMask<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, partitionCount);

    programMiAtomic<GfxFamily>(currentBatchBufferPointer,
                               totalBytesProgrammed,
                               gpuAddressOfAllocation + controlSectionOffset,
                               true,
                               MI_ATOMIC<GfxFamily>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT);

    //move atomic result to wparid
    programMiLoadRegisterReg<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, generalPurposeRegister4, wparidCCSOffset);

    //enable predication basing on wparid value
    programWparidPredication<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, true);

    programMiBatchBufferStart<GfxFamily>(currentBatchBufferPointer,
                                         totalBytesProgrammed,
                                         gpuAddressOfAllocation +
                                             computeWalkerSectionStart<GfxFamily>(partitionCount,
                                                                                  synchronizeBeforeExecution,
                                                                                  nativeCrossTileAtomicSync,
                                                                                  useAtomicsForNativeCleanup),
                                         true,
                                         secondaryBatchBuffer);

    //disable predication to not noop subsequent commands.
    programWparidPredication<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, false);

    if (nativeCrossTileAtomicSync) {
        const auto finalSyncTileCountField = gpuAddressOfAllocation + controlSectionOffset + offsetof(BatchBufferControlData, finalSyncTileCount);
        programNativeCrossTileSyncControl<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, finalSyncTileCountField, useAtomicsForNativeCleanup);
    }

    programPipeControlCommand<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, true);

    if (isSemaphoreProgrammingRequired()) {
        auto postSyncAddress = inputWalker->getPostSync().getDestinationAddress() + 8llu;
        for (uint32_t partitionId = 0u; partitionId < partitionCount; partitionId++) {
            programWaitForSemaphore<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, postSyncAddress + partitionId * 16llu, 1u, MI_SEMAPHORE_WAIT<GfxFamily>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        }
    }

    if (isCrossTileAtomicRequired() || nativeCrossTileAtomicSync) {
        auto tileAtomicAddress = gpuAddressOfAllocation + controlSectionOffset + offsetof(BatchBufferControlData, tileCount);
        programMiAtomic<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, tileAtomicAddress, false, MI_ATOMIC<GfxFamily>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT);

        //if all tiles hit the atomic, it means we may go further
        programWaitForSemaphore<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, tileAtomicAddress, tileCount, MI_SEMAPHORE_WAIT<GfxFamily>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
    }

    //this bb start goes to the end of partitioned command buffer
    programMiBatchBufferStart<GfxFamily>(
        currentBatchBufferPointer,
        totalBytesProgrammed,
        gpuAddressOfAllocation + controlSectionOffset + sizeof(BatchBufferControlData),
        false,
        secondaryBatchBuffer);

    //Walker section
    programPartitionedWalker<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, inputWalker, partitionCount);

    programMiBatchBufferStart<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, gpuAddressOfAllocation, false, secondaryBatchBuffer);

    auto controlSection = reinterpret_cast<BatchBufferControlData *>(ptrOffset(cpuPointer, static_cast<size_t>(controlSectionOffset)));
    controlSection->partitionCount = 0u;
    controlSection->tileCount = 0u;
    controlSection->inTileCount = 0u;
    controlSection->finalSyncTileCount = 0u;
    totalBytesProgrammed += sizeof(BatchBufferControlData);
    currentBatchBufferPointer = ptrOffset(currentBatchBufferPointer, sizeof(BatchBufferControlData));

    if (nativeCrossTileAtomicSync) {
        const auto finalSyncTileCountAddress = gpuAddressOfAllocation + controlSectionOffset + offsetof(BatchBufferControlData, finalSyncTileCount);
        programNativeCrossTileSyncCleanup<GfxFamily>(currentBatchBufferPointer,
                                                     totalBytesProgrammed,
                                                     finalSyncTileCountAddress,
                                                     gpuAddressOfAllocation + controlSectionOffset,
                                                     dynamicPartitioningFieldsForCleanupCount,
                                                     tileCount,
                                                     useAtomicsForNativeCleanup);
    }

    if (emitBatchBufferEnd) {
        auto batchBufferEnd = putCommand<BATCH_BUFFER_END<GfxFamily>>(currentBatchBufferPointer, totalBytesProgrammed);
        *batchBufferEnd = GfxFamily::cmdInitBatchBufferEnd;
    }
}

struct StaticPartitioningControlSection {
    uint32_t synchronizeBeforeWalkerCounter = 0;
    uint32_t synchronizeAfterWalkerCounter = 0;
    uint32_t finalSyncTileCounter = 0;
};
static constexpr inline size_t staticPartitioningFieldsForCleanupCount = sizeof(StaticPartitioningControlSection) / sizeof(uint32_t) - 1;

template <typename GfxFamily>
uint64_t computeStaticPartitioningControlSectionOffset(uint32_t partitionCount, bool synchronizeBeforeExecution, bool nativeCrossTileAtomicSync, bool useAtomicsForNativeCleanup) {
    const auto beforeExecutionSyncAtomicSize = synchronizeBeforeExecution ? (sizeof(MI_SEMAPHORE_WAIT<GfxFamily>) + sizeof(MI_ATOMIC<GfxFamily>)) : 0u;
    const auto afterExecutionSyncAtomicSize = (isCrossTileAtomicRequired() || nativeCrossTileAtomicSync) ? (sizeof(MI_SEMAPHORE_WAIT<GfxFamily>) + sizeof(MI_ATOMIC<GfxFamily>)) : 0u;
    const auto afterExecutionSyncPostSyncSize = isSemaphoreProgrammingRequired() ? sizeof(MI_SEMAPHORE_WAIT<GfxFamily>) * partitionCount : 0u;
    const auto nativeCrossTileSyncSize = nativeCrossTileAtomicSync ? computeNativeCrossTileSyncControlSectionSize<GfxFamily>(useAtomicsForNativeCleanup) : 0u;
    return beforeExecutionSyncAtomicSize +
           sizeof(LOAD_REGISTER_MEM<GfxFamily>) +
           sizeof(PIPE_CONTROL<GfxFamily>) +
           sizeof(COMPUTE_WALKER<GfxFamily>) +
           nativeCrossTileSyncSize +
           afterExecutionSyncAtomicSize +
           afterExecutionSyncPostSyncSize +
           sizeof(BATCH_BUFFER_START<GfxFamily>);
}

template <typename GfxFamily>
void constructStaticallyPartitionedCommandBuffer(void *cpuPointer,
                                                 uint64_t gpuAddressOfAllocation,
                                                 COMPUTE_WALKER<GfxFamily> *inputWalker,
                                                 uint32_t &totalBytesProgrammed,
                                                 uint32_t partitionCount,
                                                 uint32_t tileCount,
                                                 bool synchronizeBeforeExecution,
                                                 bool secondaryBatchBuffer,
                                                 bool nativeCrossTileAtomicSync,
                                                 uint64_t workPartitionAllocationGpuVa,
                                                 bool useAtomicsForNativeCleanup) {
    totalBytesProgrammed = 0u;
    void *currentBatchBufferPointer = cpuPointer;

    // Get address of the control section
    const auto controlSectionOffset = computeStaticPartitioningControlSectionOffset<GfxFamily>(partitionCount, synchronizeBeforeExecution, nativeCrossTileAtomicSync, useAtomicsForNativeCleanup);
    const auto afterControlSectionOffset = controlSectionOffset + sizeof(StaticPartitioningControlSection);

    // Synchronize tiles before walker
    if (synchronizeBeforeExecution) {
        const auto atomicAddress = gpuAddressOfAllocation + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
        programTilesSynchronizationWithAtomics<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, atomicAddress, tileCount);
    }

    // Load partition ID to wparid register and execute walker
    programMiLoadRegisterMem<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, workPartitionAllocationGpuVa, wparidCCSOffset);
    programPartitionedWalker<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, inputWalker, partitionCount);

    // Prepare for cleanup section
    if (nativeCrossTileAtomicSync) {
        const auto finalSyncTileCountField = gpuAddressOfAllocation + controlSectionOffset + offsetof(StaticPartitioningControlSection, finalSyncTileCounter);
        programNativeCrossTileSyncControl<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, finalSyncTileCountField, useAtomicsForNativeCleanup);
    }

    programPipeControlCommand<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, true); // flush L3 cache

    // Synchronize tiles after walker
    if (isSemaphoreProgrammingRequired()) {
        programTilesSynchronizationWithPostSyncs<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, inputWalker, partitionCount);
    }
    if (isCrossTileAtomicRequired() || nativeCrossTileAtomicSync) {
        const auto atomicAddress = gpuAddressOfAllocation + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
        programTilesSynchronizationWithAtomics<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, atomicAddress, tileCount);
    }

    // Jump over the control section
    programMiBatchBufferStart<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, gpuAddressOfAllocation + afterControlSectionOffset, false, secondaryBatchBuffer);

    // Control section
    DEBUG_BREAK_IF(totalBytesProgrammed != controlSectionOffset);
    StaticPartitioningControlSection *controlSection = putCommand<StaticPartitioningControlSection>(currentBatchBufferPointer, totalBytesProgrammed);
    controlSection->synchronizeBeforeWalkerCounter = 0u;
    controlSection->synchronizeAfterWalkerCounter = 0u;
    controlSection->finalSyncTileCounter = 0u;
    DEBUG_BREAK_IF(totalBytesProgrammed != afterControlSectionOffset);

    // Cleanup section
    if (nativeCrossTileAtomicSync) {
        const auto finalSyncTileCountAddress = gpuAddressOfAllocation + controlSectionOffset + offsetof(StaticPartitioningControlSection, finalSyncTileCounter);
        programNativeCrossTileSyncCleanup<GfxFamily>(currentBatchBufferPointer,
                                                     totalBytesProgrammed,
                                                     finalSyncTileCountAddress,
                                                     gpuAddressOfAllocation + controlSectionOffset,
                                                     staticPartitioningFieldsForCleanupCount,
                                                     tileCount,
                                                     useAtomicsForNativeCleanup);
    }
}

template <typename GfxFamily>
uint64_t estimateSpaceRequiredInCommandBuffer(bool requiresBatchBufferEnd,
                                              uint32_t partitionCount,
                                              bool synchronizeBeforeExecution,
                                              bool nativeCrossTileAtomicSync,
                                              bool staticPartitioning,
                                              bool useAtomicsForNativeCleanup) {
    uint64_t size = {};
    if (staticPartitioning) {
        size += computeStaticPartitioningControlSectionOffset<GfxFamily>(partitionCount, synchronizeBeforeExecution, nativeCrossTileAtomicSync, useAtomicsForNativeCleanup);
        size += sizeof(StaticPartitioningControlSection);
        size += nativeCrossTileAtomicSync ? computeNativeCrossTileSyncCleanupSectionSize<GfxFamily>(staticPartitioningFieldsForCleanupCount, useAtomicsForNativeCleanup) : 0u;
    } else {
        size += computeControlSectionOffset<GfxFamily>(partitionCount, synchronizeBeforeExecution, nativeCrossTileAtomicSync, useAtomicsForNativeCleanup);
        size += sizeof(BatchBufferControlData);
        size += requiresBatchBufferEnd ? sizeof(BATCH_BUFFER_END<GfxFamily>) : 0u;
        size += nativeCrossTileAtomicSync ? computeNativeCrossTileSyncCleanupSectionSize<GfxFamily>(dynamicPartitioningFieldsForCleanupCount, useAtomicsForNativeCleanup) : 0u;
    }
    return size;
}

} // namespace WalkerPartition
