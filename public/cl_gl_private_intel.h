/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef OPENCL_SHARED_RESOURCE
#define OPENCL_SHARED_RESOURCE

#include "GL/gl.h"
#include "GmmLib.h"
#include "third_party/opencl_headers/CL/cl_gl.h"

// Used for creating CL resources from GL resources
typedef struct _tagCLGLResourceInfo {
    GLuint name;
    GLenum target;
    unsigned int globalShareHandle;
    GMM_RESOURCE_INFO *pGmmResInfo; /// Pointer to GMMResInfo from GL that will be copied in CL (GL)
    GLenum glFormat;
    GLint glInternalFormat;
    GLuint glHWFormat;
    GLboolean isEmulatedTarget;
    GLuint borderWidth;
    GLint textureBufferWidth;
    GLint textureBufferSize;
    GLint textureBufferOffset;
    GLboolean oglSynchronized;
    GMM_STATUS status;
    unsigned int globalShareHandleMCS;
    GMM_RESOURCE_INFO *pGmmResInfoMCS;
    GLint numberOfSamples; // Number of samples as specified by API
    GLvoid *pReleaseData;
} CL_GL_RESOURCE_INFO, *PCL_GL_RESOURCE_INFO;

// Used for creating GL resources from CL resources
typedef struct _tagGLCLResourceInfo {
    unsigned int globalShareHandle;
    unsigned int clChannelOrder;
    unsigned int clChannelDataType;
    size_t imageWidth;
    size_t imageHeight;
    size_t rowPitch;
    size_t slicePitch;
    unsigned int mipCount;
    bool isCreatedFromBuffer;
    unsigned int arraySize;
    unsigned int depth;
} GL_CL_RESOURCE_INFO, *PGL_CL_RESOURCE_INFO;

typedef struct _tagCLGLBufferInfo {
    GLenum bufferName;
    unsigned int globalShareHandle;
    GMM_RESOURCE_INFO *pGmmResInfo; /// Pointer to GMMResInfo from GL that will be copied in CL (GL)
    GLvoid *pSysMem;
    GLint bufferSize;
    GLint bufferOffset;
    GLboolean oglSynchronized;
    GMM_STATUS status;
    GLvoid *pReleaseData;
} CL_GL_BUFFER_INFO, *PCL_GL_BUFFER_INFO;

#ifdef _WIN32
// Used for creating GL sync objects from CL events
typedef struct _tagCLGLSyncInfo {

    _tagCLGLSyncInfo()
        : eventName(NULL),
          event((HANDLE)0),
          submissionEventName(NULL),
          submissionEvent((HANDLE)0),
          clientSynchronizationObject((D3DKMT_HANDLE)0),
          serverSynchronizationObject((D3DKMT_HANDLE)0),
          submissionSynchronizationObject((D3DKMT_HANDLE)0),
          hContextToBlock((D3DKMT_HANDLE)0),
          waitCalled(false) {
    }

    char *eventName;
    HANDLE event;
    char *submissionEventName;
    HANDLE submissionEvent;
    D3DKMT_HANDLE clientSynchronizationObject;
    D3DKMT_HANDLE serverSynchronizationObject;
    D3DKMT_HANDLE submissionSynchronizationObject;
    D3DKMT_HANDLE hContextToBlock;
    bool waitCalled;
} CL_GL_SYNC_INFO, *PCL_GL_SYNC_INFO;

// Used for creating CL events from GL sync objects
typedef struct _tagGLCLSyncInfo {
    __GLsync *syncName;
    GLvoid *pSync;
} GL_CL_SYNC_INFO, *PGL_CL_SYNC_INFO;
#endif

typedef int(__stdcall *pfn_clRetainEvent)(struct _cl_event *event);
typedef int(__stdcall *pfn_clReleaseEvent)(struct _cl_event *event);
typedef int(__stdcall *INTELpfn_clGetCLObjectInfoINTEL)(struct _cl_mem *pMemObj, void *pResourceInfo);
typedef int(__stdcall *INTELpfn_clEnqueueMarkerWithSyncObjectINTEL)(
    struct _cl_command_queue *pCommandQueue,
    struct _cl_event **pOclEvent,
    struct _cl_context **pOclContext);

typedef struct _tagCLGLDispatch {
    pfn_clRetainEvent clRetainEvent;
    pfn_clReleaseEvent clReleaseEvent;
    INTELpfn_clGetCLObjectInfoINTEL clGetCLObjectInfoINTEL;
    INTELpfn_clEnqueueMarkerWithSyncObjectINTEL clEnqueueMarkerWithSyncObjectINTEL;
} CL_GL_DISPATCH, *PCL_GL_DISPATCH;

#ifdef _WIN32
typedef struct _tagCLGLContextInfo {
    D3DKMT_HANDLE DeviceHandle;
    D3DKMT_HANDLE ContextHandle;
} CL_GL_CONTEXT_INFO, *PCL_GL_CONTEXT_INFO;

typedef struct _tagCLGLEvent {
    struct
    {
        void *dispatch1;
        void *dispatch2;
    } dispatch;
    void *pObj;
    void *CLCmdQ;
    struct _cl_context *CLCtx;
    unsigned int IsUserEvent;
    PCL_GL_SYNC_INFO pSyncInfo;
} CL_GL_EVENT, *PCL_GL_EVENT;
#endif //_WIN32
#endif
