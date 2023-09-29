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
#include <vector>

namespace NEO {
class GraphicsAllocation;
class MemoryManager;
} // namespace NEO

namespace L0 {

struct InOrderExecInfo : public NEO::NonCopyableClass {
    ~InOrderExecInfo();

    InOrderExecInfo() = delete;

    InOrderExecInfo(NEO::GraphicsAllocation &inOrderDependencyCounterAllocation, NEO::MemoryManager &memoryManager, bool isRegularCmdList);

    NEO::GraphicsAllocation &inOrderDependencyCounterAllocation;
    NEO::MemoryManager &memoryManager;
    uint64_t inOrderDependencyCounter = 0;
    uint64_t regularCmdListSubmissionCounter = 0;
    bool isRegularCmdList = false;
};

namespace InOrderPatchCommandHelpers {
inline uint64_t getAppendCounterValue(const InOrderExecInfo &inOrderExecInfo) {
    if (inOrderExecInfo.isRegularCmdList && inOrderExecInfo.regularCmdListSubmissionCounter > 1) {
        return inOrderExecInfo.inOrderDependencyCounter * (inOrderExecInfo.regularCmdListSubmissionCounter - 1);
    }

    return 0;
}

enum class PatchCmdType {
    None,
    Sdi,
    Semaphore,
    Walker
};

template <typename GfxFamily>
struct PatchCmd {
    PatchCmd(void *cmd, uint64_t baseCounterValue, PatchCmdType patchCmdType) : cmd(cmd), baseCounterValue(baseCounterValue), patchCmdType(patchCmdType) {}

    void patch(uint64_t appendCunterValue) {
        switch (patchCmdType) {
        case PatchCmdType::Sdi:
            patchSdi(appendCunterValue);
            break;
        case PatchCmdType::Semaphore:
            patchSemaphore(appendCunterValue);
            break;
        case PatchCmdType::Walker:
            patchComputeWalker(appendCunterValue);
            break;
        default:
            UNRECOVERABLE_IF(true);
            break;
        }
    }

    void *cmd = nullptr;
    const uint64_t baseCounterValue = 0;
    const PatchCmdType patchCmdType = PatchCmdType::None;

  protected:
    void patchSdi(uint64_t appendCunterValue) {
        auto sdiCmd = reinterpret_cast<typename GfxFamily::MI_STORE_DATA_IMM *>(cmd);
        sdiCmd->setDataDword0(getLowPart(baseCounterValue + appendCunterValue));
        sdiCmd->setDataDword1(getHighPart(baseCounterValue + appendCunterValue));
    }

    void patchSemaphore(uint64_t appendCunterValue) {
        auto semaphoreCmd = reinterpret_cast<typename GfxFamily::MI_SEMAPHORE_WAIT *>(cmd);
        semaphoreCmd->setSemaphoreDataDword(static_cast<uint32_t>(baseCounterValue + appendCunterValue));
    }

    void patchComputeWalker(uint64_t appendCunterValue) {
        if constexpr (GfxFamily::walkerPostSyncSupport) {
            auto walkerCmd = reinterpret_cast<typename GfxFamily::COMPUTE_WALKER *>(cmd);
            auto &postSync = walkerCmd->getPostSync();
            postSync.setImmediateData(baseCounterValue + appendCunterValue);
        } else {
            UNRECOVERABLE_IF(true);
        }
    }

    PatchCmd() = delete;
};

} // namespace InOrderPatchCommandHelpers

template <typename GfxFamily>
using InOrderPatchCommandsContainer = std::vector<InOrderPatchCommandHelpers::PatchCmd<GfxFamily>>;

} // namespace L0
