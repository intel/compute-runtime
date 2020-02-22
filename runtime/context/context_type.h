/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {

enum ContextType : uint32_t {
    CONTEXT_TYPE_DEFAULT,
    CONTEXT_TYPE_SPECIALIZED,
    CONTEXT_TYPE_UNRESTRICTIVE
};

} // namespace NEO
