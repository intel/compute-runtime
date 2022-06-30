/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/windows/windows_wrapper.h"

#ifndef _WIN32
#define ScanLineOrdering \
    ScanLineOrdering:    \
    8;                   \
    int padding
#endif
#include <d3dkmthk.h>

struct _D3DDDICB_LOCKFLAGS;
typedef _D3DDDICB_LOCKFLAGS D3DDDICB_LOCKFLAGS;

struct _D3DKMT_TRIMNOTIFICATION;
typedef struct _D3DKMT_TRIMNOTIFICATION D3DKMT_TRIMNOTIFICATION;
typedef VOID(APIENTRY *PFND3DKMT_TRIMNOTIFICATIONCALLBACK)(_Inout_ D3DKMT_TRIMNOTIFICATION *);

typedef NTSTATUS(APIENTRY *PFND3DKMT_ESCAPE)(_In_ CONST D3DKMT_ESCAPE *);
