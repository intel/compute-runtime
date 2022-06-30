/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/adapter_factory.h"
#include "shared/source/os_interface/windows/wddm/adapter_factory_dxcore.h"

namespace NEO {

std::unique_ptr<AdapterFactory> AdapterFactory::create(AdapterFactory::CreateAdapterFactoryFcn dxCoreCreateAdapterFactoryF,
                                                       AdapterFactory::CreateAdapterFactoryFcn dxgiCreateAdapterFactoryF) {
    return std::make_unique<DxCoreAdapterFactory>(dxCoreCreateAdapterFactoryF);
}

} // namespace NEO
