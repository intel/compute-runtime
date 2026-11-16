/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {

template <>
bool CompilerProductHelperHw<gfxProduct>::failBuildProgramWithBufferStatefulAccessPreference() const {
    return true;
}

} // namespace NEO
