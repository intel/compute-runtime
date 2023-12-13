/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/ptr_math.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace NEO {
class GraphicsAllocation;
class MemoryManager;

class InOrderExecInfo : public NEO::NonCopyableClass {
  public:
    ~InOrderExecInfo();

    InOrderExecInfo() = delete;

    InOrderExecInfo(NEO::GraphicsAllocation &deviceCounterAllocation, NEO::GraphicsAllocation *hostCounterAllocation, NEO::MemoryManager &memoryManager, uint32_t partitionCount, bool regularCmdList, bool atomicDeviceSignalling);

    NEO::GraphicsAllocation &getDeviceCounterAllocation() const { return deviceCounterAllocation; }
    NEO::GraphicsAllocation *getHostCounterAllocation() const { return hostCounterAllocation; }
    uint64_t *getBaseHostAddress() const { return hostAddress; }

    uint64_t getCounterValue() const { return counterValue; }
    void addCounterValue(uint64_t addValue) { counterValue += addValue; }
    void resetCounterValue() { counterValue = 0; }

    uint64_t getRegularCmdListSubmissionCounter() const { return regularCmdListSubmissionCounter; }
    void addRegularCmdListSubmissionCounter(uint64_t addValue) { regularCmdListSubmissionCounter += addValue; }

    bool isRegularCmdList() const { return regularCmdList; }
    bool isHostStorageDuplicated() const { return duplicatedHostStorage; }
    bool isAtomicDeviceSignalling() const { return atomicDeviceSignalling; }

    uint32_t getNumDevicePartitionsToWait() const { return numDevicePartitionsToWait; }
    uint32_t getNumHostPartitionsToWait() const { return numHostPartitionsToWait; }

    void addAllocationOffset(uint32_t addValue) { allocationOffset += addValue; }
    uint32_t getAllocationOffset() const { return allocationOffset; }

    void reset();

  protected:
    NEO::GraphicsAllocation &deviceCounterAllocation;
    NEO::MemoryManager &memoryManager;
    NEO::GraphicsAllocation *hostCounterAllocation = nullptr;
    uint64_t counterValue = 0;
    uint64_t regularCmdListSubmissionCounter = 0;
    uint64_t *hostAddress = nullptr;
    uint32_t numDevicePartitionsToWait = 0;
    uint32_t numHostPartitionsToWait = 0;
    uint32_t allocationOffset = 0;
    bool regularCmdList = false;
    bool duplicatedHostStorage = false;
    bool atomicDeviceSignalling = false;
};

namespace InOrderPatchCommandHelpers {
inline uint64_t getAppendCounterValue(const InOrderExecInfo &inOrderExecInfo) {
    if (inOrderExecInfo.isRegularCmdList() && inOrderExecInfo.getRegularCmdListSubmissionCounter() > 1) {
        return inOrderExecInfo.getCounterValue() * (inOrderExecInfo.getRegularCmdListSubmissionCounter() - 1);
    }

    return 0;
}

enum class PatchCmdType {
    none,
    lri64b,
    sdi,
    semaphore,
    walker
};

template <typename GfxFamily>
struct PatchCmd {
    PatchCmd(std::shared_ptr<InOrderExecInfo> *inOrderExecInfo, void *cmd1, void *cmd2, uint64_t baseCounterValue, PatchCmdType patchCmdType)
        : cmd1(cmd1), cmd2(cmd2), baseCounterValue(baseCounterValue), patchCmdType(patchCmdType) {
        if (inOrderExecInfo) {
            this->inOrderExecInfo = *inOrderExecInfo;
        }
    }

    void patch(uint64_t appendCounterValue) {
        switch (patchCmdType) {
        case PatchCmdType::sdi:
            patchSdi(appendCounterValue);
            break;
        case PatchCmdType::semaphore:
            patchSemaphore(appendCounterValue);
            break;
        case PatchCmdType::walker:
            patchComputeWalker(appendCounterValue);
            break;
        case PatchCmdType::lri64b:
            patchLri64b(appendCounterValue);
            break;
        default:
            UNRECOVERABLE_IF(true);
            break;
        }
    }

    bool isExternalDependency() const { return inOrderExecInfo.get(); }

    std::shared_ptr<InOrderExecInfo> inOrderExecInfo;
    void *cmd1 = nullptr;
    void *cmd2 = nullptr;
    const uint64_t baseCounterValue = 0;
    const PatchCmdType patchCmdType = PatchCmdType::none;

  protected:
    void patchSdi(uint64_t appendCounterValue) {
        auto sdiCmd = reinterpret_cast<typename GfxFamily::MI_STORE_DATA_IMM *>(cmd1);
        sdiCmd->setDataDword0(getLowPart(baseCounterValue + appendCounterValue));
        sdiCmd->setDataDword1(getHighPart(baseCounterValue + appendCounterValue));
    }

    void patchSemaphore(uint64_t appendCounterValue) {
        if (isExternalDependency()) {
            appendCounterValue = InOrderPatchCommandHelpers::getAppendCounterValue(*inOrderExecInfo);
            if (appendCounterValue == 0) {
                return;
            }
        }

        auto semaphoreCmd = reinterpret_cast<typename GfxFamily::MI_SEMAPHORE_WAIT *>(cmd1);
        semaphoreCmd->setSemaphoreDataDword(static_cast<uint32_t>(baseCounterValue + appendCounterValue));
    }

    void patchComputeWalker(uint64_t appendCounterValue) {
        if constexpr (GfxFamily::walkerPostSyncSupport) {
            auto walkerCmd = reinterpret_cast<typename GfxFamily::COMPUTE_WALKER *>(cmd1);
            auto &postSync = walkerCmd->getPostSync();
            postSync.setImmediateData(baseCounterValue + appendCounterValue);
        } else {
            UNRECOVERABLE_IF(true);
        }
    }

    void patchLri64b(uint64_t appendCounterValue) {
        if (isExternalDependency()) {
            appendCounterValue = InOrderPatchCommandHelpers::getAppendCounterValue(*inOrderExecInfo);
            if (appendCounterValue == 0) {
                return;
            }
        }

        const uint64_t counterValue = baseCounterValue + appendCounterValue;

        auto lri1 = reinterpret_cast<typename GfxFamily::MI_LOAD_REGISTER_IMM *>(cmd1);
        lri1->setDataDword(getLowPart(counterValue));

        auto lri2 = reinterpret_cast<typename GfxFamily::MI_LOAD_REGISTER_IMM *>(cmd2);
        lri2->setDataDword(getHighPart(counterValue));
    }

    PatchCmd() = delete;
};

} // namespace InOrderPatchCommandHelpers

template <typename GfxFamily>
using InOrderPatchCommandsContainer = std::vector<NEO::InOrderPatchCommandHelpers::PatchCmd<GfxFamily>>;

} // namespace NEO
