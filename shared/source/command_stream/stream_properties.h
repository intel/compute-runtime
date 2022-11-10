/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "stream_properties.inl"

namespace NEO {

struct StreamProperties {
    StateComputeModeProperties stateComputeMode{};
    FrontEndProperties frontEndState{};
    PipelineSelectProperties pipelineSelect{};
};

} // namespace NEO
