/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "mock_gmm_client_context.h"

namespace NEO {
MockGmmClientContext::MockGmmClientContext(OSInterface *osInterface, HardwareInfo *hwInfo, decltype(&InitializeGmm) initFunc, decltype(&GmmAdapterDestroy) destroyFunc) : MockGmmClientContextBase(osInterface, hwInfo, initFunc, destroyFunc) {
}
} // namespace NEO
