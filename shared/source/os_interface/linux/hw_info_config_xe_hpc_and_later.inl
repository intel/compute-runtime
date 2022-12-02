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

template <>
size_t HwInfoConfigHw<gfxProduct>::getSvmCpuAlignment() const {
    return MemoryConstants::pageSize64k;
}