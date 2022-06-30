/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#ifndef _WIN32
#define COM_NO_WINDOWS_H
#include "shared/source/os_interface/windows/windows_wrapper.h"
#endif

#include <dxcore.h>

static const char *const dXCoreCreateAdapterFactoryFuncName = "DXCoreCreateAdapterFactory";

#ifndef _WIN32
template <>
inline GUID uuidof<IDXCoreAdapterFactory>() { return IID_IDXCoreAdapterFactory; }
template <>
inline GUID uuidof<IDXCoreAdapterList>() { return IID_IDXCoreAdapterList; }
template <>
inline GUID uuidof<IDXCoreAdapter>() { return IID_IDXCoreAdapter; }
#endif
