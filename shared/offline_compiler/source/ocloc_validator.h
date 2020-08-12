/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>
#include <vector>

class OclocArgHelper;

namespace NEO {
namespace Ocloc {

int validate(const std::vector<std::string> &args, OclocArgHelper *argHelper);
}

} // namespace NEO
