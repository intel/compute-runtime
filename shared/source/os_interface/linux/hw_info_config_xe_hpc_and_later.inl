/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
bool ProductHelperHw<gfxProduct>::isVmBindPatIndexProgrammingSupported() const {
    return true;
}

template <>
size_t ProductHelperHw<gfxProduct>::getSvmCpuAlignment() const {
    return MemoryConstants::pageSize64k;
}