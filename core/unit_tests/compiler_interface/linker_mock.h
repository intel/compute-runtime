/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/compiler_interface/linker.h"

template <class BaseClass>
struct WhiteBox;

template <>
struct WhiteBox<NEO::LinkerInput> : NEO::LinkerInput {
    using BaseClass = NEO::LinkerInput;

    using BaseClass::exportedFunctionsSegmentId;
    using BaseClass::relocations;
    using BaseClass::symbols;
    using BaseClass::traits;
};
