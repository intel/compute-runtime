/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/vec.h"
#include "shared/source/program/kernel_info.h"

namespace NEO {
class Context;
class DispatchInfo;

Vec3<size_t> computeWorkgroupSize(
    const DispatchInfo &dispatchInfo);

Vec3<size_t> generateWorkgroupSize(
    const DispatchInfo &dispatchInfo);

Vec3<size_t> generateWorkgroupsNumber(
    const DispatchInfo &dispatchInfo);

void provideLocalWorkGroupSizeHints(Context *context, const DispatchInfo &dispatchInfo);

WorkSizeInfo createWorkSizeInfoFromDispatchInfo(const DispatchInfo &dispatchInfo);

} // namespace NEO
