/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/abort.h"

#include <exception>

namespace OCLRT {
void abortExecution() {
    throw std::exception();
}
} // namespace OCLRT
