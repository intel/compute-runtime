/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#ifndef _WIN32
#define COM_NO_WINDOWS_H
#include "shared/source/os_interface/windows/windows_wrapper.h"
#endif

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wignored-pragma-intrinsic"
#pragma clang diagnostic ignored "-Wpragma-pack"
#pragma clang diagnostic ignored "-Wignored-attributes"
#pragma clang diagnostic ignored "-Wmacro-redefined"
#endif

#include <dxcore.h>

#if __clang__
#pragma clang diagnostic pop
#endif

static const char *const dXCoreCreateAdapterFactoryFuncName = "DXCoreCreateAdapterFactory";

#ifndef _WIN32
template <>
inline GUID uuidof<IDXCoreAdapterFactory>() { return IID_IDXCoreAdapterFactory; }
template <>
inline GUID uuidof<IDXCoreAdapterList>() { return IID_IDXCoreAdapterList; }
template <>
inline GUID uuidof<IDXCoreAdapter>() { return IID_IDXCoreAdapter; }
#endif
