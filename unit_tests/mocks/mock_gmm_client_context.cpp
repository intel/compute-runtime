/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_gmm_client_context.h"

namespace NEO {
MockGmmClientContext::MockGmmClientContext(HardwareInfo *hwInfo, decltype(&InitializeGmm) initFunc, decltype(&GmmDestroy) destroyFunc) : MockGmmClientContextBase(hwInfo, initFunc, destroyFunc) {
}
} // namespace NEO
