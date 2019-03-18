/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/queue_helpers.h"

namespace OCLRT {
bool isExtraToken(const cl_queue_properties *property) {
    return false;
}

bool verifyExtraTokens(Device *&device, Context &context, const cl_queue_properties *properties) {
    return true;
}

void CommandQueue::processProperties(const cl_queue_properties *properties) {
}
} // namespace OCLRT
