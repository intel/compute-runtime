/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

namespace NEO {

struct BlitProperties;

using BlitPropertiesContainer = StackVec<BlitProperties, 16>;

} // namespace NEO