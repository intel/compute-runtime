/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef WIN32
#include "runtime/os_interface/windows/windows_wrapper.h"

#include <d3d9types.h>

#include "d3dumddi.h"
#include <d3dkmthk.h>
#else
#ifndef C_ASSERT
#define C_ASSERT(e) static_assert(e, #e)
#endif
#define __stdcall
#endif
#include "GmmLib.h"
