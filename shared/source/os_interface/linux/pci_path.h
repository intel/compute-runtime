/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <optional>
#include <string>

namespace NEO {

std::optional<std::string> getPciPath(int deviceFd);
std::optional<std::string> getPciLinkPath(int deviceFd);
std::optional<std::string> getPciRootPath(int deviceFd);

} // namespace NEO