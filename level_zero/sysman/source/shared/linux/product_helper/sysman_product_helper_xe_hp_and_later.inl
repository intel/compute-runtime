/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void SysmanProductHelperHw<gfxProduct>::getFrequencyStepSize(double *pStepSize) {
    *pStepSize = 50.0;
}