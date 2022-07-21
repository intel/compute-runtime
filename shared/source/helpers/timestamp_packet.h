/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/csr_deps.h"
#include "shared/source/helpers/aux_translation.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/helpers/string.h"
#include "shared/source/utilities/tag_allocator.h"

#include <cstdint>

namespace NEO {
class CommandStreamReceiver;
class LinearStream;

namespace TimestampPacketSizeControl {
constexpr uint32_t preferredPacketCount = 16u;
}

#pragma pack(1)
template <typename TSize>
class TimestampPackets : public TagTypeBase {
  protected:
    struct Packet {
        TSize contextStart = 1u;
        TSize globalStart = 1u;
        TSize contextEnd = 1u;
        TSize globalEnd = 1u;
    };

  public:
    static constexpr AllocationType getAllocationType() {
        return AllocationType::TIMESTAMP_PACKET_TAG_BUFFER;
    }

    static constexpr TagNodeType getTagNodeType() { return TagNodeType::TimestampPacket; }

    static constexpr size_t getSinglePacketSize() { return sizeof(Packet); }

    void initialize() {
        for (auto &packet : packets) {
            packet.contextStart = 1u;
            packet.globalStart = 1u;
            packet.contextEnd = 1u;
            packet.globalEnd = 1u;
        }
    }

    void assignDataToAllTimestamps(uint32_t packetIndex, void *source) {
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

  protected:
    Packet packets[TimestampPacketSizeControl::preferredPacketCount];
};
#pragma pack()

static_assert(((4 * TimestampPacketSizeControl::preferredPacketCount) * sizeof(uint32_t)) == sizeof(TimestampPackets<uint32_t>),
              "This structure is consumed by GPU and has to follow specific restrictions for padding and size");

class TimestampPacketContainer : public NonCopyableClass {
  public:
    TimestampPacketContainer() = default;
    TimestampPacketContainer(TimestampPacketContainer &&) = default;
    TimestampPacketContainer &operator=(TimestampPacketContainer &&) = default;
    MOCKABLE_VIRTUAL ~TimestampPacketContainer();

    const StackVec<TagNodeBase *, 32u> &peekNodes() const { return timestampPacketNodes; }
    void add(TagNodeBase *timestampPacketNode);
    void swapNodes(TimestampPacketContainer &timestampPacketContainer);
    void assignAndIncrementNodesRefCounts(const TimestampPacketContainer &inputTimestampPacketContainer);
    void makeResident(CommandStreamReceiver &commandStreamReceiver);
    void moveNodesToNewContainer(TimestampPacketContainer &timestampPacketContainer);

  protected:
    StackVec<TagNodeBase *, 32u> timestampPacketNodes;
};

struct TimestampPacketDependencies : public NonCopyableClass {
    TimestampPacketContainer cacheFlushNodes;
    TimestampPacketContainer previousEnqueueNodes;
    TimestampPacketContainer barrierNodes;
    TimestampPacketContainer auxToNonAuxNodes;
    TimestampPacketContainer nonAuxToAuxNodes;

    void moveNodesToNewContainer(TimestampPacketContainer &timestampPacketContainer);
};

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
        using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;

        auto compareAddress = getContextEndGpuAddress(timestampPacketNode);

        for (uint32_t packetId = 0; packetId < timestampPacketNode.getPacketsUsed(); packetId++) {
            uint64_t compareOffset = packetId * timestampPacketNode.getSinglePacketSize();
            EncodeSempahore<GfxFamily>::addMiSemaphoreWaitCommand(cmdStream, compareAddress + compareOffset, 1, COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        }
    }

    template <typename GfxFamily>
    static void programCsrDependenciesForTimestampPacketContainer(LinearStream &cmdStream, const CsrDependencies &csrDependencies) {
        for (auto timestampPacketContainer : csrDependencies.timestampPacketContainer) {
            for (auto &node : timestampPacketContainer->peekNodes()) {
                TimestampPacketHelper::programSemaphore<GfxFamily>(cmdStream, *node);
            }
        }
    }

    template <typename GfxFamily>
    static void programCsrDependenciesForForTaskCountContainer(LinearStream &cmdStream, const CsrDependencies &csrDependencies) {
        auto &taskCountContainer = csrDependencies.taskCountContainer;

        for (auto &[taskCountPreviousRootDevice, tagAddressPreviousRootDevice] : taskCountContainer) {
            using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
            using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;

            EncodeSempahore<GfxFamily>::addMiSemaphoreWaitCommand(cmdStream,
                                                                  static_cast<uint64_t>(tagAddressPreviousRootDevice),
                                                                  taskCountPreviousRootDevice,
                                                                  COMPARE_OPERATION::COMPARE_OPERATION_SAD_GREATER_THAN_OR_EQUAL_SDD);
        }
    }

    template <typename GfxFamily, AuxTranslationDirection auxTranslationDirection>
    static void programSemaphoreForAuxTranslation(LinearStream &cmdStream,
                                                  const TimestampPacketDependencies *timestampPacketDependencies,
                                                  const HardwareInfo &hwInfo) {
        auto &container = (auxTranslationDirection == AuxTranslationDirection::AuxToNonAux)
                              ? timestampPacketDependencies->auxToNonAuxNodes
                              : timestampPacketDependencies->nonAuxToAuxNodes;

        // cache flush after NDR, before NonAuxToAux
        if (auxTranslationDirection == AuxTranslationDirection::NonAuxToAux && timestampPacketDependencies->cacheFlushNodes.peekNodes().size() > 0) {
            UNRECOVERABLE_IF(timestampPacketDependencies->cacheFlushNodes.peekNodes().size() != 1);
            auto cacheFlushTimestampPacketGpuAddress = getContextEndGpuAddress(*timestampPacketDependencies->cacheFlushNodes.peekNodes()[0]);

            PipeControlArgs args;
            args.dcFlushEnable = MemorySynchronizationCommands<GfxFamily>::getDcFlushEnable(true, hwInfo);
            MemorySynchronizationCommands<GfxFamily>::addBarrierWithPostSyncOperation(
                cmdStream, PostSyncMode::ImmediateData,
                cacheFlushTimestampPacketGpuAddress, 0, hwInfo, args);
        }

        for (auto &node : container.peekNodes()) {
            TimestampPacketHelper::programSemaphore<GfxFamily>(cmdStream, *node);
        }
    }

    template <typename GfxFamily, AuxTranslationDirection auxTranslationDirection>
    static size_t getRequiredCmdStreamSizeForAuxTranslationNodeDependency(size_t count, const HardwareInfo &hwInfo, bool cacheFlushForBcsRequired) {
        size_t size = count * TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue<GfxFamily>();

        if (auxTranslationDirection == AuxTranslationDirection::NonAuxToAux && cacheFlushForBcsRequired) {
            size += MemorySynchronizationCommands<GfxFamily>::getSizeForBarrierWithPostSyncOperation(hwInfo);
        }

        return size;
    }

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue() {
        return sizeof(typename GfxFamily::MI_SEMAPHORE_WAIT);
    }

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSizeForNodeDependency(TagNodeBase &timestampPacketNode) {
        return (timestampPacketNode.getPacketsUsed() * sizeof(typename GfxFamily::MI_SEMAPHORE_WAIT));
    }

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSize(const CsrDependencies &csrDependencies) {
        size_t totalCommandsSize = 0;
        for (auto timestampPacketContainer : csrDependencies.timestampPacketContainer) {
            for (auto &node : timestampPacketContainer->peekNodes()) {
                totalCommandsSize += getRequiredCmdStreamSizeForNodeDependency<GfxFamily>(*node);
            }
        }

        return totalCommandsSize;
    }

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSizeForTaskCountContainer(const CsrDependencies &csrDependencies) {
        return csrDependencies.taskCountContainer.size() * sizeof(typename GfxFamily::MI_SEMAPHORE_WAIT);
    }
};

} // namespace NEO
