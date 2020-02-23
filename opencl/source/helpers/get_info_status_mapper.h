/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/get_info_status.h"

#include <CL/cl.h>

#include <cassert>

static inline cl_int changeGetInfoStatusToCLResultType(GetInfoStatus status) {
    switch (status) {
    case GetInfoStatus::SUCCESS:
        return CL_SUCCESS;

    case GetInfoStatus::INVALID_CONTEXT:
        return CL_INVALID_CONTEXT;

    case GetInfoStatus::INVALID_VALUE:
        return CL_INVALID_VALUE;
    }

    return CL_INVALID_VALUE;
}
