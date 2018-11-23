/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/queue_helpers.h"

namespace OCLRT {
bool processExtraTokens(Device *&device, Context &context, const cl_queue_properties *property) {
    return false;
}

void CommandQueue::processProperties(const cl_queue_properties *properties) {
}
} // namespace OCLRT
