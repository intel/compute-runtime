/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <vector>

namespace NEO {

struct FrontEndProperties;
struct PipelineSelectProperties;
struct StateComputeModeProperties;
struct StreamProperty;

std::vector<StreamProperty *> getAllStateComputeModeProperties(StateComputeModeProperties &properties);
std::vector<StreamProperty *> getAllFrontEndProperties(FrontEndProperties &properties);
std::vector<StreamProperty *> getAllPipelineSelectProperties(PipelineSelectProperties &properties);

} // namespace NEO
