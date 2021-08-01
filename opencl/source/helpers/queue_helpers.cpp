/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/queue_helpers.h"

namespace NEO {
bool isCommandWithoutKernel(uint32_t commandType) {
    return ((commandType == CL_COMMAND_BARRIER) ||
            (commandType == CL_COMMAND_MARKER) ||
            (commandType == CL_COMMAND_MIGRATE_MEM_OBJECTS) ||
            (commandType == CL_COMMAND_SVM_FREE) ||
            (commandType == CL_COMMAND_SVM_MAP) ||
            (commandType == CL_COMMAND_SVM_MIGRATE_MEM) ||
            (commandType == CL_COMMAND_SVM_UNMAP));
}
} // namespace NEO
