/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gmm_client_context.h"

namespace NEO {
GmmClientContext::GmmClientContext(OSInterface *osInterface, HardwareInfo *hwInfo, decltype(&InitializeGmm) initFunc, decltype(&GmmDestroy) destroyFunc) : GmmClientContextBase(osInterface, hwInfo, initFunc, destroyFunc){};
} // namespace NEO
