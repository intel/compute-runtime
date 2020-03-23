/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/windows/mock_gdi_interface.h"

namespace NEO {
UINT64 MockGdi::pagingFenceReturnValue = 0x3ull;
} // namespace NEO
