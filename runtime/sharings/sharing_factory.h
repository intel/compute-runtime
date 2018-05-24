/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#include "CL/cl.h"

#include <memory>
#include <string>
#include <vector>

namespace OCLRT {
class Context;

enum SharingType {
    CLGL_SHARING = 0,
    VA_SHARING = 1,
    D3D9_SHARING = 2,
    D3D10_SHARING = 3,
    D3D11_SHARING = 4,
    MAX_SHARING_VALUE = 5
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
    virtual void fillGlobalDispatchTable() = 0;
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
}
