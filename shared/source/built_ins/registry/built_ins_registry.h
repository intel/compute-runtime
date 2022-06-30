/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/built_ins/built_ins.h"

#include <string>
#include <unordered_map>

namespace NEO {

struct RegisterEmbeddedResource {
    RegisterEmbeddedResource(const char *name, const char *resource, size_t resourceLength) {
        auto &storageRegistry = EmbeddedStorageRegistry::getInstance();
        storageRegistry.store(name, createBuiltinResource(resource, resourceLength));
    }

    RegisterEmbeddedResource(const char *name, std::string &&resource)
        : RegisterEmbeddedResource(name, resource.data(), resource.size() + 1) {
    }
};

} // namespace NEO
