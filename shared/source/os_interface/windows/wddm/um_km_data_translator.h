/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/gmm_helper/client_context/gmm_handle_allocator.h"
#include "shared/source/os_interface/windows/sharedata_wrapper.h"
#include "shared/source/utilities/stackvec.h"

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

    virtual std::unique_ptr<GmmHandleAllocator> createGmmHandleAllocator() {
        return {};
    }

  protected:
    bool isEnabled = false;
};

template <size_t StaticSize>
struct UmKmDataTempStorageBase {
    UmKmDataTempStorageBase() = default;
    UmKmDataTempStorageBase(size_t dynSize) {
        this->resize(dynSize);
    }

    void *data() {
        return storage.data();
    }

    void resize(size_t dynSize) {
        auto oldSize = storage.size() * sizeof(uint64_t);
        storage.resize((dynSize + sizeof(uint64_t) - 1) / sizeof(uint64_t));
        requestedSize = dynSize;
        memset(reinterpret_cast<char *>(data()) + oldSize, 0, storage.size() * sizeof(uint64_t) - oldSize);
    }

    size_t size() const {
        return requestedSize;
    }

  protected:
    static constexpr size_t staticSizeQwordsCount = (StaticSize + sizeof(uint64_t) - 1) / sizeof(uint64_t);
    StackVec<uint64_t, staticSizeQwordsCount> storage;
    size_t requestedSize = 0U;
};

template <typename SrcT, size_t OverestimateMul = 2, typename BaseT = UmKmDataTempStorageBase<sizeof(SrcT) * OverestimateMul>>
struct UmKmDataTempStorage : BaseT {
    using BaseT::BaseT;
};

std::unique_ptr<UmKmDataTranslator> createUmKmDataTranslator(const Gdi &gdi, D3DKMT_HANDLE adapter);

} // namespace NEO
