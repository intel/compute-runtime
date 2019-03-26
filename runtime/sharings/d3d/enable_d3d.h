/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/sharings/sharing_factory.h"

#include <memory>

namespace NEO {
class Context;

template <typename D3D>
struct D3DCreateContextProperties {
    typename D3D::D3DDevice *pDevice = nullptr;
    bool argumentsDefined = false;
};

template <typename D3D>
class D3DSharingContextBuilder : public SharingContextBuilder {
  protected:
    std::unique_ptr<D3DCreateContextProperties<D3D>> contextData;

  public:
    bool processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet) override;
    bool finalizeProperties(Context &context, int32_t &errcodeRet) override;
};

template <typename D3D>
class D3DSharingBuilderFactory : public SharingBuilderFactory {
  public:
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override;
    std::string getExtensions() override;
    void fillGlobalDispatchTable() override;
    void *getExtensionFunctionAddress(const std::string &functionName) override;
};
} // namespace NEO