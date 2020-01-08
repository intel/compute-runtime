/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "core/os_interface/os_library.h"
#include "runtime/sharings/sharing.h"

#include "CL/cl.h"
#include "GL/gl.h"
#include "GL/glext.h"

#include <functional>
#include <mutex>
#include <unordered_map>

struct _tagCLGLSyncInfo;
typedef struct _tagCLGLSyncInfo CL_GL_SYNC_INFO, *PCL_GL_SYNC_INFO;

namespace NEO {
class Event;
class GlArbSyncEvent;
class GLSharingFunctions;
class OSInterface;
class OsContext;

typedef unsigned int OS_HANDLE;

typedef struct CLGLContextInfo {
    OS_HANDLE DeviceHandle;
    OS_HANDLE ContextHandle;
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

    virtual GLboolean initGLFunctions() = 0;
    virtual bool isOpenGlSharingSupported() = 0;
};

class GlSharing : public SharingHandler {
  public:
    GlSharing(GLSharingFunctions *sharingFunctions, unsigned int glObjectType, unsigned int glObjectId)
        : sharingFunctions(sharingFunctions), clGlObjectType(glObjectType), clGlObjectId(glObjectId){};
    GLSharingFunctions *peekFunctionsHandler() { return sharingFunctions; }
    void getGlObjectInfo(unsigned int *pClGlObjectType, unsigned int *pClGlObjectId) {
        if (pClGlObjectType) {
            *pClGlObjectType = clGlObjectType;
        }
        if (pClGlObjectId) {
            *pClGlObjectId = clGlObjectId;
        }
    }
    static const std::unordered_map<GLenum, const cl_image_format> gLToCLFormats;

  protected:
    int synchronizeHandler(UpdateData &updateData) override;
    GLSharingFunctions *sharingFunctions = nullptr;
    unsigned int clGlObjectType = 0u;
    unsigned int clGlObjectId = 0u;
};

} // namespace NEO
