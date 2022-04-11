/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
bool HwInfoConfigHw<gfxProduct>::isVmBindPatIndexProgrammingSupported() const {
    return true;
}
