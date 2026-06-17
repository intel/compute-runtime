/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>

namespace NEO {
inline constexpr size_t streamCopyAvx512Width = 64;
inline constexpr size_t streamCopyAvx2Width = 32;
inline constexpr size_t streamCopySseWidth = 16;

template <bool withAvx512>
void streamCopyImpl(void *dst, const void *src, size_t bytes) noexcept;
} // namespace NEO
