/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>

namespace NEO {

struct RegisterEmbeddedResource {
    RegisterEmbeddedResource(const char *name, const char *resource, size_t resourceLength);
};

} // namespace NEO
