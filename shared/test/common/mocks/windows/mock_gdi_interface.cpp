/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/windows/mock_gdi_interface.h"

namespace NEO {
UINT64 MockGdi::pagingFenceReturnValue = 0x3ull;
LUID MockGdi::adapterLuidToReturn{};
} // namespace NEO
