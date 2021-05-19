/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <optional>
#include <string>

namespace NEO {

std::optional<std::string> getPciPath(int deviceFd);

}