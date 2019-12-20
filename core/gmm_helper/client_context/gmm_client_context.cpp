/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "gmm_client_context.h"

namespace NEO {
GmmClientContext::GmmClientContext(OSInterface *osInterface, HardwareInfo *hwInfo, decltype(&InitializeGmm) initFunc, decltype(&GmmAdapterDestroy) destroyFunc) : GmmClientContextBase(osInterface, hwInfo, initFunc, destroyFunc){};
} // namespace NEO
