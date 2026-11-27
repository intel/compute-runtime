/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/enable_family_full_ocl.h"

namespace NEO {

struct Gen12LpFamily;

static EnableOCL<Gen12LpFamily> enable;
} // namespace NEO
