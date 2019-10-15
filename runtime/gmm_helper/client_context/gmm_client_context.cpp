/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gmm_client_context.h"

namespace NEO {
GmmClientContext::GmmClientContext(HardwareInfo *hwInfo, decltype(&InitializeGmm) initFunc, decltype(&GmmDestroy) destroyFunc) : GmmClientContextBase(hwInfo, initFunc, destroyFunc){};
} // namespace NEO
