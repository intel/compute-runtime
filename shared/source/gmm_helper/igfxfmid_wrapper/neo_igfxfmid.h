/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "igfxfmid.h"

namespace NEO {
constexpr GFXCORE_FAMILY xe3pCoreEnumValue = static_cast<GFXCORE_FAMILY>(0x2300);                    // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-16649
constexpr GFXCORE_FAMILY maxCoreEnumValue = static_cast<GFXCORE_FAMILY>(xe3pCoreEnumValue + 1);      // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-16649
constexpr PRODUCT_FAMILY nvlsProductEnumValue = static_cast<PRODUCT_FAMILY>(1340);                   // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-16649
constexpr PRODUCT_FAMILY criProductEnumValue = static_cast<PRODUCT_FAMILY>(1380);                    // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-16649
constexpr PRODUCT_FAMILY maxProductEnumValue = static_cast<PRODUCT_FAMILY>(criProductEnumValue + 1); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange), NEO-16649
} // namespace NEO
