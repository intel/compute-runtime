/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/test/common/helpers/variable_backup.h"

#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch.h"
#include <level_zero/ze_api.h>

#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct CommandListCreateImmediateHook {
    CommandListCreateImmediateHook() : createImmediateBackup(&NEO::LEO::zeCommandListCreateImmediate, &hookedCreateImmediate) {
        activeHook = this;
    }

    ~CommandListCreateImmediateHook() {
        activeHook = nullptr;
    }

    static ze_result_t ZE_APICALL hookedCreateImmediate(ze_context_handle_t hContext, ze_device_handle_t hDevice, const ze_command_queue_desc_t *desc, ze_command_list_handle_t *phCommandList) {
        activeHook->requestedFlags.push_back(desc->flags);
        if (activeHook->resultToReturn != ZE_RESULT_SUCCESS) {
            return activeHook->resultToReturn;
        }
        if (activeHook->cmdListToReturn != nullptr) {
            *phCommandList = activeHook->cmdListToReturn;
            return ZE_RESULT_SUCCESS;
        }
        return ::zeCommandListCreateImmediate(hContext, hDevice, desc, phCommandList);
    }

    inline static CommandListCreateImmediateHook *activeHook = nullptr;

    VariableBackup<decltype(NEO::LEO::zeCommandListCreateImmediate)> createImmediateBackup;
    std::vector<ze_command_queue_flags_t> requestedFlags;
    ze_result_t resultToReturn = ZE_RESULT_SUCCESS;
    ze_command_list_handle_t cmdListToReturn = nullptr;
};

} // namespace ult
} // namespace LEO
} // namespace NEO
