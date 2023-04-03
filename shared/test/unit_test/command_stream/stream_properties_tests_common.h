/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <vector>

namespace NEO {

struct FrontEndProperties;
struct PipelineSelectProperties;
struct StateComputeModeProperties;

template <typename Type, bool fullStatePropert>
struct StreamPropertyType;

using StreamProperty = StreamPropertyType<int32_t, true>;

std::vector<StreamProperty *> getAllStateComputeModeProperties(StateComputeModeProperties &properties);
std::vector<StreamProperty *> getAllFrontEndProperties(FrontEndProperties &properties);
std::vector<StreamProperty *> getAllPipelineSelectProperties(PipelineSelectProperties &properties);

} // namespace NEO
