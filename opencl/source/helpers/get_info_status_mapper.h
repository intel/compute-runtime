/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/get_info_status.h"

#include <CL/cl.h>

static inline cl_int changeGetInfoStatusToCLResultType(GetInfoStatus status) {
    switch (status) {
    case GetInfoStatus::success:
        return CL_SUCCESS;

    case GetInfoStatus::invalidContext:
        return CL_INVALID_CONTEXT;

    case GetInfoStatus::invalidValue:
        return CL_INVALID_VALUE;
    }

    return CL_INVALID_VALUE;
}
