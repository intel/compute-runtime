/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

MmioTimestampPtrHelper Wddm::createMmioTimestampPtrHelper(D3DKMT_HANDLE context) {
    return {};
}

} // namespace NEO
