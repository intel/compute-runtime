/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
std::string SysmanProductHelperHw<gfxProduct>::getGpuGeneration() {
    return "xe";
}
