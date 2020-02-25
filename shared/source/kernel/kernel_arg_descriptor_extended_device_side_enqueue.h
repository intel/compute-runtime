/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/kernel/kernel_arg_descriptor.h"

namespace NEO {

struct ArgDescriptorDeviceSideEnqueue : ArgDescriptorExtended {
    CrossThreadDataOffset objectId = undefined<CrossThreadDataOffset>;
};

} // namespace NEO
