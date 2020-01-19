/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/gmm_helper/gmm_lib.h"

namespace NEO {

namespace GmmInterface {

GMM_STATUS initialize(GMM_INIT_IN_ARGS *pInArgs, GMM_INIT_OUT_ARGS *pOutArgs);

void destroy(GMM_INIT_OUT_ARGS *pInArgs);
} // namespace GmmInterface
} // namespace NEO
