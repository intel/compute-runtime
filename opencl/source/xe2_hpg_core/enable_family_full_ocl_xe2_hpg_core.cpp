/*
 * Copyright (C) 2024-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/enable_family_full_ocl.h"

namespace NEO {

struct Xe2HpgCoreFamily;

static EnableOCL<Xe2HpgCoreFamily> enable;
} // namespace NEO
