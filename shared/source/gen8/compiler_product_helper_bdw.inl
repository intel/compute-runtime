/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
bool CompilerProductHelperHw<IGFX_BROADWELL>::isStatelessToStatefulBufferOffsetSupported() const {
    return false;
}
