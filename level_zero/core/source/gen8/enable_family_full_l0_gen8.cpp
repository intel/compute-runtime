/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen8/hw_cmds.h"

namespace NEO {

using Family = Gen8Family;

struct EnableL0Gen8 {
    EnableL0Gen8() {
    }
};

static EnableL0Gen8 enable;
} // namespace NEO
