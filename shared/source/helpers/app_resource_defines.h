/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
namespace AppResourceDefines {
template <typename TMemberSource, typename = int>
static constexpr bool hasResourceTag = false;

#if defined(_DEBUG) || (_RELEASE_INTERNAL)
template <typename TMemberSource>
static constexpr bool hasResourceTag<TMemberSource, decltype((void)TMemberSource::ResourceTag, int{})> = true;
constexpr bool resourceTagSupport = true;

#else
constexpr bool resourceTagSupport = false;
template <typename TMemberSource>
static constexpr bool hasResourceTag<TMemberSource, decltype((void)TMemberSource::ResourceTag, int{})> = false;
#endif

constexpr uint32_t maxStrLen = 8u;
} // namespace AppResourceDefines
} // namespace NEO
