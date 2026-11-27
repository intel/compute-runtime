/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/enable_family_full_core.h"

namespace NEO {

struct XeHpgCoreFamily;

static EnableCore<XeHpgCoreFamily> enable;
} // namespace NEO
