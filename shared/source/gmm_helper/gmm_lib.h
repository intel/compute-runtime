/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef _WIN32
#include "shared/source/os_interface/windows/windows_wrapper.h"
#else // !_WIN32
#ifdef WDDM_LINUX
#include "shared/source/os_interface/windows/d3dkmthk_wrapper.h"

#include "umKmInc/sharedata.h"
#ifdef LHDM
#undef LHDM
#define RESTORE_LHDM
#endif
#undef WDDM_LINUX
#define RESTORE_WDDM_LINUX
#define UFO_PORTABLE_COMPILER_H
#define UFO_PORTABLE_WINDEF_H
#else // !WDDM_LINUX
#define __stdcall
#endif // !WDDM_LINUX
#endif // !_WIN32

#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonportable-include-path"
#pragma clang diagnostic ignored "-Wmissing-braces"
#pragma clang diagnostic ignored "-Wparentheses-equality"

#endif

#include "GmmLib.h"

#if __clang__
#pragma clang diagnostic pop
#endif

#ifdef RESTORE_WDDM_LINUX
#ifdef RESTORE_LHDM
#define LHDM 1
#endif
#define WDDM_LINUX 1
#ifndef GMM_ESCAPE_HANDLE
#define GMM_ESCAPE_HANDLE D3DKMT_HANDLE
#endif
#ifndef GMM_ESCAPE_FUNC_TYPE
#define GMM_ESCAPE_FUNC_TYPE PFND3DKMT_ESCAPE
#endif
#ifndef GMM_HANDLE
#define GMM_HANDLE D3DKMT_HANDLE
#endif
#endif // RESTORE_WDDM_LINUX
