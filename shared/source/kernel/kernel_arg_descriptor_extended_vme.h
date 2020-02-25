/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/kernel/kernel_arg_descriptor.h"

namespace NEO {

struct ArgDescVme : ArgDescriptorExtended {
    CrossThreadDataOffset mbBlockType = undefined<CrossThreadDataOffset>;
    CrossThreadDataOffset subpixelMode = undefined<CrossThreadDataOffset>;
    CrossThreadDataOffset sadAdjustMode = undefined<CrossThreadDataOffset>;
    CrossThreadDataOffset searchPathType = undefined<CrossThreadDataOffset>;
};

} // namespace NEO
