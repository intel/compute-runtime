/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"

#include "stream_properties.inl"

namespace NEO {

struct StreamProperties {
    StateComputeModeProperties stateComputeMode{};
    FrontEndProperties frontEndState{};
};

} // namespace NEO
