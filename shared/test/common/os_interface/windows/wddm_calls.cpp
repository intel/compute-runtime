/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/host_ptr_defines.h"
#include "shared/test/common/mocks/mock_wddm.h"
#include "shared/test/common/os_interface/windows/ult_dxcore_factory.h"
#include "shared/test/common/os_interface/windows/ult_dxgi_factory.h"

namespace NEO {
Wddm::CreateDXGIFactoryFcn getCreateDxgiFactory() {
    return ULTCreateDXGIFactory;
}

Wddm::DXCoreCreateAdapterFactoryFcn getDXCoreCreateAdapterFactory() {
    return ultDxCoreCreateAdapterFactory;
}

Wddm::GetSystemInfoFcn getGetSystemInfo() {
    return ultGetSystemInfo;
}
} // namespace NEO
