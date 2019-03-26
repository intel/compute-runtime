/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gmm_memory.h"

namespace NEO {
GmmMemory *GmmMemory::create() {
    return new GmmMemory();
}
} // namespace NEO
