/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/enable_family_full_ocl.h"

namespace NEO {

struct Xe3CoreFamily;

static EnableOCL<Xe3CoreFamily> enable;
} // namespace NEO
