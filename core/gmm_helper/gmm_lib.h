/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#ifdef WIN32
#include "core/os_interface/windows/windows_wrapper.h"

#else
#ifndef C_ASSERT
#define C_ASSERT(e) static_assert(e, #e)
#endif
#define __stdcall
#endif
#include "GmmLib.h"
