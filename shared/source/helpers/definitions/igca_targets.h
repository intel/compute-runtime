/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <map>

namespace AOT {

enum IgcaFeatureSetFlag : uint32_t {
    IGCA_FEATURE_SET_CORE = 0x0,
    IGCA_FEATURE_SET_COMPUTE = 0x1,
    IGCA_FEATURE_SET_RENDER = 0x2,
    IGCA_FEATURE_SET_EXACT = 0x4,
};

inline constexpr uint32_t igcaTargetVersionShift = 16;
inline constexpr uint32_t igcaTargetFeatureSetMask = 0xffff;

enum IgcaTargetValue : uint32_t {
    UNKNOWN_IGCA_TARGET = 0,
    IGCA_10R = 0x000a0002,
    IGCA_15RA = 0x000f0006,
    IGCA_20CA = 0x00140005,
    IGCA_30R = 0x001e0002,
    IGCA_35RA = 0x00230006,
    IGCA_40R = 0x00280002,
    IGCA_50R = 0x00320002,
    IGCA_60 = 0x003c0000,
    IGCA_60R = 0x003c0002,
};

enum class IgcaProduct : uint32_t {
    tgl = 0x03000000,
    rkl = 0x03004000,
    adlS = 0x03008000,
    adlP = 0x0300c000,
    adlN = 0x03010000,
    dg1 = 0x03028000,
    dg2G10 = 0x030dc000,
    dg2G11 = 0x030e0000,
    dg2G12 = 0x030e4000,
    pvc = 0x030f0000,
    pvcVg = 0x030f4000,
    mtlU = 0x03118000,
    mtlH = 0x0311c000,
    arlH = 0x03128000,
    bmgG21 = 0x05004000,
    bmgG31 = 0x05008000,
    lnl = 0x05010000,
    ptlH = 0x07800000,
    ptlU = 0x07804000,
    wcl = 0x0780c000,
    nvlS = 0x07810000,
    nvlU = 0x07814000,
    nvlP = 0x08c28000,
    cri = 0x08c2c000,
};

inline const std::map<IgcaProduct, IgcaTargetValue> igcaProductTargets = {
    {IgcaProduct::tgl, IGCA_10R},
    {IgcaProduct::rkl, IGCA_10R},
    {IgcaProduct::adlS, IGCA_10R},
    {IgcaProduct::adlP, IGCA_10R},
    {IgcaProduct::adlN, IGCA_10R},
    {IgcaProduct::dg1, IGCA_10R},
    {IgcaProduct::dg2G10, IGCA_15RA},
    {IgcaProduct::dg2G11, IGCA_15RA},
    {IgcaProduct::dg2G12, IGCA_15RA},
    {IgcaProduct::pvc, IGCA_20CA},
    {IgcaProduct::pvcVg, IGCA_20CA},
    {IgcaProduct::mtlU, IGCA_30R},
    {IgcaProduct::mtlH, IGCA_30R},
    {IgcaProduct::arlH, IGCA_35RA},
    {IgcaProduct::bmgG21, IGCA_40R},
    {IgcaProduct::bmgG31, IGCA_40R},
    {IgcaProduct::lnl, IGCA_40R},
    {IgcaProduct::ptlH, IGCA_50R},
    {IgcaProduct::ptlU, IGCA_50R},
    {IgcaProduct::wcl, IGCA_50R},
    {IgcaProduct::nvlS, IGCA_60R},
    {IgcaProduct::nvlU, IGCA_60R},
    {IgcaProduct::nvlP, IGCA_60R},
    {IgcaProduct::cri, IGCA_60},
};
} // namespace AOT
