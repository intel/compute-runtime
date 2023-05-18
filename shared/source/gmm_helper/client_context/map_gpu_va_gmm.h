/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm_lib.h"

namespace NEO {
class Gdi;

class MapGpuVirtualAddressGmm {
  public:
    MapGpuVirtualAddressGmm(D3DDDI_MAPGPUVIRTUALADDRESS *mapGpuVirtualAddressParams, GMM_RESOURCE_INFO **resourceInfoHandle, D3DGPU_VIRTUAL_ADDRESS *outVirtualAddress, Gdi *gdi) : mapGpuVirtualAddressParams(mapGpuVirtualAddressParams), resourceInfoHandle(resourceInfoHandle), outVirtualAddress(outVirtualAddress), gdi(gdi) {}
    D3DDDI_MAPGPUVIRTUALADDRESS *mapGpuVirtualAddressParams;
    GMM_RESOURCE_INFO **resourceInfoHandle;
    D3DGPU_VIRTUAL_ADDRESS *outVirtualAddress;
    Gdi *gdi;
};

class FreeGpuVirtualAddressGmm {
  public:
    FreeGpuVirtualAddressGmm(D3DKMT_HANDLE hAdapter, D3DGPU_VIRTUAL_ADDRESS baseAddress, D3DGPU_SIZE_T size, GMM_RESOURCE_INFO **resourceInfoHandle, Gdi *gdi) : hAdapter(hAdapter), baseAddress(baseAddress), size(size), resourceInfoHandle(resourceInfoHandle), gdi(gdi) {}
    D3DKMT_HANDLE hAdapter;
    D3DGPU_VIRTUAL_ADDRESS baseAddress;
    D3DGPU_SIZE_T size;
    GMM_RESOURCE_INFO **resourceInfoHandle;
    Gdi *gdi;
};

} // namespace NEO