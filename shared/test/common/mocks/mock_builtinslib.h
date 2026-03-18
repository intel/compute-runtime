/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/built_ins.h"

using namespace NEO;
class MockBuiltInResourceLoader : BuiltIn::ResourceLoader {
  public:
    using BuiltIn::ResourceLoader::allStorages;
    using BuiltIn::ResourceLoader::getBuiltinCode;
    using BuiltIn::ResourceLoader::getBuiltinResource;
};
