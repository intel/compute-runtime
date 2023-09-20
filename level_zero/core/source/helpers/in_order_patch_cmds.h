/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/ptr_math.h"

#include <cstdint>
#include <vector>

namespace L0 {
namespace InOrderPatchCommandTypes {
enum class CmdType {
    Sdi,
    Semaphore
};

template <typename GfxFamily>
struct BaseCmd {
    BaseCmd(void *cmd, uint64_t baseCounterValue, CmdType cmdType) : cmd(cmd), baseCounterValue(baseCounterValue), cmdType(cmdType) {}

    void patch(uint64_t appendCunterValue) {
        if (CmdType::Sdi == cmdType) {
            patchSdi(appendCunterValue);
        } else {
            UNRECOVERABLE_IF(CmdType::Semaphore != cmdType);
            patchSemaphore(appendCunterValue);
        }
    }

    void *cmd = nullptr;
    const uint64_t baseCounterValue = 0;
    const CmdType cmdType;

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

    BaseCmd() = delete;
};

} // namespace InOrderPatchCommandTypes

template <typename GfxFamily>
using InOrderPatchCommandsContainer = std::vector<InOrderPatchCommandTypes::BaseCmd<GfxFamily>>;

} // namespace L0
