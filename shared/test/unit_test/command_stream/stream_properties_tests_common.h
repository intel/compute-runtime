/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <vector>

namespace NEO {

struct FrontEndProperties;
struct StateComputeModeProperties;
struct StreamProperty;

std::vector<StreamProperty *> getAllStateComputeModeProperties(StateComputeModeProperties &properties);
std::vector<StreamProperty *> getAllFrontEndProperties(FrontEndProperties &properties);

} // namespace NEO
