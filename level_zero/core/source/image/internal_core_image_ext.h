/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/memory_manager/allocation_type.h"

#include <level_zero/ze_api.h>

namespace L0 {

// NOLINTBEGIN(readability-identifier-naming)

constexpr ze_structure_type_t ZE_STRUCTURE_TYPE_IMAGE_ALLOCATION_TYPE_EXT_DESC = static_cast<ze_structure_type_t>(0x00050000);

struct ze_image_allocation_type_ext_desc_t {
    ze_structure_type_t stype = ZE_STRUCTURE_TYPE_IMAGE_ALLOCATION_TYPE_EXT_DESC;
    const void *pNext = nullptr;
    NEO::AllocationType allocationType = NEO::AllocationType::unknown;
};

constexpr ze_structure_type_t ZE_STRUCTURE_TYPE_EXTERNAL_GL_TEXTURE_EXT_DESC = static_cast<ze_structure_type_t>(0x00050001);

struct ze_external_gl_texture_ext_desc_t {
    ze_structure_type_t stype = ZE_STRUCTURE_TYPE_EXTERNAL_GL_TEXTURE_EXT_DESC;
    const void *pNext = nullptr;
    void *pGmmResInfo = nullptr;      // GMM_RESOURCE_INFO* obtained from the GL driver
    uint64_t textureBufferOffset = 0; // byte offset of the texture data within the shared allocation
    uint32_t glHWFormat = 0;          // genx surface-state format override (0 means no override)
    uint32_t cubeFaceIndex = 0;       // GMM cube face index (__GMM_NO_CUBE_MAP for non-cube targets)
    bool isAuxEnabled = false;        // request aux/compression mapping

    // Multisample / MCS sharing (cl_khr_gl_msaa_sharing)
    uint32_t numberOfSamples = 0;      // GL texture sample count (>1 for MSAA)
    void *mcsNtHandle = nullptr;       // NT handle of the MCS / auxiliary surface (nullptr if unified)
    void *pGmmResInfoMcs = nullptr;    // GMM_RESOURCE_INFO* of the MCS surface
    bool hasUnifiedMcsSurface = false; // MCS lives inside the main allocation (unified aux)
};

// Extended image region that carries a mip-level index.  Inherits from
// ze_image_region_t so a pointer to this struct is implicitly convertible to
// const ze_image_region_t* and can be passed directly to any L0 image-copy
// entrypoint.  The L0 implementation recovers the mip level by casting back
// when the image is mipmapped (numMipLevels > 1).
struct ze_image_region_mip_level_exp_desc_t : ze_image_region_t {
    uint32_t mipLevel = 0;
};

inline uint32_t getRegionMipLevel(const ze_image_region_t *pRegion, uint32_t numMipLevels) {
    if (pRegion == nullptr || numMipLevels <= 1u) {
        return 0u;
    }
    return static_cast<const ze_image_region_mip_level_exp_desc_t *>(pRegion)->mipLevel;
}

constexpr ze_structure_type_t ZE_STRUCTURE_TYPE_SAMPLER_LOD_EXT_DESC = static_cast<ze_structure_type_t>(0x00050003);

struct ze_sampler_lod_ext_desc_t {
    ze_structure_type_t stype = ZE_STRUCTURE_TYPE_SAMPLER_LOD_EXT_DESC;
    const void *pNext = nullptr;
    float lodMin = 0.0f;
    float lodMax = 0.0f;
    bool mipFilterLinear = false;
};

// NOLINTEND(readability-identifier-naming)

} // namespace L0
