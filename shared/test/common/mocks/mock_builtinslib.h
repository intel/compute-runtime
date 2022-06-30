/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/built_ins.h"

using namespace NEO;
class MockBuiltinsLib : BuiltinsLib {
  public:
    using BuiltinsLib::allStorages;
    using BuiltinsLib::getBuiltinResource;
};
