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
    Lri64b,
    Sdi,
    Semaphore,
    Walker
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
        case PatchCmdType::Sdi:
            patchSdi(appendCounterValue);
            break;
        case PatchCmdType::Semaphore:
            patchSemaphore(appendCounterValue);
            break;
        case PatchCmdType::Walker:
            patchComputeWalker(appendCounterValue);
            break;
        case PatchCmdType::Lri64b:
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
    const PatchCmdType patchCmdType = PatchCmdType::None;

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
using InOrderPatchCommandsContainer = std::vector<InOrderPatchCommandHelpers::PatchCmd<GfxFamily>>;

} // namespace L0
