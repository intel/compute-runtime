/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#if defined(__AVX2__) || defined(_MSC_VER)
#include "shared/source/helpers/x86_64/stream_copy.inl"

namespace NEO {
template void streamCopyImpl<false>(void *dst, const void *src, size_t bytes) noexcept;
} // namespace NEO
#endif
