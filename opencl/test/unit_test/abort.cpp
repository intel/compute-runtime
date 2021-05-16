/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/abort.h"

#include <exception>

namespace NEO {
void abortExecution() {
    throw std::exception();
}
} // namespace NEO
