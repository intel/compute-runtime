/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "wsl_compute_helper_types_tokens.h"

#ifdef _WIN32
#define CCONV __cdecl
static const char *wslComputeHelperLibName = "wsl_compute_helper.dll";
#else
#define CCONV
[[maybe_unused]] static const char *wslComputeHelperLibName = "libwsl_compute_helper.so";
#endif

typedef size_t(CCONV *getSizeRequiredForStructFPT)(TOK structId);
static const char *const getSizeRequiredForStructName = "getSizeRequiredForStruct";

typedef bool(CCONV *tokensToStructFPT)(TOK structId, void *dst, size_t dstSizeInBytes, const TokenHeader *begin, const TokenHeader *end);
static const char *const tokensToStructName = "tokensToStruct";

typedef size_t(CCONV *getSizeRequiredForTokensFPT)(TOK structId);
static const char *const getSizeRequiredForTokensName = "getSizeRequiredForTokens";

typedef bool(CCONV *structToTokensFPT)(TOK structId, TokenHeader *dst, size_t dstSizeInBytes, const void *src, size_t srcSizeInBytes);
static const char *const structToTokensName = "structToTokens";

typedef void(CCONV *destroyStructRepresentationFPT)(TOK structId, void *src, size_t srcSizeInBytes);
static const char *const destroyStructRepresentationName = "destroyStructRepresentation";

typedef uint64_t(CCONV *getVersionFPT)();
static const char *const getVersionName = "getVersion";
