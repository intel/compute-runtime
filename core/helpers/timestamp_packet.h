/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/command_container/command_encoder.h"
#include "core/command_stream/csr_deps.h"
#include "core/helpers/aux_translation.h"
#include "core/helpers/non_copyable_or_moveable.h"
#include "core/utilities/tag_allocator.h"
#include "runtime/helpers/properties_helper.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace NEO {
class CommandStreamReceiver;
class LinearStream;

namespace TimestampPacketSizeControl {
constexpr uint32_t preferredPacketCount = 16u;
}

#pragma pack(1)
struct TimestampPacketStorage {
    struct Packet {
        uint32_t contextStart = 1u;
        uint32_t globalStart = 1u;
        uint32_t contextEnd = 1u;
        uint32_t globalEnd = 1u;
    };

    enum class WriteOperationType : uint32_t {
        BeforeWalker,
        AfterWalker
    };

    static GraphicsAllocation::AllocationType getAllocationType() {
        return GraphicsAllocation::AllocationType::TIMESTAMP_PACKET_TAG_BUFFER;
    }

    bool isCompleted() const {
        for (uint32_t i = 0; i < packetsUsed; i++) {
            if ((packets[i].contextEnd & 1) || (packets[i].globalEnd & 1)) {
                return false;
            }
        }
        return implicitDependenciesCount.load() == 0;
    }

    void initialize() {
        for (auto &packet : packets) {
            packet.contextStart = 1u;
            packet.globalStart = 1u;
            packet.contextEnd = 1u;
            packet.globalEnd = 1u;
        }
        implicitDependenciesCount.store(0);
        packetsUsed = 1;
    }

    void incImplicitDependenciesCount() { implicitDependenciesCount++; }

    Packet packets[TimestampPacketSizeControl::preferredPacketCount];
    std::atomic<uint32_t> implicitDependenciesCount{0u};
    uint32_t packetsUsed = 1;
};
#pragma pack()

static_assert(((4 * TimestampPacketSizeControl::preferredPacketCount + 2) * sizeof(uint32_t)) == sizeof(TimestampPacketStorage),
              "This structure is consumed by GPU and has to follow specific restrictions for padding and size");

class TimestampPacketContainer : public NonCopyableClass {
  public:
    using Node = TagNode<TimestampPacketStorage>;
    TimestampPacketContainer() = default;
    TimestampPacketContainer(TimestampPacketContainer &&) = default;
    TimestampPacketContainer &operator=(TimestampPacketContainer &&) = default;
    MOCKABLE_VIRTUAL ~TimestampPacketContainer();

    const std::vector<Node *> &peekNodes() const { return timestampPacketNodes; }
    void add(Node *timestampPacketNode);
    void swapNodes(TimestampPacketContainer &timestampPacketContainer);
    void assignAndIncrementNodesRefCounts(const TimestampPacketContainer &inputTimestampPacketContainer);
    void resolveDependencies(bool clearAllDependencies);
    void makeResident(CommandStreamReceiver &commandStreamReceiver);
    bool isCompleted() const;

  protected:
    std::vector<Node *> timestampPacketNodes;
};

struct TimestampPacketDependencies : public NonCopyableClass {
    TimestampPacketContainer previousEnqueueNodes;
    TimestampPacketContainer barrierNodes;
    TimestampPacketContainer auxToNonAuxNodes;
    TimestampPacketContainer nonAuxToAuxNodes;
};

struct TimestampPacketHelper {
    template <typename GfxFamily>
    static void programSemaphoreWithImplicitDependency(LinearStream &cmdStream, TagNode<TimestampPacketStorage> &timestampPacketNode) {
        using MI_ATOMIC = typename GfxFamily::MI_ATOMIC;
        using COMPARE_OPERATION = typename GfxFamily::MI_SEMAPHORE_WAIT::COMPARE_OPERATION;
        using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;

        auto compareAddress = timestampPacketNode.getGpuAddress() + offsetof(TimestampPacketStorage, packets[0].contextEnd);
        auto dependenciesCountAddress = timestampPacketNode.getGpuAddress() + offsetof(TimestampPacketStorage, implicitDependenciesCount);

        for (uint32_t packetId = 0; packetId < timestampPacketNode.tagForCpuAccess->packetsUsed; packetId++) {
            uint64_t compareOffset = packetId * sizeof(TimestampPacketStorage::Packet);
            auto miSemaphoreCmd = cmdStream.getSpaceForCmd<MI_SEMAPHORE_WAIT>();
            EncodeSempahore<GfxFamily>::programMiSemaphoreWait(miSemaphoreCmd, compareAddress + compareOffset, 1, COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD);
        }

        timestampPacketNode.tagForCpuAccess->incImplicitDependenciesCount();

        auto miAtomic = cmdStream.getSpaceForCmd<MI_ATOMIC>();
        EncodeAtomic<GfxFamily>::programMiAtomic(miAtomic, dependenciesCountAddress,
                                                 MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_DECREMENT,
                                                 MI_ATOMIC::DATA_SIZE::DATA_SIZE_DWORD);
    }

    template <typename GfxFamily>
    static void programCsrDependencies(LinearStream &cmdStream, const CsrDependencies &csrDependencies) {
        for (auto timestampPacketContainer : csrDependencies) {
            for (auto &node : timestampPacketContainer->peekNodes()) {
                TimestampPacketHelper::programSemaphoreWithImplicitDependency<GfxFamily>(cmdStream, *node);
            }
        }
    }

    template <typename GfxFamily, AuxTranslationDirection auxTranslationDirection>
    static void programSemaphoreWithImplicitDependencyForAuxTranslation(LinearStream &cmdStream,
                                                                        const TimestampPacketDependencies *timestampPacketDependencies) {
        auto &container = (auxTranslationDirection == AuxTranslationDirection::AuxToNonAux)
                              ? timestampPacketDependencies->auxToNonAuxNodes
                              : timestampPacketDependencies->nonAuxToAuxNodes;

        for (auto &node : container.peekNodes()) {
            TimestampPacketHelper::programSemaphoreWithImplicitDependency<GfxFamily>(cmdStream, *node);
        }
    }

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSizeForAuxTranslationNodeDependency(const MemObjsForAuxTranslation *memObjsForAuxTranslation) {
        return memObjsForAuxTranslation->size() * TimestampPacketHelper::getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue<GfxFamily>();
    }

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSizeForNodeDependencyWithBlitEnqueue() {
        return sizeof(typename GfxFamily::MI_SEMAPHORE_WAIT) + sizeof(typename GfxFamily::MI_ATOMIC);
    }

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSizeForNodeDependency(TagNode<TimestampPacketStorage> &timestampPacketNode) {
        size_t totalMiSemaphoreWaitSize = timestampPacketNode.tagForCpuAccess->packetsUsed * sizeof(typename GfxFamily::MI_SEMAPHORE_WAIT);

        return totalMiSemaphoreWaitSize + sizeof(typename GfxFamily::MI_ATOMIC);
    }

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSize(const CsrDependencies &csrDependencies) {
        size_t totalCommandsSize = 0;
        for (auto timestampPacketContainer : csrDependencies) {
            for (auto &node : timestampPacketContainer->peekNodes()) {
                totalCommandsSize += getRequiredCmdStreamSizeForNodeDependency<GfxFamily>(*node);
            }
        }

        return totalCommandsSize;
    }
};

} // namespace NEO
