/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>

namespace NEO {

using MemoryUnmapFunction = int (*)(void *, std::size_t);

} // namespace NEO
