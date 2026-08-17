/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>
#include <string_view>
#include <type_traits>

namespace NEO {

// links itself into a static list, so the node, its name and its resource must live as long as the module
struct RegisterEmbeddedResource {
    RegisterEmbeddedResource(const char *name, const char *resource, size_t resourceLength);

    RegisterEmbeddedResource(const RegisterEmbeddedResource &) = delete;
    RegisterEmbeddedResource &operator=(const RegisterEmbeddedResource &) = delete;

    static const RegisterEmbeddedResource *find(std::string_view name);
    static bool anyRegistered();

    std::string_view name;
    const char *resource;
    size_t resourceLength;
    RegisterEmbeddedResource *next = nullptr;
};

static_assert(std::is_trivially_destructible_v<RegisterEmbeddedResource>);

} // namespace NEO
