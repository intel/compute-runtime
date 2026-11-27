/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/helpers/l0_populate_factory.h"

namespace NEO {

struct Gen12LpFamily;

static L0::EnableL0<Gen12LpFamily> enable;
} // namespace NEO
