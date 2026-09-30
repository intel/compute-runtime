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

struct CommandListCreateHook {
    CommandListCreateHook() : createBackup(&NEO::LEO::zeCommandListCreate, &hookedCreate) {
        activeHook = this;
    }

    ~CommandListCreateHook() {
        activeHook = nullptr;
    }

    static ze_result_t ZE_APICALL hookedCreate(ze_context_handle_t hContext, ze_device_handle_t hDevice, const ze_command_list_desc_t *desc, ze_command_list_handle_t *phCommandList) {
        activeHook->requestedDescs.push_back(*desc);
        if (activeHook->resultToReturn != ZE_RESULT_SUCCESS) {
            return activeHook->resultToReturn;
        }
        if (activeHook->cmdListToReturn != nullptr) {
            *phCommandList = activeHook->cmdListToReturn;
            return ZE_RESULT_SUCCESS;
        }
        return ::zeCommandListCreate(hContext, hDevice, desc, phCommandList);
    }

    inline static CommandListCreateHook *activeHook = nullptr;

    VariableBackup<decltype(NEO::LEO::zeCommandListCreate)> createBackup;
    std::vector<ze_command_list_desc_t> requestedDescs;
    ze_result_t resultToReturn = ZE_RESULT_SUCCESS;
    ze_command_list_handle_t cmdListToReturn = nullptr;
};

} // namespace ult
} // namespace LEO
} // namespace NEO
