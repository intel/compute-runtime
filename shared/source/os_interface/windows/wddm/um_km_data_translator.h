/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/os_interface/windows/sharedata_wrapper.h"

#include <memory>

typedef unsigned int D3DKMT_HANDLE;

namespace NEO {

class Gdi;
class GmmHandleAllocator;

class UmKmDataTranslator {
  public:
    virtual ~UmKmDataTranslator() = default;

    virtual size_t getSizeForAdapterInfoInternalRepresentation();
    virtual bool translateAdapterInfoFromInternalRepresentation(ADAPTER_INFO_KMD &dst, const void *src, size_t srcSize);

    virtual size_t getSizeForCreateContextDataInternalRepresentation();
    virtual bool translateCreateContextDataToInternalRepresentation(void *dst, size_t dstSize, const CREATECONTEXT_PVTDATA &src);

    virtual size_t getSizeForCommandBufferHeaderDataInternalRepresentation();
    virtual bool translateCommandBufferHeaderDataToInternalRepresentation(void *dst, size_t dstSize, const COMMAND_BUFFER_HEADER &src);

    virtual size_t getSizeForGmmGfxPartitioningInternalRepresentation();
    virtual bool translateGmmGfxPartitioningToInternalRepresentation(void *dst, size_t dstSize, const GMM_GFX_PARTITIONING &src);
    virtual bool translateGmmGfxPartitioningFromInternalRepresentation(GMM_GFX_PARTITIONING &dst, const void *src, size_t srcSize);

    bool enabled() const {
        return isEnabled;
    }

    virtual std::unique_ptr<GmmHandleAllocator> createGmmHandleAllocator();

  protected:
    bool isEnabled = false;
};

std::unique_ptr<UmKmDataTranslator> createUmKmDataTranslator(const Gdi &gdi, D3DKMT_HANDLE adapter);

} // namespace NEO
