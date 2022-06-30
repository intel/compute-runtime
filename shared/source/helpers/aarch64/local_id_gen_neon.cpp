/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aarch64/uint16_neon.h"
#include "shared/source/helpers/local_id_gen.inl"

#include <array>

namespace NEO {
template void generateLocalIDsSimd<uint16x16_t, 8>(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder, bool chooseMaxRowSize);
template void generateLocalIDsSimd<uint16x16_t, 16>(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder, bool chooseMaxRowSize);
template void generateLocalIDsSimd<uint16x16_t, 32>(void *b, const std::array<uint16_t, 3> &localWorkgroupSize, uint16_t threadsPerWorkGroup, const std::array<uint8_t, 3> &dimensionsOrder, bool chooseMaxRowSize);
} // namespace NEO
