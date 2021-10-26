/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
bool CompilerHwInfoConfigHw<IGFX_ROCKETLAKE>::isForceEmuInt32DivRemSPRequired() const {
    return true;
}
