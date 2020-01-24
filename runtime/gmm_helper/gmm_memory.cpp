/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gmm_memory.h"

namespace NEO {
GmmMemory *GmmMemory::create(GmmClientContext *gmmClientContext) {
    return new GmmMemory(gmmClientContext);
}
} // namespace NEO
