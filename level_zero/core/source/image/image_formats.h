/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/surface_format_info.h"

#include <array>

namespace L0 {
namespace ImageFormats {
constexpr uint32_t ZE_IMAGE_FORMAT_RENDER_LAYOUT_MAX = 43u;

using FormatTypes = std::array<NEO::SurfaceFormatInfo, 5u>;

constexpr std::array<NEO::SurfaceFormatInfo, 5u> surfaceFormatsForRedescribe = {
    {{GMM_FORMAT_R8_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8_UINT, 0, 1, 1, 1},
     {GMM_FORMAT_R16_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16_UINT, 0, 1, 2, 2},
     {GMM_FORMAT_R32_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32_UINT, 0, 1, 4, 4},
     {GMM_FORMAT_R32G32_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32G32_UINT, 0, 2, 4, 8},
     {GMM_FORMAT_R32G32B32A32_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_UINT, 0, 4, 4, 16}}};

constexpr FormatTypes layout8 = {{{GMM_FORMAT_R8_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8_UINT, 0, 1, 1, 1},
                                  {GMM_FORMAT_R8_SINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8_SINT, 0, 1, 1, 1},
                                  {GMM_FORMAT_R8_UNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8_UNORM, 0, 1, 1, 1},
                                  {GMM_FORMAT_R8_SNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8_SNORM, 0, 1, 1, 1},
                                  {GMM_FORMAT_R8_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8_UINT, 0, 1, 1, 1}}};
constexpr FormatTypes layout88 = {{{GMM_FORMAT_R8G8_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8_UINT, 0, 2, 1, 2},
                                   {GMM_FORMAT_R8G8_SINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8_SINT, 0, 2, 1, 2},
                                   {GMM_FORMAT_R8G8_UNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8_UNORM, 0, 2, 1, 2},
                                   {GMM_FORMAT_R8G8_SNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8_SNORM, 0, 2, 1, 2},
                                   {GMM_FORMAT_R8G8_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8_UINT, 0, 2, 1, 2}}};
constexpr FormatTypes layout8888 = {{{GMM_FORMAT_R8G8B8A8_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UINT, 0, 4, 1, 4},
                                     {GMM_FORMAT_R8G8B8A8_SINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_SINT, 0, 4, 1, 4},
                                     {GMM_FORMAT_R8G8B8A8_UNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM, 0, 4, 1, 4},
                                     {GMM_FORMAT_R8G8B8A8_SNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_SNORM, 0, 4, 1, 4},
                                     {GMM_FORMAT_R8G8B8A8_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UINT, 0, 4, 1, 4}}};
constexpr FormatTypes layout16 = {{{GMM_FORMAT_R16_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16_UINT, 0, 1, 2, 2},
                                   {GMM_FORMAT_R16_SINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16_SINT, 0, 1, 2, 2},
                                   {GMM_FORMAT_R16_UNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16_UNORM, 0, 1, 2, 2},
                                   {GMM_FORMAT_R16_SNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16_SNORM, 0, 1, 2, 2},
                                   {GMM_FORMAT_R16_FLOAT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16_FLOAT, 0, 1, 2, 2}}};
constexpr FormatTypes layout1616 = {{{GMM_FORMAT_R16G16_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16_UINT, 0, 2, 2, 4},
                                     {GMM_FORMAT_R16G16_SINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16_SINT, 0, 2, 2, 4},
                                     {GMM_FORMAT_R16G16_UNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16_UNORM, 0, 2, 2, 4},
                                     {GMM_FORMAT_R16G16_SNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16_SNORM, 0, 2, 2, 4},
                                     {GMM_FORMAT_R16G16_FLOAT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16_FLOAT, 0, 2, 2, 4}}};
constexpr FormatTypes layout16161616 = {{{GMM_FORMAT_R16G16B16A16_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UINT, 0, 4, 2, 8},
                                         {GMM_FORMAT_R16G16B16A16_SINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_SINT, 0, 4, 2, 8},
                                         {GMM_FORMAT_R16G16B16A16_UNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8},
                                         {GMM_FORMAT_R16G16B16A16_SNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_SNORM, 0, 4, 2, 8},
                                         {GMM_FORMAT_R16G16B16A16_FLOAT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_FLOAT, 0, 4, 2, 8}}};
constexpr FormatTypes layout32 = {{{GMM_FORMAT_R32_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32_UINT, 0, 1, 4, 4},
                                   {GMM_FORMAT_R32_SINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32_SINT, 0, 1, 4, 4},
                                   {GMM_FORMAT_R32_UNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32_UNORM, 0, 1, 4, 4},
                                   {GMM_FORMAT_R32_SNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32_SNORM, 0, 1, 4, 4},
                                   {GMM_FORMAT_R32_FLOAT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32_FLOAT, 0, 1, 4, 4}}};
constexpr FormatTypes layout3232 = {{{GMM_FORMAT_R32G32_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32G32_UINT, 0, 2, 4, 8},
                                     {GMM_FORMAT_R32G32_SINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32G32_SINT, 0, 2, 4, 8},
                                     {GMM_FORMAT_R32G32_UNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32G32_UNORM, 0, 2, 4, 8},
                                     {GMM_FORMAT_R32G32_SNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32G32_SNORM, 0, 2, 4, 8},
                                     {GMM_FORMAT_R32G32_FLOAT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32G32_FLOAT, 0, 2, 4, 8}}};
constexpr FormatTypes layout32323232 = {{{GMM_FORMAT_R32G32B32A32_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_UINT, 0, 4, 4, 16},
                                         {GMM_FORMAT_R32G32B32A32_SINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_SINT, 0, 4, 4, 16},
                                         {GMM_FORMAT_R32G32B32A32_UNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_UNORM, 0, 4, 4, 16},
                                         {GMM_FORMAT_R32G32B32A32_SNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_SNORM, 0, 4, 4, 16},
                                         {GMM_FORMAT_R32G32B32A32_FLOAT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R32G32B32A32_FLOAT, 0, 4, 4, 16}}};
constexpr FormatTypes layout1010102 = {{{GMM_FORMAT_R10G10B10A2_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R10G10B10A2_UINT, 0, 4, 1, 4},
                                        {GMM_FORMAT_R10G10B10A2_SINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R10G10B10A2_SINT, 0, 4, 1, 4},
                                        {GMM_FORMAT_R10G10B10A2_UNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R10G10B10A2_UNORM, 0, 4, 1, 4},
                                        {GMM_FORMAT_R10G10B10A2_SNORM_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R10G10B10A2_SNORM, 0, 4, 1, 4},
                                        {GMM_FORMAT_R10G10B10A2_UINT_TYPE, NEO::GFX3DSTATE_SURFACEFORMAT_R10G10B10A2_UINT, 0, 4, 1, 4}}};
constexpr FormatTypes layout111110 = {{{GMM_FORMAT_R11G11B10_FLOAT, NEO::GFX3DSTATE_SURFACEFORMAT_R11G11B10_FLOAT, 0, 3, 0, 4},
                                       {GMM_FORMAT_R11G11B10_FLOAT, NEO::GFX3DSTATE_SURFACEFORMAT_R11G11B10_FLOAT, 0, 3, 0, 4},
                                       {GMM_FORMAT_R11G11B10_FLOAT, NEO::GFX3DSTATE_SURFACEFORMAT_R11G11B10_FLOAT, 0, 3, 0, 4},
                                       {GMM_FORMAT_R11G11B10_FLOAT, NEO::GFX3DSTATE_SURFACEFORMAT_R11G11B10_FLOAT, 0, 3, 0, 4},
                                       {GMM_FORMAT_R11G11B10_FLOAT, NEO::GFX3DSTATE_SURFACEFORMAT_R11G11B10_FLOAT, 0, 3, 0, 4}}};
constexpr FormatTypes layout565 = {{{GMM_FORMAT_B5G6R5_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B5G6R5_UNORM, 0, 3, 0, 2},
                                    {GMM_FORMAT_B5G6R5_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B5G6R5_UNORM, 0, 3, 0, 2},
                                    {GMM_FORMAT_B5G6R5_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B5G6R5_UNORM, 0, 3, 0, 2},
                                    {GMM_FORMAT_B5G6R5_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B5G6R5_UNORM, 0, 3, 0, 2},
                                    {GMM_FORMAT_B5G6R5_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B5G6R5_UNORM, 0, 3, 0, 2}}};
constexpr FormatTypes layout5551 = {{{GMM_FORMAT_B5G5R5A1_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B5G5R5A1_UNORM, 0, 4, 0, 2},
                                     {GMM_FORMAT_B5G5R5A1_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B5G5R5A1_UNORM, 0, 4, 0, 2},
                                     {GMM_FORMAT_B5G5R5A1_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B5G5R5A1_UNORM, 0, 4, 0, 2},
                                     {GMM_FORMAT_B5G5R5A1_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B5G5R5A1_UNORM, 0, 4, 0, 2},
                                     {GMM_FORMAT_B5G5R5A1_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B5G5R5A1_UNORM, 0, 4, 0, 2}}};
constexpr FormatTypes layout4444 = {{{GMM_FORMAT_B4G4R4A4_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B4G4R4A4_UNORM, 0, 4, 1, 2},
                                     {GMM_FORMAT_B4G4R4A4_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B4G4R4A4_UNORM, 0, 4, 1, 2},
                                     {GMM_FORMAT_B4G4R4A4_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B4G4R4A4_UNORM, 0, 4, 1, 2},
                                     {GMM_FORMAT_B4G4R4A4_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B4G4R4A4_UNORM, 0, 4, 1, 2},
                                     {GMM_FORMAT_B4G4R4A4_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_B4G4R4A4_UNORM, 0, 4, 1, 2}}};
constexpr FormatTypes layoutY8 = {{{GMM_FORMAT_Y8_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_Y8_UNORM, 0, 1, 1, 1},
                                   {GMM_FORMAT_Y8_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_Y8_UNORM, 0, 1, 1, 1},
                                   {GMM_FORMAT_Y8_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_Y8_UNORM, 0, 1, 1, 1},
                                   {GMM_FORMAT_Y8_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_Y8_UNORM, 0, 1, 1, 1},
                                   {GMM_FORMAT_Y8_UNORM, NEO::GFX3DSTATE_SURFACEFORMAT_Y8_UNORM, 0, 1, 1, 1}}};
constexpr FormatTypes layoutNV12 = {{{GMM_FORMAT_NV12, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1},
                                     {GMM_FORMAT_NV12, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1},
                                     {GMM_FORMAT_NV12, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1},
                                     {GMM_FORMAT_NV12, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1},
                                     {GMM_FORMAT_NV12, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1}}};
constexpr FormatTypes layoutYUYV = {{{GMM_FORMAT_YCRCB_NORMAL, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_NORMAL, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_NORMAL, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_NORMAL, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_NORMAL, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_NORMAL, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_NORMAL, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_NORMAL, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_NORMAL, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_NORMAL, 0, 2, 1, 2}}};
constexpr FormatTypes layoutVYUY = {{{GMM_FORMAT_YCRCB_SWAPUVY, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUVY, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_SWAPUVY, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUVY, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_SWAPUVY, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUVY, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_SWAPUVY, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUVY, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_SWAPUVY, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUVY, 0, 2, 1, 2}}};
constexpr FormatTypes layoutYVYU = {{{GMM_FORMAT_YCRCB_SWAPUV, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUV, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_SWAPUV, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUV, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_SWAPUV, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUV, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_SWAPUV, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUV, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_SWAPUV, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPUV, 0, 2, 1, 2}}};
constexpr FormatTypes layoutUYVY = {{{GMM_FORMAT_YCRCB_SWAPY, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPY, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_SWAPY, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPY, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_SWAPY, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPY, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_SWAPY, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPY, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_SWAPY, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_SWAPY, 0, 2, 1, 2}}};
constexpr FormatTypes layoutAYUV = {{{GMM_FORMAT_AYUV, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM, 0, 4, 1, 4},
                                     {GMM_FORMAT_AYUV, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM, 0, 4, 1, 4},
                                     {GMM_FORMAT_AYUV, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM, 0, 4, 1, 4},
                                     {GMM_FORMAT_AYUV, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM, 0, 4, 1, 4},
                                     {GMM_FORMAT_AYUV, NEO::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM, 0, 4, 1, 4}}};
constexpr FormatTypes layoutY410 = {{{GMM_FORMAT_Y410, NEO::GFX3DSTATE_SURFACEFORMAT_R10G10B10A2_UNORM, 0, 4, 1, 4},
                                     {GMM_FORMAT_Y410, NEO::GFX3DSTATE_SURFACEFORMAT_R10G10B10A2_UNORM, 0, 4, 1, 4},
                                     {GMM_FORMAT_Y410, NEO::GFX3DSTATE_SURFACEFORMAT_R10G10B10A2_UNORM, 0, 4, 1, 4},
                                     {GMM_FORMAT_Y410, NEO::GFX3DSTATE_SURFACEFORMAT_R10G10B10A2_UNORM, 0, 4, 1, 4},
                                     {GMM_FORMAT_Y410, NEO::GFX3DSTATE_SURFACEFORMAT_R10G10B10A2_UNORM, 0, 4, 1, 4}}};
constexpr FormatTypes layoutY16 = {{{GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                    {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                    {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                    {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                    {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1}}};
constexpr FormatTypes layoutP010 = {{{GMM_FORMAT_P010, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1},
                                     {GMM_FORMAT_P010, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1},
                                     {GMM_FORMAT_P010, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1},
                                     {GMM_FORMAT_P010, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1},
                                     {GMM_FORMAT_P010, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1}}};
constexpr FormatTypes layoutP012 = {{{GMM_FORMAT_P012, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1},
                                     {GMM_FORMAT_P012, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1},
                                     {GMM_FORMAT_P012, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1},
                                     {GMM_FORMAT_P012, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1},
                                     {GMM_FORMAT_P012, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1}}};
constexpr FormatTypes layoutP016 = {{{GMM_FORMAT_P016, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1},
                                     {GMM_FORMAT_P016, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1},
                                     {GMM_FORMAT_P016, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1},
                                     {GMM_FORMAT_P016, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1},
                                     {GMM_FORMAT_P016, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_16, 0, 2, 1, 1}}};
constexpr FormatTypes layoutY216 = {{{GMM_FORMAT_Y216, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8},
                                     {GMM_FORMAT_Y216, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8},
                                     {GMM_FORMAT_Y216, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8},
                                     {GMM_FORMAT_Y216, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8},
                                     {GMM_FORMAT_Y216, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8}}};
constexpr FormatTypes layoutP216 = {{{GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1}}};
constexpr FormatTypes layoutP8 = {{{GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                   {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                   {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                   {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                   {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1}}};
constexpr FormatTypes layoutYUY2 = {{{GMM_FORMAT_YCRCB_NORMAL, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_NORMAL, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_NORMAL, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_NORMAL, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_NORMAL, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_NORMAL, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_NORMAL, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_NORMAL, 0, 2, 1, 2},
                                     {GMM_FORMAT_YCRCB_NORMAL, NEO::GFX3DSTATE_SURFACEFORMAT_YCRCB_NORMAL, 0, 2, 1, 2}}};
constexpr FormatTypes layoutA8P8 = {{{GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1}}};
constexpr FormatTypes layoutIA44 = {{{GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1}}};
constexpr FormatTypes layoutAI44 = {{{GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1}}};
constexpr FormatTypes layoutY416 = {{{GMM_FORMAT_Y416, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8},
                                     {GMM_FORMAT_Y416, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8},
                                     {GMM_FORMAT_Y416, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8},
                                     {GMM_FORMAT_Y416, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8},
                                     {GMM_FORMAT_Y416, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8}}};
constexpr FormatTypes layoutY210 = {{{GMM_FORMAT_Y210, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8},
                                     {GMM_FORMAT_Y210, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8},
                                     {GMM_FORMAT_Y210, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8},
                                     {GMM_FORMAT_Y210, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8},
                                     {GMM_FORMAT_Y210, NEO::GFX3DSTATE_SURFACEFORMAT_R16G16B16A16_UNORM, 0, 4, 2, 8}}};
constexpr FormatTypes layoutI420 = {{{GMM_FORMAT_I420, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1},
                                     {GMM_FORMAT_I420, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1},
                                     {GMM_FORMAT_I420, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1},
                                     {GMM_FORMAT_I420, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1},
                                     {GMM_FORMAT_I420, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1}}};
constexpr FormatTypes layoutYV12 = {{{GMM_FORMAT_YV12, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1},
                                     {GMM_FORMAT_YV12, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1},
                                     {GMM_FORMAT_YV12, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1},
                                     {GMM_FORMAT_YV12, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1},
                                     {GMM_FORMAT_YV12, NEO::GFX3DSTATE_SURFACEFORMAT_PLANAR_420_8, 0, 1, 1, 1}}};
constexpr FormatTypes layout400P = {{{GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1}}};
constexpr FormatTypes layout422H = {{{GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1}}};
constexpr FormatTypes layout422V = {{{GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1}}};
constexpr FormatTypes layout444P = {{{GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1},
                                     {GMM_FORMAT_INVALID, NEO::NUM_GFX3DSTATE_SURFACEFORMATS, 0, 1, 1, 1}}};
constexpr FormatTypes layoutRGBP = {{{GMM_FORMAT_RGBP, NEO::GFX3DSTATE_SURFACEFORMAT_R8_UINT, 0, 3, 1, 3},
                                     {GMM_FORMAT_RGBP, NEO::GFX3DSTATE_SURFACEFORMAT_R8_UINT, 0, 3, 1, 3},
                                     {GMM_FORMAT_RGBP, NEO::GFX3DSTATE_SURFACEFORMAT_R8_UINT, 0, 3, 1, 3},
                                     {GMM_FORMAT_RGBP, NEO::GFX3DSTATE_SURFACEFORMAT_R8_UINT, 0, 3, 1, 3},
                                     {GMM_FORMAT_RGBP, NEO::GFX3DSTATE_SURFACEFORMAT_R8_UINT, 0, 3, 1, 3}}};
constexpr FormatTypes layoutBGRP = {{{GMM_FORMAT_BGRP, NEO::GFX3DSTATE_SURFACEFORMAT_R8_UINT, 0, 3, 1, 3},
                                     {GMM_FORMAT_BGRP, NEO::GFX3DSTATE_SURFACEFORMAT_R8_UINT, 0, 3, 1, 3},
                                     {GMM_FORMAT_BGRP, NEO::GFX3DSTATE_SURFACEFORMAT_R8_UINT, 0, 3, 1, 3},
                                     {GMM_FORMAT_BGRP, NEO::GFX3DSTATE_SURFACEFORMAT_R8_UINT, 0, 3, 1, 3},
                                     {GMM_FORMAT_BGRP, NEO::GFX3DSTATE_SURFACEFORMAT_R8_UINT, 0, 3, 1, 3}}};

constexpr std::array<FormatTypes, ZE_IMAGE_FORMAT_RENDER_LAYOUT_MAX> formats = {layout8, layout16, layout32, layout88, layout8888, layout1616, layout16161616, layout3232, layout32323232, layout1010102,
                                                                                layout111110, layout565, layout5551, layout4444, layoutY8, layoutNV12, layoutYUYV, layoutVYUY, layoutYVYU, layoutUYVY,
                                                                                layoutAYUV, layoutP010, layoutY410, layoutP012, layoutY16, layoutP016, layoutY216, layoutP216, layoutP8, layoutYUY2,
                                                                                layoutA8P8, layoutIA44, layoutAI44, layoutY416, layoutY210, layoutI420, layoutYV12, layout400P, layout422H, layout422V,
                                                                                layout444P, layoutRGBP, layoutBGRP};
} // namespace ImageFormats
} // namespace L0
