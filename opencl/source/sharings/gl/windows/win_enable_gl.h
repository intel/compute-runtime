/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/source/sharings/gl/windows/gl_sharing_windows.h"
#include "opencl/source/sharings/sharing_factory.h"

#include <memory>

namespace NEO {
class Context;

struct GlCreateContextProperties {
    GLType glHDCType = 0;
    GLContext glHGLRCHandle = 0;
    GLDisplay glHDCHandle = 0;
};

class GlSharingContextBuilder : public SharingContextBuilder {
  protected:
    std::unique_ptr<GlCreateContextProperties> contextData;

  public:
    bool processProperties(cl_context_properties &propertyType, cl_context_properties &propertyValue) override;
    bool finalizeProperties(Context &context, int32_t &errcodeRet) override;
};

class GlSharingBuilderFactory : public SharingBuilderFactory {
  public:
    std::unique_ptr<SharingContextBuilder> createContextBuilder() override;
    std::string getExtensions(DriverInfo *driverInfo) override;
    void fillGlobalDispatchTable() override;
    void *getExtensionFunctionAddress(const std::string &functionName) override;
};
} // namespace NEO
