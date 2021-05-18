/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <vector>

namespace NEO {

struct StateComputeModeProperties;
struct StreamProperty;

std::vector<StreamProperty *> getAllStateComputeModeProperties(StateComputeModeProperties &properties);

} // namespace NEO
