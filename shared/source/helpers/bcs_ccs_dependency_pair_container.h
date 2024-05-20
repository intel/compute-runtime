/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <vector>

namespace NEO {
class CommandStreamReceiver;
class TagNodeBase;
using CsrDependencyContainer = std::vector<std::pair<CommandStreamReceiver *, TagNodeBase *>>;
} // namespace NEO
