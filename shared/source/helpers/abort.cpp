/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/abort.h"

#include <cstdlib>

namespace NEO {
void abortExecution() {
    abort();
}
} // namespace NEO
