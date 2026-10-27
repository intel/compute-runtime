/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/api/opencl/source/context/leo_context.h"
#include "level_zero/core/source/cmdlist/cmdlist.h"
#include <level_zero/ze_api.h>

#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct CommandListCreateImmediateHook {
    CommandListCreateImmediateHook() : createImmediateBackup(&NEO::LEO::InternalCmdListFunctions::createImmediate, &hookedCreateImmediate) {
        activeHook = this;
    }

    ~CommandListCreateImmediateHook() {
        activeHook = nullptr;
    }

    static L0::CommandList *hookedCreateImmediate(L0::Device *device, const ze_command_queue_desc_t *desc, bool internalUsage, NEO::EngineGroupType engineGroupType, ze_result_t &resultValue) {
        activeHook->requestedFlags.push_back(desc->flags);
        activeHook->requestedInternalUsage.push_back(internalUsage);
        activeHook->requestedEngineGroupTypes.push_back(engineGroupType);
        if (activeHook->resultToReturn != ZE_RESULT_SUCCESS) {
            resultValue = activeHook->resultToReturn;
            return nullptr;
        }
        if (activeHook->cmdListToReturn != nullptr) {
            resultValue = ZE_RESULT_SUCCESS;
            return L0::CommandList::fromHandle(activeHook->cmdListToReturn);
        }
        return L0::CommandList::createImmediate(device, desc, internalUsage, engineGroupType, resultValue);
    }

    inline static CommandListCreateImmediateHook *activeHook = nullptr;

    VariableBackup<NEO::LEO::InternalCmdListFunctions::CreateImmediateFunc> createImmediateBackup;
    std::vector<ze_command_queue_flags_t> requestedFlags;
    std::vector<bool> requestedInternalUsage;
    std::vector<NEO::EngineGroupType> requestedEngineGroupTypes;
    ze_result_t resultToReturn = ZE_RESULT_SUCCESS;
    ze_command_list_handle_t cmdListToReturn = nullptr;
};

} // namespace ult
} // namespace LEO
} // namespace NEO
