/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/helpers/csr_deps.h"
#include "runtime/helpers/hardware_commands_helper.h"
#include "runtime/helpers/properties_helper.h"
#include "runtime/utilities/tag_allocator.h"

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

    bool canBeReleased() const {
        return packets[0].contextEnd != 1 &&
               packets[0].globalEnd != 1 &&
               implicitDependenciesCount.load() == 0;
    }

    void initialize() {
        for (auto &packet : packets) {
            packet.contextStart = 1u;
            packet.globalStart = 1u;
            packet.contextEnd = 1u;
            packet.globalEnd = 1u;
        }
        implicitDependenciesCount.store(0);
    }

    void incImplicitDependenciesCount() { implicitDependenciesCount++; }

    Packet packets[TimestampPacketSizeControl::preferredPacketCount];
    std::atomic<uint32_t> implicitDependenciesCount{0u};
};
#pragma pack()

static_assert(((4 * TimestampPacketSizeControl::preferredPacketCount + 1) * sizeof(uint32_t)) == sizeof(TimestampPacketStorage),
              "This structure is consumed by GPU and has to follow specific restrictions for padding and size");

class TimestampPacketContainer : public NonCopyableOrMovableClass {
  public:
    using Node = TagNode<TimestampPacketStorage>;
    TimestampPacketContainer() = default;
    MOCKABLE_VIRTUAL ~TimestampPacketContainer();

    const std::vector<Node *> &peekNodes() const { return timestampPacketNodes; }
    void add(Node *timestampPacketNode);
    void swapNodes(TimestampPacketContainer &timestampPacketContainer);
    void assignAndIncrementNodesRefCounts(const TimestampPacketContainer &inputTimestampPacketContainer);
    void resolveDependencies(bool clearAllDependencies);
    void makeResident(CommandStreamReceiver &commandStreamReceiver);

  protected:
    std::vector<Node *> timestampPacketNodes;
};

struct TimestampPacketHelper {
    template <typename GfxFamily>
    static void programSemaphoreWithImplicitDependency(LinearStream &cmdStream, TagNode<TimestampPacketStorage> &timestampPacketNode) {
        using MI_ATOMIC = typename GfxFamily::MI_ATOMIC;
        auto compareAddress = timestampPacketNode.getGpuAddress() + offsetof(TimestampPacketStorage, packets[0].contextEnd);
        auto dependenciesCountAddress = timestampPacketNode.getGpuAddress() + offsetof(TimestampPacketStorage, implicitDependenciesCount);

        HardwareCommandsHelper<GfxFamily>::programMiSemaphoreWait(cmdStream, compareAddress, 1);

        timestampPacketNode.tagForCpuAccess->incImplicitDependenciesCount();

        HardwareCommandsHelper<GfxFamily>::programMiAtomic(cmdStream, dependenciesCountAddress,
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

    template <typename GfxFamily>
    static size_t getRequiredCmdStreamSize(const CsrDependencies &csrDependencies) {
        size_t totalNodesCount = 0;
        for (auto timestampPacketContainer : csrDependencies) {
            totalNodesCount += timestampPacketContainer->peekNodes().size();
        }

        return totalNodesCount * (sizeof(typename GfxFamily::MI_SEMAPHORE_WAIT) + sizeof(typename GfxFamily::MI_ATOMIC));
    }
};
} // namespace NEO
