/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/vec.h"

#include <algorithm>

namespace NEO {
struct WorkSizeInfo;

void computeWorkgroupSize1D(
    uint32_t maxWorkGroupSize,
    size_t workGroupSize[3],
    const size_t workItems[3],
    size_t simdSize);

void computeWorkgroupSizeND(
    WorkSizeInfo &wsInfo,
    size_t workGroupSize[3],
    const size_t workItems[3],
    const uint32_t workDim);

void computeWorkgroupSize2D(
    uint32_t maxWorkGroupSize,
    size_t workGroupSize[3],
    const size_t workItems[3],
    size_t simdSize);

void computeWorkgroupSizeSquared(
    uint32_t maxWorkGroupSize,
    size_t workGroupSize[3],
    const size_t workItems[3],
    size_t simdSize,
    const uint32_t workDim);

Vec3<size_t> computeWorkgroupsNumber(
    const Vec3<size_t> &gws,
    const Vec3<size_t> &lws);

Vec3<size_t> generateWorkgroupsNumber(
    const Vec3<size_t> &gws,
    const Vec3<size_t> &lws);

inline uint32_t calculateDispatchDim(const Vec3<size_t> &dispatchSize, const Vec3<size_t> &dispatchOffset) {
    return std::max(1U, std::max(dispatchSize.getSimplifiedDim(), dispatchOffset.getSimplifiedDim()));
}

Vec3<size_t> canonizeWorkgroup(
    const Vec3<size_t> &workgroup);

inline uint32_t computeDimensions(const size_t workItems[3]) {
    return (workItems[2] > 1) ? 3 : (workItems[1] > 1) ? 2
                                                       : 1;
}

} // namespace NEO
