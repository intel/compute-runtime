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
    uint64_t regularCmdListSubmissionCounter = 0;
    bool isRegularCmdList = false;
};

namespace InOrderPatchCommandTypes {
enum class CmdType {
    None,
    Sdi,
    Semaphore,
    Walker
};

template <typename GfxFamily>
struct BaseCmd {
    BaseCmd(void *cmd, uint64_t baseCounterValue, CmdType cmdType) : cmd(cmd), baseCounterValue(baseCounterValue), cmdType(cmdType) {}

    void patch(uint64_t appendCunterValue) {
        switch (cmdType) {
        case CmdType::Sdi:
            patchSdi(appendCunterValue);
            break;
        case CmdType::Semaphore:
            patchSemaphore(appendCunterValue);
            break;
        case CmdType::Walker:
            patchComputeWalker(appendCunterValue);
            break;
        default:
            UNRECOVERABLE_IF(true);
            break;
        }
    }

    void *cmd = nullptr;
    const uint64_t baseCounterValue = 0;
    const CmdType cmdType = CmdType::None;

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

    BaseCmd() = delete;
};

} // namespace InOrderPatchCommandTypes

template <typename GfxFamily>
using InOrderPatchCommandsContainer = std::vector<InOrderPatchCommandTypes::BaseCmd<GfxFamily>>;

} // namespace L0
