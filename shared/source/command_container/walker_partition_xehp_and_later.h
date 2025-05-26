/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/walker_partition_interface.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/string.h"

#include <cassert>
#include <optional>

namespace NEO {
struct PipeControlArgs;
}

namespace WalkerPartition {

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
template <typename GfxFamily>
using POST_SYNC_OPERATION = typename PIPE_CONTROL<GfxFamily>::POST_SYNC_OPERATION;

template <typename GfxFamily, typename WalkerType>
void appendWalkerFields(WalkerType &walkerCmd, uint32_t tileCount, uint32_t workgroupCount);

template <typename Command>
Command *putCommand(void *&inputAddress, uint32_t &totalBytesProgrammed) {
    totalBytesProgrammed += sizeof(Command);
    auto commandToReturn = reinterpret_cast<Command *>(inputAddress);
    inputAddress = ptrOffset(inputAddress, sizeof(Command));
    return commandToReturn;
}

inline void *putCommand(void *&inputAddress, uint32_t &totalBytesProgrammed, size_t commandSize) {
    totalBytesProgrammed += static_cast<uint32_t>(commandSize);
    auto commandToReturn = inputAddress;
    inputAddress = ptrOffset(inputAddress, commandSize);
    return commandToReturn;
}

template <typename GfxFamily, typename WalkerType>
uint32_t computePartitionCountAndPartitionType(uint32_t preferredMinimalPartitionCount,
                                               bool preferStaticPartitioning,
                                               const Vec3<size_t> &groupStart,
                                               const Vec3<size_t> &groupCount,
                                               std::optional<typename WalkerType::PARTITION_TYPE> requestedPartitionType,
                                               typename WalkerType::PARTITION_TYPE *outSelectedPartitionType,
                                               bool *outSelectStaticPartitioning) {

    using PARTITION_TYPE = typename WalkerType::PARTITION_TYPE;
    // For non uniform starting point, there is no support for partition in Hardware. Disable partitioning and select dynamic algorithm
    if (groupStart.x || groupStart.y || groupStart.z) {
        *outSelectedPartitionType = PARTITION_TYPE::PARTITION_TYPE_DISABLED;
        *outSelectStaticPartitioning = false;
        return 1u;
    }

    size_t workgroupCount = 0u;
    bool disablePartitionForPartitionCountOne{};

    if (NEO::debugManager.flags.ExperimentalSetWalkerPartitionType.get() != -1) {
        requestedPartitionType = static_cast<PARTITION_TYPE>(NEO::debugManager.flags.ExperimentalSetWalkerPartitionType.get());
    }

    if (requestedPartitionType.has_value()) {
        switch (requestedPartitionType.value()) {
        case PARTITION_TYPE::PARTITION_TYPE_X:
            workgroupCount = groupCount.x;
            break;
        case PARTITION_TYPE::PARTITION_TYPE_Y:
            workgroupCount = groupCount.y;
            break;
        case PARTITION_TYPE::PARTITION_TYPE_Z:
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
        if (NEO::debugManager.flags.WalkerPartitionPreferHighestDimension.get() != -1) {
            goWithMaxAlgorithm = !!!NEO::debugManager.flags.WalkerPartitionPreferHighestDimension.get();
        }

        // compute misaligned %, accept imbalance below threshold in favor of Z/Y/X distribution.
        const float minimalThreshold = 0.05f;
        float zImbalance = static_cast<float>(groupCount.z - alignDown(groupCount.z, preferredMinimalPartitionCount)) / static_cast<float>(groupCount.z);
        float yImbalance = static_cast<float>(groupCount.y - alignDown(groupCount.y, preferredMinimalPartitionCount)) / static_cast<float>(groupCount.y);

        // we first try with deepest dimension to see if we can partition there
        if (groupCount.z > 1 && (zImbalance <= minimalThreshold)) {
            *outSelectedPartitionType = PARTITION_TYPE::PARTITION_TYPE_Z;
        } else if (groupCount.y > 1 && (yImbalance < minimalThreshold)) {
            *outSelectedPartitionType = PARTITION_TYPE::PARTITION_TYPE_Y;
        } else if (groupCount.x % preferredMinimalPartitionCount == 0) {
            *outSelectedPartitionType = PARTITION_TYPE::PARTITION_TYPE_X;
        }
        // if we are here then there is no dimension that results in even distribution, choose max dimension to minimize impact
        else {
            goWithMaxAlgorithm = true;
        }

        if (goWithMaxAlgorithm) {
            // default mode, select greatest dimension
            if (maxDimension == groupCount.x) {
                *outSelectedPartitionType = PARTITION_TYPE::PARTITION_TYPE_X;
            } else if (maxDimension == groupCount.y) {
                *outSelectedPartitionType = PARTITION_TYPE::PARTITION_TYPE_Y;
            } else {
                *outSelectedPartitionType = PARTITION_TYPE::PARTITION_TYPE_Z;
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
    if (NEO::debugManager.flags.SetMinimalPartitionSize.get() != 0) {
        const auto workgroupPerPartitionThreshold = NEO::debugManager.flags.SetMinimalPartitionSize.get() == -1
                                                        ? 512u
                                                        : static_cast<unsigned>(NEO::debugManager.flags.SetMinimalPartitionSize.get());
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
        *outSelectedPartitionType = PARTITION_TYPE::PARTITION_TYPE_DISABLED;
    }

    return static_cast<uint32_t>(partitionCount);
}

template <typename GfxFamily, typename WalkerType>
uint32_t computePartitionCountAndSetPartitionType(WalkerType *walker,
                                                  NEO::RequiredPartitionDim requiredPartitionDim,
                                                  uint32_t preferredMinimalPartitionCount,
                                                  bool preferStaticPartitioning,
                                                  bool *outSelectStaticPartitioning) {

    using PARTITION_TYPE = typename WalkerType::PARTITION_TYPE;

    const Vec3<size_t> groupStart = {walker->getThreadGroupIdStartingX(), walker->getThreadGroupIdStartingY(), walker->getThreadGroupIdStartingZ()};
    const Vec3<size_t> groupCount = {walker->getThreadGroupIdXDimension(), walker->getThreadGroupIdYDimension(), walker->getThreadGroupIdZDimension()};
    std::optional<PARTITION_TYPE> requestedPartitionType{};

    switch (requiredPartitionDim) {
    case NEO::RequiredPartitionDim::x:
        requestedPartitionType = PARTITION_TYPE::PARTITION_TYPE_X;
        break;
    case NEO::RequiredPartitionDim::y:
        requestedPartitionType = PARTITION_TYPE::PARTITION_TYPE_Y;
        break;
    case NEO::RequiredPartitionDim::z:
        requestedPartitionType = PARTITION_TYPE::PARTITION_TYPE_Z;
        break;
    default:
        UNRECOVERABLE_IF(requiredPartitionDim != NEO::RequiredPartitionDim::none);
        break;
    }

    PARTITION_TYPE partitionType{};
    const auto partitionCount = computePartitionCountAndPartitionType<GfxFamily, WalkerType>(preferredMinimalPartitionCount,
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
    NEO::EncodeSemaphore<GfxFamily>::programMiSemaphoreWait(semaphoreWait, gpuAddress, semaphoreCompareValue, compareOperation, false, true, false, false, false);
}

template <typename GfxFamily>
bool programWparidMask(void *&inputAddress, uint32_t &totalBytesProgrammed, uint32_t partitionCount) {
    // currently only power of 2 values of partitionCount are being supported
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
void programPipeControlCommand(void *&inputAddress, uint32_t &totalBytesProgrammed, NEO::PipeControlArgs &flushArgs) {
    auto singleBarrierSize = NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();

    auto pipeControl = putCommand(inputAddress, totalBytesProgrammed, singleBarrierSize);

    UNRECOVERABLE_IF(sizeof(PIPE_CONTROL<GfxFamily>) < singleBarrierSize);
    uint8_t cmd[sizeof(PIPE_CONTROL<GfxFamily>)] = {};

    NEO::MemorySynchronizationCommands<GfxFamily>::setSingleBarrier(cmd, flushArgs);

    memcpy_s(pipeControl, singleBarrierSize, cmd, singleBarrierSize);
}

template <typename GfxFamily>
void programPostSyncPipeControlCommand(void *&inputAddress,
                                       uint32_t &totalBytesProgrammed,
                                       WalkerPartitionArgs &args,
                                       NEO::PipeControlArgs &flushArgs,
                                       const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {

    NEO::MemorySynchronizationCommands<GfxFamily>::setBarrierWithPostSyncOperation(inputAddress,
                                                                                   NEO::PostSyncMode::immediateData,
                                                                                   args.postSyncGpuAddress,
                                                                                   args.postSyncImmediateValue,
                                                                                   rootDeviceEnvironment,
                                                                                   flushArgs);

    totalBytesProgrammed += static_cast<uint32_t>(NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::immediateData));
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
uint64_t computeSelfCleanupSectionSize(bool useAtomicsForSelfCleanup) {
    if (useAtomicsForSelfCleanup) {
        return sizeof(MI_ATOMIC<GfxFamily>);
    } else {
        return sizeof(MI_STORE_DATA_IMM<GfxFamily>);
    }
}

template <typename GfxFamily>
void programSelfCleanupSection(void *&inputAddress,
                               uint32_t &totalBytesProgrammed,
                               uint64_t address,
                               bool useAtomicsForSelfCleanup) {
    if (useAtomicsForSelfCleanup) {
        programMiAtomic<GfxFamily>(inputAddress,
                                   totalBytesProgrammed,
                                   address,
                                   false,
                                   MI_ATOMIC<GfxFamily>::ATOMIC_OPCODES::ATOMIC_4B_MOVE);
    } else {
        programStoreMemImmediateDword<GfxFamily>(inputAddress,
                                                 totalBytesProgrammed,
                                                 address,
                                                 0u);
    }
}

template <typename GfxFamily>
uint64_t computeTilesSynchronizationWithAtomicsSectionSize() {
    return sizeof(MI_ATOMIC<GfxFamily>) +
           NEO::EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait();
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
uint64_t computeSelfCleanupEndSectionSize(size_t fieldsForCleanupCount, WalkerPartitionArgs &args) {
    size_t extraSize = 0;
    if (args.pipeControlBeforeCleanupCrossTileSync) {
        extraSize += 2 * NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
    }
    return fieldsForCleanupCount * computeSelfCleanupSectionSize<GfxFamily>(args.useAtomicsForSelfCleanup) +
           2 * computeTilesSynchronizationWithAtomicsSectionSize<GfxFamily>() + extraSize;
}

template <typename GfxFamily>
void programSelfCleanupEndSection(void *&inputAddress,
                                  uint32_t &totalBytesProgrammed,
                                  uint64_t finalSyncTileCountAddress,
                                  uint64_t baseAddressForCleanup,
                                  size_t fieldsForCleanupCount,
                                  WalkerPartitionArgs &args) {
    NEO::PipeControlArgs pipeControlArgs;
    if (args.pipeControlBeforeCleanupCrossTileSync) {
        programPipeControlCommand<GfxFamily>(inputAddress, totalBytesProgrammed, pipeControlArgs);
    }

    // Synchronize tiles, so the fields are not cleared while still in use
    programTilesSynchronizationWithAtomics<GfxFamily>(inputAddress, totalBytesProgrammed, finalSyncTileCountAddress, args.tileCount);

    for (auto fieldIndex = 0u; fieldIndex < fieldsForCleanupCount; fieldIndex++) {
        const uint64_t addressForCleanup = baseAddressForCleanup + fieldIndex * sizeof(uint32_t);
        programSelfCleanupSection<GfxFamily>(inputAddress,
                                             totalBytesProgrammed,
                                             addressForCleanup,
                                             args.useAtomicsForSelfCleanup);
    }

    if (args.pipeControlBeforeCleanupCrossTileSync) {
        programPipeControlCommand<GfxFamily>(inputAddress, totalBytesProgrammed, pipeControlArgs);
    }

    // this synchronization point ensures that all tiles finished zeroing and will fairly access control section atomic variables
    programTilesSynchronizationWithAtomics<GfxFamily>(inputAddress, totalBytesProgrammed, finalSyncTileCountAddress, 2 * args.tileCount);
}

template <typename GfxFamily, typename WalkerType>
void programTilesSynchronizationWithPostSyncs(void *&currentBatchBufferPointer,
                                              uint32_t &totalBytesProgrammed,
                                              WalkerType *inputWalker,
                                              uint32_t partitionCount) {
    const auto postSyncAddress = inputWalker->getPostSync().getDestinationAddress() + 8llu;
    for (uint32_t partitionId = 0u; partitionId < partitionCount; partitionId++) {
        programWaitForSemaphore<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, postSyncAddress + partitionId * 16llu, 1u, MI_SEMAPHORE_WAIT<GfxFamily>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
    }
}

template <typename GfxFamily, typename WalkerType>
uint64_t computeWalkerSectionSize() {
    return sizeof(BATCH_BUFFER_START<GfxFamily>) +
           sizeof(WalkerType);
}

template <typename GfxFamily, typename WalkerType>
uint64_t computeControlSectionOffset(WalkerPartitionArgs &args) {
    uint64_t size = 0u;

    size += args.synchronizeBeforeExecution ? computeTilesSynchronizationWithAtomicsSectionSize<GfxFamily>() : 0;
    size += sizeof(LOAD_REGISTER_IMM<GfxFamily>); // predication mask
    size += sizeof(MI_ATOMIC<GfxFamily>);         // current id for partition
    size += sizeof(LOAD_REGISTER_REG<GfxFamily>); // id into register
    size += sizeof(MI_SET_PREDICATE<GfxFamily>) * 2 +
            sizeof(BATCH_BUFFER_START<GfxFamily>) * 2;
    size += (args.semaphoreProgrammingRequired ? NEO::EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait() * args.partitionCount : 0u);
    size += computeWalkerSectionSize<GfxFamily, WalkerType>();
    size += args.emitPipeControlStall ? NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier() : 0u;
    if (args.crossTileAtomicSynchronization || args.emitSelfCleanup) {
        size += computeTilesSynchronizationWithAtomicsSectionSize<GfxFamily>();
    }
    if (args.emitSelfCleanup) {
        size += computeSelfCleanupSectionSize<GfxFamily>(args.useAtomicsForSelfCleanup);
    }
    size += args.preferredStaticPartitioning ? sizeof(LOAD_REGISTER_MEM<GfxFamily>) : 0u;
    return size;
}

template <typename GfxFamily, typename WalkerType>
uint64_t computeWalkerSectionStart(WalkerPartitionArgs &args) {
    return computeControlSectionOffset<GfxFamily, WalkerType>(args) -
           computeWalkerSectionSize<GfxFamily, WalkerType>();
}

template <typename GfxFamily, typename WalkerType>
void *programPartitionedWalker(void *&inputAddress, uint32_t &totalBytesProgrammed,
                               WalkerType *inputWalker,
                               WalkerPartitionArgs &args,
                               const NEO::Device &device) {
    WalkerType *computeWalker = nullptr;
    if (!args.blockDispatchToCommandBuffer) {
        computeWalker = putCommand<WalkerType>(inputAddress, totalBytesProgrammed);
    }

    if (args.partitionCount > 1) {
        auto partitionType = inputWalker->getPartitionType();

        assert(inputWalker->getThreadGroupIdStartingX() == 0u);
        assert(inputWalker->getThreadGroupIdStartingY() == 0u);
        assert(inputWalker->getThreadGroupIdStartingZ() == 0u);
        assert(partitionType != WalkerType::PARTITION_TYPE::PARTITION_TYPE_DISABLED);

        inputWalker->setWorkloadPartitionEnable(true);

        uint32_t workgroupCount = 0;
        if (partitionType == WalkerType::PARTITION_TYPE::PARTITION_TYPE_X) {
            workgroupCount = inputWalker->getThreadGroupIdXDimension();
        } else if (partitionType == WalkerType::PARTITION_TYPE::PARTITION_TYPE_Y) {
            workgroupCount = inputWalker->getThreadGroupIdYDimension();
        } else {
            workgroupCount = inputWalker->getThreadGroupIdZDimension();
        }

        if (args.forceExecutionOnSingleTile) {
            inputWalker->setPartitionSize(workgroupCount);
        } else {
            inputWalker->setPartitionSize(Math::divideAndRoundUp(workgroupCount, args.partitionCount));
        }

        NEO::EncodeDispatchKernel<GfxFamily>::setWalkerRegionSettings(*inputWalker,
                                                                      device,
                                                                      args.partitionCount,
                                                                      args.workgroupSize,
                                                                      args.threadGroupCount,
                                                                      args.maxWgCountPerTile,
                                                                      args.isRequiredDispatchWorkGroupOrder);

        appendWalkerFields<GfxFamily, WalkerType>(*inputWalker, args.tileCount, workgroupCount);
    }

    if (computeWalker != nullptr) {
        *computeWalker = *inputWalker;
    }

    return computeWalker;
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

template <typename GfxFamily, typename WalkerType>
void constructDynamicallyPartitionedCommandBuffer(void *cpuPointer,
                                                  void **outWalkerPtr,
                                                  uint64_t gpuAddressOfAllocation,
                                                  WalkerType *inputWalker,
                                                  uint32_t &totalBytesProgrammed,
                                                  WalkerPartitionArgs &args,
                                                  const NEO::Device &device) {
    totalBytesProgrammed = 0u;
    void *currentBatchBufferPointer = cpuPointer;

    auto controlSectionOffset = computeControlSectionOffset<GfxFamily, WalkerType>(args);
    if (args.synchronizeBeforeExecution) {
        auto tileAtomicAddress = gpuAddressOfAllocation + controlSectionOffset + offsetof(BatchBufferControlData, inTileCount);
        programTilesSynchronizationWithAtomics<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, tileAtomicAddress, args.tileCount);
    }

    programWparidMask<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, args.partitionCount);

    programMiAtomic<GfxFamily>(currentBatchBufferPointer,
                               totalBytesProgrammed,
                               gpuAddressOfAllocation + controlSectionOffset,
                               true,
                               MI_ATOMIC<GfxFamily>::ATOMIC_OPCODES::ATOMIC_4B_INCREMENT);

    // move atomic result to wparid
    programMiLoadRegisterReg<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, generalPurposeRegister4, wparidCCSOffset);

    // enable predication basing on wparid value
    programWparidPredication<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, true);

    programMiBatchBufferStart<GfxFamily>(currentBatchBufferPointer,
                                         totalBytesProgrammed,
                                         gpuAddressOfAllocation +
                                             computeWalkerSectionStart<GfxFamily, WalkerType>(args),
                                         true,
                                         args.secondaryBatchBuffer);

    // disable predication to not noop subsequent commands.
    programWparidPredication<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, false);

    if (args.emitSelfCleanup) {
        const auto finalSyncTileCountField = gpuAddressOfAllocation + controlSectionOffset + offsetof(BatchBufferControlData, finalSyncTileCount);
        programSelfCleanupSection<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, finalSyncTileCountField, args.useAtomicsForSelfCleanup);
    }

    if (args.emitPipeControlStall) {
        NEO::PipeControlArgs pipeControlArgs;
        pipeControlArgs.dcFlushEnable = args.dcFlushEnable;
        programPipeControlCommand<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, pipeControlArgs);
    }

    if (args.semaphoreProgrammingRequired) {
        auto postSyncAddress = inputWalker->getPostSync().getDestinationAddress() + 8llu;
        for (uint32_t partitionId = 0u; partitionId < args.partitionCount; partitionId++) {
            programWaitForSemaphore<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, postSyncAddress + partitionId * 16llu, 1u, MI_SEMAPHORE_WAIT<GfxFamily>::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        }
    }

    if (args.crossTileAtomicSynchronization || args.emitSelfCleanup) {
        auto tileAtomicAddress = gpuAddressOfAllocation + controlSectionOffset + offsetof(BatchBufferControlData, tileCount);
        programTilesSynchronizationWithAtomics<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, tileAtomicAddress, args.tileCount);
    }

    if (args.preferredStaticPartitioning) {
        programMiLoadRegisterMem<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, args.workPartitionAllocationGpuVa, wparidCCSOffset);
    }

    // this bb start goes to the end of partitioned command buffer
    programMiBatchBufferStart<GfxFamily>(
        currentBatchBufferPointer,
        totalBytesProgrammed,
        gpuAddressOfAllocation + controlSectionOffset + sizeof(BatchBufferControlData),
        false,
        args.secondaryBatchBuffer);

    // Walker section
    auto walkerPtr = programPartitionedWalker<GfxFamily, WalkerType>(currentBatchBufferPointer, totalBytesProgrammed, inputWalker, args, device);
    if (outWalkerPtr) {
        *outWalkerPtr = walkerPtr;
    }

    programMiBatchBufferStart<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, gpuAddressOfAllocation, false, args.secondaryBatchBuffer);

    auto controlSection = reinterpret_cast<BatchBufferControlData *>(ptrOffset(cpuPointer, static_cast<size_t>(controlSectionOffset)));
    controlSection->partitionCount = 0u;
    controlSection->tileCount = 0u;
    controlSection->inTileCount = 0u;
    controlSection->finalSyncTileCount = 0u;
    totalBytesProgrammed += sizeof(BatchBufferControlData);
    currentBatchBufferPointer = ptrOffset(currentBatchBufferPointer, sizeof(BatchBufferControlData));

    if (args.emitSelfCleanup) {
        const auto finalSyncTileCountAddress = gpuAddressOfAllocation + controlSectionOffset + offsetof(BatchBufferControlData, finalSyncTileCount);
        programSelfCleanupEndSection<GfxFamily>(currentBatchBufferPointer,
                                                totalBytesProgrammed,
                                                finalSyncTileCountAddress,
                                                gpuAddressOfAllocation + controlSectionOffset,
                                                dynamicPartitioningFieldsForCleanupCount,
                                                args);
    }

    if (args.emitBatchBufferEnd) {
        auto batchBufferEnd = putCommand<BATCH_BUFFER_END<GfxFamily>>(currentBatchBufferPointer, totalBytesProgrammed);
        *batchBufferEnd = GfxFamily::cmdInitBatchBufferEnd;
    }
}

template <typename GfxFamily>
bool isStartAndControlSectionRequired(WalkerPartitionArgs &args) {
    return args.synchronizeBeforeExecution || args.crossTileAtomicSynchronization || args.emitSelfCleanup;
}

template <typename GfxFamily, typename WalkerType>
uint64_t computeStaticPartitioningControlSectionOffset(WalkerPartitionArgs &args) {
    const auto beforeExecutionSyncAtomicSize = args.synchronizeBeforeExecution
                                                   ? computeTilesSynchronizationWithAtomicsSectionSize<GfxFamily>()
                                                   : 0u;
    const auto afterExecutionSyncAtomicSize = (args.crossTileAtomicSynchronization || args.emitSelfCleanup)
                                                  ? computeTilesSynchronizationWithAtomicsSectionSize<GfxFamily>()
                                                  : 0u;
    const auto afterExecutionSyncPostSyncSize = args.semaphoreProgrammingRequired
                                                    ? NEO::EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait() * args.partitionCount
                                                    : 0u;
    const auto selfCleanupSectionSize = args.emitSelfCleanup
                                            ? computeSelfCleanupSectionSize<GfxFamily>(args.useAtomicsForSelfCleanup)
                                            : 0u;
    const auto wparidRegisterSize = args.initializeWparidRegister
                                        ? sizeof(LOAD_REGISTER_MEM<GfxFamily>)
                                        : 0u;
    const auto pipeControlSize = args.emitPipeControlStall
                                     ? NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier()
                                     : 0u;
    const auto bbStartSize = isStartAndControlSectionRequired<GfxFamily>(args)
                                 ? sizeof(BATCH_BUFFER_START<GfxFamily>)
                                 : 0u;
    return beforeExecutionSyncAtomicSize +
           wparidRegisterSize +
           pipeControlSize +
           sizeof(WalkerType) +
           selfCleanupSectionSize +
           afterExecutionSyncAtomicSize +
           afterExecutionSyncPostSyncSize +
           bbStartSize;
}

template <typename GfxFamily, typename WalkerType>
void constructStaticallyPartitionedCommandBuffer(void *cpuPointer,
                                                 void **outWalkerPtr,
                                                 uint64_t gpuAddressOfAllocation,
                                                 WalkerType *inputWalker,
                                                 uint32_t &totalBytesProgrammed,
                                                 WalkerPartitionArgs &args,
                                                 const NEO::Device &device) {
    totalBytesProgrammed = 0u;
    void *currentBatchBufferPointer = cpuPointer;

    // Get address of the control section
    const auto controlSectionOffset = args.blockDispatchToCommandBuffer ? 0u : computeStaticPartitioningControlSectionOffset<GfxFamily, WalkerType>(args);
    const auto afterControlSectionOffset = args.blockDispatchToCommandBuffer ? 0u : controlSectionOffset + sizeof(StaticPartitioningControlSection);

    if (!args.blockDispatchToCommandBuffer) {
        // Synchronize tiles before walker
        if (args.synchronizeBeforeExecution) {
            const auto atomicAddress = gpuAddressOfAllocation + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeBeforeWalkerCounter);
            programTilesSynchronizationWithAtomics<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, atomicAddress, args.tileCount);
        }

        // Load partition ID to wparid register and execute walker
        if (args.initializeWparidRegister) {
            programMiLoadRegisterMem<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, args.workPartitionAllocationGpuVa, wparidCCSOffset);
        }
    }

    auto walkerPtr = programPartitionedWalker<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, inputWalker, args, device);

    if (!args.blockDispatchToCommandBuffer) {
        if (outWalkerPtr) {
            *outWalkerPtr = walkerPtr;
        }

        // Prepare for cleanup section
        if (args.emitSelfCleanup) {
            const auto finalSyncTileCountField = gpuAddressOfAllocation + controlSectionOffset + offsetof(StaticPartitioningControlSection, finalSyncTileCounter);
            programSelfCleanupSection<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, finalSyncTileCountField, args.useAtomicsForSelfCleanup);
        }

        if (args.emitPipeControlStall) {
            NEO::PipeControlArgs pipeControlArgs;
            pipeControlArgs.dcFlushEnable = args.dcFlushEnable;
            programPipeControlCommand<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, pipeControlArgs);
        }

        // Synchronize tiles after walker
        if (args.semaphoreProgrammingRequired) {
            programTilesSynchronizationWithPostSyncs<GfxFamily, WalkerType>(currentBatchBufferPointer, totalBytesProgrammed, inputWalker, args.partitionCount);
        }

        if (args.crossTileAtomicSynchronization || args.emitSelfCleanup) {
            const auto atomicAddress = gpuAddressOfAllocation + controlSectionOffset + offsetof(StaticPartitioningControlSection, synchronizeAfterWalkerCounter);
            programTilesSynchronizationWithAtomics<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, atomicAddress, args.tileCount);
        }

        // Jump over the control section only when needed
        if (isStartAndControlSectionRequired<GfxFamily>(args)) {
            programMiBatchBufferStart<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, gpuAddressOfAllocation + afterControlSectionOffset, false, args.secondaryBatchBuffer);

            // Control section
            DEBUG_BREAK_IF(totalBytesProgrammed != controlSectionOffset);
            StaticPartitioningControlSection *controlSection = putCommand<StaticPartitioningControlSection>(currentBatchBufferPointer, totalBytesProgrammed);
            controlSection->synchronizeBeforeWalkerCounter = 0u;
            controlSection->synchronizeAfterWalkerCounter = 0u;
            controlSection->finalSyncTileCounter = 0u;
            DEBUG_BREAK_IF(totalBytesProgrammed != afterControlSectionOffset);
        }

        // Cleanup section
        if (args.emitSelfCleanup) {
            const auto finalSyncTileCountAddress = gpuAddressOfAllocation + controlSectionOffset + offsetof(StaticPartitioningControlSection, finalSyncTileCounter);
            programSelfCleanupEndSection<GfxFamily>(currentBatchBufferPointer,
                                                    totalBytesProgrammed,
                                                    finalSyncTileCountAddress,
                                                    gpuAddressOfAllocation + controlSectionOffset,
                                                    staticPartitioningFieldsForCleanupCount,
                                                    args);
        }
    }
}

template <typename GfxFamily, typename WalkerType>
uint64_t estimateSpaceRequiredInCommandBuffer(WalkerPartitionArgs &args) {
    uint64_t size = {};
    if (args.staticPartitioning) {
        size += computeStaticPartitioningControlSectionOffset<GfxFamily, WalkerType>(args);
        size += isStartAndControlSectionRequired<GfxFamily>(args) ? sizeof(StaticPartitioningControlSection) : 0u;
        size += args.emitSelfCleanup ? computeSelfCleanupEndSectionSize<GfxFamily>(staticPartitioningFieldsForCleanupCount, args) : 0u;
    } else {
        size += computeControlSectionOffset<GfxFamily, WalkerType>(args);
        size += sizeof(BatchBufferControlData);
        size += args.emitBatchBufferEnd ? sizeof(BATCH_BUFFER_END<GfxFamily>) : 0u;
        size += args.emitSelfCleanup ? computeSelfCleanupEndSectionSize<GfxFamily>(dynamicPartitioningFieldsForCleanupCount, args) : 0u;
    }
    return size;
}

template <typename GfxFamily>
uint64_t computeBarrierControlSectionOffset(WalkerPartitionArgs &args,
                                            const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    uint64_t offset = 0u;
    if (args.emitSelfCleanup) {
        offset += computeSelfCleanupSectionSize<GfxFamily>(args.useAtomicsForSelfCleanup);
    }

    if (args.usePostSync) {
        offset += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::immediateData);
    } else {
        offset += NEO::MemorySynchronizationCommands<GfxFamily>::getSizeForSingleBarrier();
    }

    offset += (computeTilesSynchronizationWithAtomicsSectionSize<GfxFamily>() +
               sizeof(BATCH_BUFFER_START<GfxFamily>));
    return offset;
}

template <typename GfxFamily>
uint64_t estimateBarrierSpaceRequiredInCommandBuffer(WalkerPartitionArgs &args,
                                                     const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    uint64_t size = computeBarrierControlSectionOffset<GfxFamily>(args, rootDeviceEnvironment) +
                    sizeof(BarrierControlSection);
    if (args.emitSelfCleanup) {
        size += computeSelfCleanupEndSectionSize<GfxFamily>(barrierControlSectionFieldsForCleanupCount, args);
    }
    return size;
}

template <typename GfxFamily>
void constructBarrierCommandBuffer(void *cpuPointer,
                                   uint64_t gpuAddressOfAllocation,
                                   uint32_t &totalBytesProgrammed,
                                   WalkerPartitionArgs &args,
                                   NEO::PipeControlArgs &flushArgs,
                                   const NEO::RootDeviceEnvironment &rootDeviceEnvironment) {
    void *currentBatchBufferPointer = cpuPointer;
    const auto controlSectionOffset = computeBarrierControlSectionOffset<GfxFamily>(args, rootDeviceEnvironment);

    const auto finalSyncTileCountField = gpuAddressOfAllocation + controlSectionOffset + offsetof(BarrierControlSection, finalSyncTileCount);
    if (args.emitSelfCleanup) {
        programSelfCleanupSection<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, finalSyncTileCountField, args.useAtomicsForSelfCleanup);
    }

    if (args.usePostSync) {
        programPostSyncPipeControlCommand<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, args, flushArgs, rootDeviceEnvironment);
    } else {
        programPipeControlCommand<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, flushArgs);
    }

    const auto crossTileSyncCountField = gpuAddressOfAllocation + controlSectionOffset + offsetof(BarrierControlSection, crossTileSyncCount);
    programTilesSynchronizationWithAtomics<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, crossTileSyncCountField, args.tileCount);

    const auto afterControlSectionOffset = controlSectionOffset + sizeof(BarrierControlSection);
    programMiBatchBufferStart<GfxFamily>(currentBatchBufferPointer, totalBytesProgrammed, gpuAddressOfAllocation + afterControlSectionOffset, false, args.secondaryBatchBuffer);

    DEBUG_BREAK_IF(totalBytesProgrammed != controlSectionOffset);
    BarrierControlSection *controlSection = putCommand<BarrierControlSection>(currentBatchBufferPointer, totalBytesProgrammed);
    controlSection->crossTileSyncCount = 0u;
    controlSection->finalSyncTileCount = 0u;
    DEBUG_BREAK_IF(totalBytesProgrammed != afterControlSectionOffset);

    if (args.emitSelfCleanup) {
        programSelfCleanupEndSection<GfxFamily>(currentBatchBufferPointer,
                                                totalBytesProgrammed,
                                                finalSyncTileCountField,
                                                gpuAddressOfAllocation + controlSectionOffset,
                                                barrierControlSectionFieldsForCleanupCount,
                                                args);
    }
}

} // namespace WalkerPartition
