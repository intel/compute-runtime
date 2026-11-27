/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/enable_family_full_ocl.h"

namespace NEO {

struct XeHpcCoreFamily;

static EnableOCL<XeHpcCoreFamily> enable;
} // namespace NEO
