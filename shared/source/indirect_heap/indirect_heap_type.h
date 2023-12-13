/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

namespace NEO {
enum IndirectHeapType {
    dynamicState = 0,
    indirectObject,
    surfaceState,
    numTypes
};
} // namespace NEO
