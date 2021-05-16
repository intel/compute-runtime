/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#if !defined __has_cpp_attribute

#define CPP_ATTRIBUTE_FALLTHROUGH

#else

#if !__has_cpp_attribute(fallthrough)
#define CPP_ATTRIBUTE_FALLTHROUGH
#else
#define CPP_ATTRIBUTE_FALLTHROUGH [[fallthrough]]
#endif

#endif
