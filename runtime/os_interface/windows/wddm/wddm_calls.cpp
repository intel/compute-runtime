/*
 * Copyright (c) 2018, Intel Corporation
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

#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/os_interface/windows/wddm/wddm.h"
#include <dxgi.h>

namespace OCLRT {
Wddm::CreateDXGIFactoryFcn getCreateDxgiFactory() {
    return CreateDXGIFactory;
}

Wddm::GetSystemInfoFcn getGetSystemInfo() {
    return GetSystemInfo;
}

Wddm::VirtualFreeFcn getVirtualFree() {
    return VirtualFree;
}

Wddm::VirtualAllocFcn getVirtualAlloc() {
    return VirtualAlloc;
}

bool Wddm::initGmmContext() {
    return GmmHelper::initContext(gfxPlatform.get(),
                                  featureTable.get(),
                                  waTable.get(),
                                  gtSystemInfo.get());
}

void Wddm::destroyGmmContext() {
    GmmHelper::destroyContext();
}
} // namespace OCLRT
