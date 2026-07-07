/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_library.h"

#include "level_zero/api/opencl/source/sharings/leo_sharing.h"

#include "CL/cl.h"
#include "CL/cl_gl.h"
#include "leo_gl_types.h"

#include <functional>
#include <mutex>
#include <unordered_map>

struct _tagCLGLSyncInfo;
typedef struct _tagCLGLSyncInfo CL_GL_SYNC_INFO, *PCL_GL_SYNC_INFO;

namespace NEO {
class DriverModel;
class OSInterface;
class OsContext;
} // namespace NEO

namespace NEO {
namespace LEO {

class GLSharingFunctions;

using OSInterface = NEO::OSInterface;
using OsContext = NEO::OsContext;

typedef unsigned int OS_HANDLE;

typedef struct CLGLContextInfo {
    OS_HANDLE deviceHandle;
    OS_HANDLE contextHandle;
} ContextInfo;

class GLSharingFunctions : public SharingFunctions {
  public:
    GLSharingFunctions() = default;

    uint32_t getId() const override {
        return GLSharingFunctions::sharingId;
    }
    static const uint32_t sharingId;

    static cl_int getSupportedFormats(cl_mem_flags flags,
                                      cl_mem_object_type imageType,
                                      size_t numEntries,
                                      cl_GLenum *formats,
                                      uint32_t *numImageFormats);

    static std::unique_ptr<GLSharingFunctions> create();
    virtual bool isHandleCompatible(const NEO::DriverModel &driverModel, uint32_t handle) const { return true; }
    virtual bool isGlHdcHandleMissing(uint32_t handle) const { return false; }
    virtual GLboolean initGLFunctions() = 0;
    virtual bool isOpenGlSharingSupported() = 0;
};

class GlSharing : public SharingHandler {
  public:
    GlSharing(GLSharingFunctions *sharingFunctions, unsigned int glObjectType, unsigned int glObjectId)
        : sharingFunctions(sharingFunctions), clGlObjectType(glObjectType), clGlObjectId(glObjectId) {};
    GLSharingFunctions *peekFunctionsHandler() { return sharingFunctions; }
    void getGlObjectInfo(unsigned int *pClGlObjectType, unsigned int *pClGlObjectId) {
        if (pClGlObjectType) {
            *pClGlObjectType = clGlObjectType;
        }
        if (pClGlObjectId) {
            *pClGlObjectId = clGlObjectId;
        }
    }
    static const std::unordered_map<GLenum, const cl_image_format> glToCLFormats;

  protected:
    int synchronizeHandler(UpdateData &updateData) override;
    GLSharingFunctions *sharingFunctions = nullptr;
    unsigned int clGlObjectType = 0u;
    unsigned int clGlObjectId = 0u;
};

} // namespace LEO
} // namespace NEO
