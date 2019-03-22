/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/queue_helpers.h"

namespace NEO {
bool isExtraToken(const cl_queue_properties *property) {
    return false;
}

bool verifyExtraTokens(Device *&device, Context &context, const cl_queue_properties *properties) {
    return true;
}

void CommandQueue::processProperties(const cl_queue_properties *properties) {
}

void getIntelQueueInfo(CommandQueue *queue, cl_command_queue_info paramName, GetInfoHelper &getInfoHelper, cl_int &retVal) {
    retVal = CL_INVALID_VALUE;
}
bool isCommandWithoutKernel(uint32_t commandType) {
    return ((commandType == CL_COMMAND_BARRIER) || (commandType == CL_COMMAND_MARKER) ||
            (commandType == CL_COMMAND_MIGRATE_MEM_OBJECTS) ||
            (commandType == CL_COMMAND_SVM_MAP) ||
            (commandType == CL_COMMAND_SVM_UNMAP) ||
            (commandType == CL_COMMAND_SVM_FREE));
}
} // namespace NEO
