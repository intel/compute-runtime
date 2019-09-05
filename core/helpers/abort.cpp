/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/abort.h"

#include <cstdlib>

namespace NEO {
void abortExecution() {
    abort();
}
} // namespace NEO
