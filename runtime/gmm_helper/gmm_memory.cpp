/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gmm_memory.h"

namespace OCLRT {
GmmMemory *GmmMemory::create() {
    return new GmmMemory();
}
} // namespace OCLRT
