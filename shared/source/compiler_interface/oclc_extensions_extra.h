/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/utilities/stackvec.h"

#include "CL/cl.h"

using OpenClCFeaturesContainer = StackVec<cl_name_version, 35>;

namespace NEO {
class ReleaseHelper;
void getOpenclCFeaturesListExtra(const ReleaseHelper *releaseHelper, OpenClCFeaturesContainer &openclCFeatures);
} // namespace NEO