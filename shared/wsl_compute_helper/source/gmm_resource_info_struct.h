/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

// gmm enforces include order
// clang-format off
#include "shared/source/os_interface/windows/windows_wrapper.h"
#ifdef WDDM_LINUX
#  include "umKmInc/sharedata.h"
#  ifdef LHDM
#    undef LHDM
#    define RESTORE_LHDM
#  endif
#  undef WDDM_LINUX
#  define RESTORE_WDDM_LINUX
#  define UFO_PORTABLE_COMPILER_H
#  define UFO_PORTABLE_WINDEF_H
#endif
#include "umKmInc/sharedata.h"

#include "External/Common/GmmCommonExt.h"
#include "External/Common/GmmUtil.h"
#include "External/Common/GmmResourceFlags.h"
#include "External/Common/GmmCachePolicy.h"
#include "External/Common/GmmCachePolicyExt.h"
#include "External/Common/GmmResourceInfoExt.h"
#include "External/Common/GmmPlatformExt.h"
#include "External/Common/GmmTextureExt.h"
#include "External/Common/GmmInfoExt.h"
#include "External/Common/GmmResourceInfo.h"
// clang-format on

#ifdef RESTORE_WDDM_LINUX
#ifdef RESTORE_LHDM
#define LHDM 1
#endif
#define WDDM_LINUX 1
#ifndef GMM_ESCAPE_HANDLE
#define GMM_ESCAPE_HANDLE D3DKMT_HANDLE
#endif
#ifndef GMM_ESCAPE_FUNC_TYPE
#define GMM_ESCAPE_FUNC_TYPE PFND3DKMT_ESCAPE
#endif
#ifndef GMM_HANDLE
#define GMM_HANDLE D3DKMT_HANDLE
#endif
#endif // RESTORE_WDDM_LINUX

struct GmmResourceInfoCommonStruct {
    GMM_CLIENT ClientType;
    GMM_TEXTURE_INFO Surf;
    GMM_TEXTURE_INFO AuxSurf;
    GMM_TEXTURE_INFO AuxSecSurf;

    uint32_t RotateInfo;
    GMM_EXISTING_SYS_MEM ExistingSysMem;
    GMM_GFX_ADDRESS SvmAddress;

    uint64_t pPrivateData;

    GMM_MULTI_TILE_ARCH MultiTileArch;
};

struct GmmResourceInfoWinStruct {
    struct GmmResourceInfoCommonStruct GmmResourceInfoCommon;
};
