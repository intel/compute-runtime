/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_container/cmdcontainer.h"

struct MockCommandContainer : public NEO::CommandContainer {
    using CommandContainer::cmdBufferAllocations;
    using CommandContainer::dirtyHeaps;
    using CommandContainer::isFlushTaskUsedForImmediate;
    using CommandContainer::reusableAllocationList;
};
