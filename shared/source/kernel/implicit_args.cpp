/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "implicit_args.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>

namespace NEO {

static_assert(sizeof(ImplicitArgsHeader) == (2 * sizeof(uint8_t)));

static_assert(std::alignment_of_v<ImplicitArgsV0> == 32, "Implicit args size need to be aligned to 32");
static_assert(sizeof(ImplicitArgsV0) == (32 * sizeof(uint32_t)));
static_assert(NEO::TypeTraits::isPodV<ImplicitArgsV0>);

static_assert(std::alignment_of_v<ImplicitArgsV1> == 32, "Implicit args size need to be aligned to 32");
static_assert(sizeof(ImplicitArgsV1) == (40 * sizeof(uint32_t)));
static_assert(NEO::TypeTraits::isPodV<ImplicitArgsV1>);

static_assert(std::alignment_of_v<ImplicitArgsV2> == 32, "Implicit args size need to be aligned to 32");
static_assert(sizeof(ImplicitArgsV2) == (40 * sizeof(uint32_t)));
static_assert(NEO::TypeTraits::isPodV<ImplicitArgsV2>);

static_assert(NEO::TypeTraits::isPodV<ImplicitArgs>);

} // namespace NEO
