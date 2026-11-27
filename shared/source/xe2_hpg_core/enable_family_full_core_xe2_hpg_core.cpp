/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/enable_family_full_core.h"

namespace NEO {

struct Xe2HpgCoreFamily;

static EnableCore<Xe2HpgCoreFamily> enable;
} // namespace NEO
