/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef WIN32
#include "shared/source/os_interface/windows/windows_wrapper.h"

#else
#ifndef C_ASSERT
#define C_ASSERT(e) static_assert(e, #e)
#endif
#define __stdcall
#endif
#include "GmmLib.h"
