/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <string>

namespace NEO {

struct RegisterEmbeddedResource {
    RegisterEmbeddedResource(const char *name, const char *resource, size_t resourceLength);

    RegisterEmbeddedResource(const char *name, std::string &&resource)
        : RegisterEmbeddedResource(name, resource.data(), resource.size() + 1) {
    }
};

} // namespace NEO
