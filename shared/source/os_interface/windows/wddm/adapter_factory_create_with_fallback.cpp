/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/adapter_factory.h"
#include "shared/source/os_interface/windows/wddm/adapter_factory_dxcore.h"
#include "shared/source/os_interface/windows/wddm/adapter_factory_dxgi.h"

namespace NEO {

std::unique_ptr<AdapterFactory> AdapterFactory::create(AdapterFactory::CreateAdapterFactoryFcn dxCoreCreateAdapterFactoryF,
                                                       AdapterFactory::CreateAdapterFactoryFcn dxgiCreateAdapterFactoryF) {
    std::unique_ptr<AdapterFactory> ret;
    ret = std::make_unique<DxCoreAdapterFactory>(dxCoreCreateAdapterFactoryF);
    if (false == ret->isSupported()) {
        ret = std::make_unique<DxgiAdapterFactory>(dxgiCreateAdapterFactoryF);
    }
    return ret;
}

} // namespace NEO
