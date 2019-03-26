/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/sharings/gl/gl_sharing.h"
#include "runtime/sharings/sharing_factory.h"

#include <memory>

namespace NEO {
class Context;

struct GlCreateContextProperties {
    GLType GLHDCType = 0;
    GLContext GLHGLRCHandle = 0;
    GLDisplay GLHDCHandle = 0;
};

class GlSharingContextBuilder : public SharingContextBuilder {
  protected:
    std::unique_ptr<GlCreateContextProperties> contextData;

  public:
    bool processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue, cl_int &errcodeRet) override;
    bool finalizeProperties(Context &context, int32_t &errcodeRet) override;
};

class GlSharingBuilderFactory : public SharingBuilderFactory {
  public:
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override;
    std::string getExtensions() override;
    void fillGlobalDispatchTable() override;
    void *getExtensionFunctionAddress(const std::string &functionName) override;
};
} // namespace NEO
