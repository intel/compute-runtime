/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

template <>
void setFilterMode<Family>(typename Family::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo){};

template <>
bool checkIfArrayNeeded<Family>(ImageType type, const HardwareInfo *hwInfo) {
    return false;
};