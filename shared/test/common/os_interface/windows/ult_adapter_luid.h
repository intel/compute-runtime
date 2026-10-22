/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/windows_wrapper.h"

#include <cstdint>

namespace NEO {
namespace UltAdapterLuid {

inline constexpr LONG dxCoreFactoryId = 0x1000;
inline constexpr LONG dxgiFactoryId = 0x2000;
inline constexpr LONG kmtFactoryId = 0x3000;
inline constexpr DWORD adapterIndexBase = 0x100;

constexpr LUID create(LONG factoryId, uint32_t adapterIndex) {
    return LUID{adapterIndexBase + adapterIndex, factoryId};
}

} // namespace UltAdapterLuid
} // namespace NEO
