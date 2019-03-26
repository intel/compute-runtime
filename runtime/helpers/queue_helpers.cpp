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
} // namespace NEO
