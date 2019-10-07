/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "CL/cl.h"

#include <memory>
#include <string>
#include <vector>

namespace NEO {
class Context;

enum SharingType {
    CLGL_SHARING = 0,
    VA_SHARING = 1,
    D3D9_SHARING = 2,
    D3D10_SHARING = 3,
    D3D11_SHARING = 4,
    UNIFIED_SHARING = 5,
    MAX_SHARING_VALUE = 6
};

class SharingContextBuilder {
  public:
    virtual ~SharingContextBuilder() = default;
    virtual bool processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet) = 0;
    virtual bool finalizeProperties(Context &context, int32_t &errcodeRet) = 0;
};

class SharingBuilderFactory {
  public:
    virtual ~SharingBuilderFactory() = default;
    virtual std::unique_ptr<SharingContextBuilder> createContextBuilder() = 0;
    virtual std::string getExtensions() = 0;
    virtual void fillGlobalDispatchTable() {}
    virtual void *getExtensionFunctionAddress(const std::string &functionName) = 0;
};

class SharingFactory {
  protected:
    static SharingBuilderFactory *sharingContextBuilder[SharingType::MAX_SHARING_VALUE];
    std::vector<std::unique_ptr<SharingContextBuilder>> sharings;

  public:
    template <typename F, typename T>
    class RegisterSharing {
      public:
        RegisterSharing();
    };

    static std::unique_ptr<SharingFactory> build();
    bool processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet);
    bool finalizeProperties(Context &context, int32_t &errcodeRet);
    std::string getExtensions();
    void fillGlobalDispatchTable();
    void *getExtensionFunctionAddress(const std::string &functionName);
};

extern SharingFactory sharingFactory;
} // namespace NEO
