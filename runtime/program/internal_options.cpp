/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/program/program.h"
#include <vector>

namespace OCLRT {
const std::vector<std::string> Program::internalOptionsToExtract = {"-cl-intel-gtpin-rera"};
};
