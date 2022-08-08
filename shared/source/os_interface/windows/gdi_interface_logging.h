/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"

namespace NEO {

namespace GdiLogging {

#if defined(_RELEASE_INTERNAL) || (_DEBUG)
constexpr bool gdiLoggingSupport = true;
#else
constexpr bool gdiLoggingSupport = false;
#endif

template <typename Param>
void logEnter(Param param);

template <typename Param>
void logExit(NTSTATUS status, Param param);

void init();

void close();

} // namespace GdiLogging

} // namespace NEO
