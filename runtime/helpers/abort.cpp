/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/abort.h"

#include <cstdlib>

namespace OCLRT {
void abortExecution() {
    abort();
}
} // namespace OCLRT
