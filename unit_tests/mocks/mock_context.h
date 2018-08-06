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
#include "runtime/context/context.h"
#include "runtime/sharings/sharing_factory.h"
#include <memory>

namespace OCLRT {
class MockContext : public Context {
  public:
    using Context::sharingFunctions;

    MockContext(Device *device, bool noSpecialQueue = false);
    MockContext(
        void(CL_CALLBACK *funcNotify)(const char *, const void *, size_t, void *),
        void *data);
    MockContext();
    ~MockContext();

    void setMemoryManager(MemoryManager *mm) {
        memoryManager = mm;
    }

    void clearSharingFunctions();
    void setSharingFunctions(SharingFunctions *sharingFunctions);
    void releaseSharingFunctions(SharingType sharing);
    void registerSharingWithId(SharingFunctions *sharing, SharingType sharingId);

    cl_bool peekPreferD3dSharedResources() { return preferD3dSharedResources; }

    void forcePreferD3dSharedResources(cl_bool value) { preferD3dSharedResources = value; }
    DriverDiagnostics *getDriverDiagnostics() { return this->driverDiagnostics; }

  private:
    Device *device;
};
} // namespace OCLRT
