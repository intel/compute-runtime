/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/csr_deps.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/aux_translation.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/string.h"
#include "shared/source/helpers/timestamp_packet_constants.h"
#include "shared/source/helpers/timestamp_packet_container.h"
#include "shared/source/utilities/tag_allocator.h"

#include <cstdint>

namespace NEO {
class CommandStreamReceiver;
class LinearStream;

#pragma pack(1)
template <typename TSize, uint32_t packetCount>
class TimestampPackets : public TagTypeBase {
  public:
    using ValueT = TSize;

    static constexpr AllocationType getAllocationType() {
        return AllocationType::timestampPacketTagBuffer;
    }

    static constexpr TagNodeType getTagNodeType() { return TagNodeType::timestampPacket; }

    static constexpr size_t getSinglePacketSize() { return sizeof(Packet); }

    void initialize(TSize initValue) {
        for (auto &packet : packets) {
            packet.contextStart = initValue;
            packet.globalStart = initValue;
            packet.contextEnd = initValue;
            packet.globalEnd = initValue;
        }
    }

    void assignDataToAllTimestamps(uint32_t packetIndex, const void *source) {
        memcpy_s(&packets[packetIndex], sizeof(Packet), source, sizeof(Packet));
    }

    static constexpr size_t getGlobalStartOffset() { return offsetof(Packet, globalStart); }
    static constexpr size_t getContextStartOffset() { return offsetof(Packet, contextStart); }
    static constexpr size_t getContextEndOffset() { return offsetof(Packet, contextEnd); }
    static constexpr size_t getGlobalEndOffset() { return offsetof(Packet, globalEnd); }

    uint64_t getContextStartValue(uint32_t packetIndex) const { return static_cast<uint64_t>(packets[packetIndex].contextStart); }
    uint64_t getGlobalStartValue(uint32_t packetIndex) const { return static_cast<uint64_t>(packets[packetIndex].globalStart); }
    uint64_t getContextEndValue(uint32_t packetIndex) const { return static_cast<uint64_t>(packets[packetIndex].contextEnd); }
    uint64_t getGlobalEndValue(uint32_t packetIndex) const { return static_cast<uint64_t>(packets[packetIndex].globalEnd); }

    void const *getContextEndAddress(uint32_t packetIndex) const { return static_cast<void const *>(&packets[packetIndex].contextEnd); }
    void const *getContextStartAddress(uint32_t packetIndex) const { return static_cast<void const *>(&packets[packetIndex].contextStart); }

    uint32_t getPacketCount() const {
        return packetCount;
    }

  protected:
    struct alignas(1) Packet {
        TSize contextStart = TimestampPacketConstants::initValue;
        TSize globalStart = TimestampPacketConstants::initValue;
        TSize contextEnd = TimestampPacketConstants::initValue;
        TSize globalEnd = TimestampPacketConstants::initValue;
    };

    Packet packets[packetCount];
};
#pragma pack()

static_assert(((4 * TimestampPacketConstants::preferredPacketCount) * sizeof(uint32_t)) == sizeof(TimestampPackets<uint32_t, TimestampPacketConstants::preferredPacketCount>),
              "This structure is consumed by GPU and has to follow specific restrictions for padding and size");

struct TimestampPacketHelper {
    static uint64_t getContextEndGpuAddress(const TagNodeBase &timestampPacketNode) {
        return timestampPacketNode.getGpuAddress() + timestampPacketNode.getContextEndOffset();
    }
    static uint64_t getContextStartGpuAddress(const TagNodeBase &timestampPacketNode) {
        return timestampPacketNode.getGpuAddress() + timestampPacketNode.getContextStartOffset();
    }
    static uint64_t getGlobalEndGpuAddress(const TagNodeBase &timestampPacketNode) {
        return timestampPacketNode.getGpuAddress() + timestampPacketNode.getGlobalEndOffset();
    }
    static uint64_t getGlobalStartGpuAddress(const TagNodeBase &timestampPacketNode) {
        return timestampPacketNode.getGpuAddress() + timestampPacketNode.getGlobalStartOffset();
    }

    template <typename GfxFamily>
    static void programSemaphore(LinearStream &cmdStream, TagNodeBase &timestampPacketNode) {
        using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

        if (debugManager.flags.PrintTimestampPacketUsage.get() == 1) {
            printf("\nPID: %u, TSP used for Semaphore: 0x%" PRIX64 ", cmdBuffer pos: 0x%" PRIX64, SysCalls::getProcessId(), timestampPacketNode.getGpuAddress(), cmdStream.getCurrentGpuAddressPosition());
        }

        auto compareAddress = getContextEndGpuAddress(timestampPacketNode);

        for (uint32_t packetId = 0; packetId < timestampPacketNode.getPacketsUsed(); packetId++) {
            uint64_t compareOffset = packetId * timestampPacketNode.getSinglePacketSize();
            EncodeSemaphore<GfxFamily>::addMiSemaphoreWaitCommand(cmdStream, compareAddress + compareOffset, TimestampPacketConstants::initValue, COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, false, false, false, false, nullptr);
        }
    }

    template <typename GfxFamily>
    static void programConditionalBbStartForRelaxedOrdering(LinearStream &cmdStream, TagNodeBase &timestampPacketNode, bool isBcs) {
        auto compareAddress = getContextEndGpuAddress(timestampPacketNode);

        for (uint32_t packetId = 0; packetId < timestampPacketNode.getPacketsUsed(); packetId++) {
            uint64_t compareOffset = packetId * timestampPacketNode.getSinglePacketSize();

            EncodeBatchBufferStartOrEnd<GfxFamily>::programConditionalDataMemBatchBufferStart(cmdStream, 0, compareAddress + compareOffset, TimestampPacketConstants::initValue,
                                                                                              NEO::CompareOperation::equal, true, false, isBcs);
        }
    }

    template <typename GfxFamily>
    static void programCsrDependenciesForTimestampPacketContainer(LinearStream &cmdStream, const CsrDependencies &csrDependencies, bool relaxedOrderingEnabled, bool isBcs) {
        for (auto timestampPacketContainer : csrDependencies.timestampPacketContainer) {
            for (auto &node : timestampPacketContainer->peekNodes()) {
                if (relaxedOrderingEnabled) {
                    TimestampPacketHelper::programConditionalBbStartForRelaxedOrdering<GfxFamily>(cmdStream, *node, isBcs);
                } else {
                    TimestampPacketHelper::programSemaphore<GfxFamily>(cmdStream, *node);
                }
            }
        }
    }

    template <typename GfxFamily>
    static void nonStallingContextEndNodeSignal(LinearStream &cmdStream, const TagNodeBase &timestampPacketNode, bool multiTileOperation) {
        uint64_t contextEndAddress = getContextEndGpuAddress(timestampPacketNode);

        NEO::EncodeStoreMemory<GfxFamily>::programStoreDataImm(cmdStream, contextEndAddress, 0, 0, false, multiTileOperation,
                                                               nullptr);
    }

    template <typename GfxFamily>
    static void programCsrDependenciesForForMultiRootDeviceSyncContainer(LinearStream &cmdStream, const CsrDependencies &csrDependencies) {
        for (auto timestampPacketContainer : csrDependencies.multiRootTimeStampSyncContainer) {
            for (auto &node : timestampPacketContainer->peekNodes()) {
                TimestampPacketHelper::programSemaphore<GfxFamily>(cmdStream, *node);
            }
        }
    }

    template <typename GfxFamily, AuxTranslationDirection auxTranslationDirection>
    static void programSemaphoreForAuxTranslation(LinearStream &cmdStream,
                                                  const TimestampPacketDependencies *timestampPacketDependencies,
                                                  const RootDeviceEnvironment &rootDeviceEnvironment) {
        auto &container = (auxTranslationDirection == AuxTranslationDirection::auxToNonAux)
                              ? timestampPacketDependencies->auxToNonAuxNodes
                              : timestampPacketDependencies->nonAuxToAuxNodes;

        // cache flush after NDR, before NonAuxToAux
        if (auxTranslationDirection == AuxTranslationDirection::nonAuxToAux && timestampPacketDependencies->cacheFlushNodes.peekNodes().size() > 0) {
            UNRECOVERABLE_IF(timestampPacketDependencies->cacheFlushNodes.peekNodes().size() != 1);
            auto cacheFlushTimestampPacketGpuAddress = getContextEndGpuAddress(*timestampPacketDependencies->cacheFlushNodes.peekNodes()[0]);

            PipeControlArgs args;
            args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, rootDeviceEnvironment);
            MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
                cmdStream, PostSyncMode::immediateData,
                cacheFlushTimestampPacketGpuAddress, 0, rootDeviceEnvironment, args);
        }

        for (auto &node : container.peekNodes()) {
            TimestampPacketHelper::programSemaphore<GfxFamily>(cmdStream, *node);
        }
    }

    template <typename GfxFamily, AuxTranslationDirection auxTranslationDirection>
    static size_t getRequiredCmdStreamSizeForAuxTranslationNodeDependency(size_t count, const RootDeviceEnvironment &rootDeviceEnvironment, bool cacheFlushForBcsRequired) {
        size_t size = count * TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue<GfxFamily>();

        if (auxTranslationDirection == AuxTranslationDirection::nonAuxToAux && cacheFlushForBcsRequired) {
            size += MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(rootDeviceEnvironment, NEO::PostSyncMode::immediateData);
        }

        return size;
    }

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue() {
        return NEO::EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait();
    }

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSizeForSemaphoreNodeDependency(TagNodeBase &timestampPacketNode) {
        return (timestampPacketNode.getPacketsUsed() * NEO::EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait());
    }

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSizeForRelaxedOrderingNodeDependency(TagNodeBase &timestampPacketNode) {
        return (timestampPacketNode.getPacketsUsed() * EncodeBatchBufferStartOrEnd<GfxFamily>::getCmdSizeConditionalDataMemBatchBufferStart(false));
    }

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSize(const CsrDependencies &csrDependencies, bool relaxedOrderingEnabled) {
        size_t totalCommandsSize = 0;
        for (auto timestampPacketContainer : csrDependencies.timestampPacketContainer) {
            for (auto &node : timestampPacketContainer->peekNodes()) {
                if (relaxedOrderingEnabled) {
                    totalCommandsSize += getRequiredCmdStreamSizeForRelaxedOrderingNodeDependency<GfxFamily>(*node);
                } else {
                    totalCommandsSize += getRequiredCmdStreamSizeForSemaphoreNodeDependency<GfxFamily>(*node);
                }
            }
        }

        return totalCommandsSize;
    }

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSizeForMultiRootDeviceSyncNodesContainer(const CsrDependencies &csrDependencies) {
        return csrDependencies.multiRootTimeStampSyncContainer.size() * NEO::EncodeSemaphore<GfxFamily>::getSizeMiSemaphoreWait();
    }
};

} // namespace NEO
