/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/abort.h"

#include <exception>

namespace NEO {
void abortExecution() {
    throw std::exception();
}
} // namespace NEO
