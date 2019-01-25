/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/helpers/csr_deps.h"
#include "runtime/helpers/kernel_commands.h"
#include "runtime/helpers/properties_helper.h"
#include "runtime/utilities/tag_allocator.h"

#include <cstdint>
#include <array>
#include <atomic>
#include <vector>

namespace OCLRT {
class CommandStreamReceiver;
class LinearStream;

namespace TimestampPacketSizeControl {
constexpr uint32_t preferedChunkCount = 16u;
}

#pragma pack(1)
class TimestampPacket {
  public:
    TimestampPacket() {
        initialize();
    }

    enum class DataIndex : uint32_t {
        ContextStart = 0,
        GlobalStart,
        ContextEnd,
        GlobalEnd,
        Max
    };

    enum class WriteOperationType : uint32_t {
        BeforeWalker,
        AfterWalker
    };

    bool canBeReleased() const {
        return data[static_cast<uint32_t>(DataIndex::ContextEnd)] != 1 &&
               data[static_cast<uint32_t>(DataIndex::GlobalEnd)] != 1 &&
               implicitDependenciesCount.load() == 0;
    }

    uint64_t pickAddressForDataWrite(DataIndex operationType) const {
        auto index = static_cast<uint32_t>(operationType);
        return reinterpret_cast<uint64_t>(&data[index]);
    }

    uint32_t getData(DataIndex operationType) const {
        return data[static_cast<uint32_t>(operationType)];
    }

    void initialize() {
        for (auto index = 0u; index < data.size(); index++) {
            data[index] = 1;
        }
        implicitDependenciesCount.store(0);
    }

    void incImplicitDependenciesCount() { implicitDependenciesCount++; }
    uint64_t pickImplicitDependenciesCountWriteAddress() const { return reinterpret_cast<uint64_t>(&implicitDependenciesCount); }

  protected:
    std::array<uint32_t, static_cast<uint32_t>(DataIndex::Max) * TimestampPacketSizeControl::preferedChunkCount> data;
    std::atomic<uint32_t> implicitDependenciesCount;
};
#pragma pack()

static_assert(((static_cast<uint32_t>(TimestampPacket::DataIndex::Max) * TimestampPacketSizeControl::preferedChunkCount + 1) * sizeof(uint32_t)) == sizeof(TimestampPacket),
              "This structure is consumed by GPU and has to follow specific restrictions for padding and size");

class TimestampPacketContainer : public NonCopyableOrMovableClass {
  public:
    using Node = TagNode<TimestampPacket>;
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
    static void programSemaphoreWithImplicitDependency(LinearStream &cmdStream, TimestampPacket &timestampPacket) {
        using MI_ATOMIC = typename GfxFamily::MI_ATOMIC;
        auto compareAddress = timestampPacket.pickAddressForDataWrite(TimestampPacket::DataIndex::ContextEnd);
        auto dependenciesCountAddress = timestampPacket.pickImplicitDependenciesCountWriteAddress();

        KernelCommandsHelper<GfxFamily>::programMiSemaphoreWait(cmdStream, compareAddress, 1);

        timestampPacket.incImplicitDependenciesCount();

        KernelCommandsHelper<GfxFamily>::programMiAtomic(cmdStream, dependenciesCountAddress,
                                                         MI_ATOMIC::ATOMIC_OPCODES::ATOMIC_4B_DECREMENT,
                                                         MI_ATOMIC::DATA_SIZE::DATA_SIZE_DWORD);
    }

    template <typename GfxFamily>
    static void programCsrDependencies(LinearStream &cmdStream, const CsrDependencies &csrDependencies) {
        for (auto timestampPacketContainer : csrDependencies) {
            for (auto &node : timestampPacketContainer->peekNodes()) {
                TimestampPacketHelper::programSemaphoreWithImplicitDependency<GfxFamily>(cmdStream, *node->tag);
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
} // namespace OCLRT
