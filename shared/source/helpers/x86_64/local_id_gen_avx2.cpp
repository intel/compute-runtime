/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#if __AVX2__
#include "shared/source/helpers/local_id_gen.inl"
#include "shared/source/helpers/uint16_avx2.h"

#include <array>

namespace NEO {
template void generateLocalIDsSimd<uint16x16_t, 32>(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder, bool chooseMaxRowSize);
template void generateLocalIDsSimd<uint16x16_t, 16>(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder, bool chooseMaxRowSize);
} // namespace NEO
#endif
