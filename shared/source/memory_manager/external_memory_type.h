/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
enum class ExternalMemoryType {
    opaqueFd,
    dmaBuf,
    opaqueWin32,
    opaqueWin32Kmt,
    d3d11Texture,
    d3d11TextureKmt,
    d3d12Heap,
    d3d12Resource,
    invalid
};
} // namespace NEO
