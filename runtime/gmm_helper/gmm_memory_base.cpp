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

#include "gmm_client_context.h"
#include "runtime/gmm_helper/gmm_memory_base.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/os_interface/windows/windows_defs.h"

namespace OCLRT {
GmmMemoryBase::GmmMemoryBase() {
    clientContext = GmmHelper::getClientContext()->getHandle();
}
bool GmmMemoryBase::configureDeviceAddressSpace(GMM_ESCAPE_HANDLE hAdapter,
                                                GMM_ESCAPE_HANDLE hDevice,
                                                GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                                GMM_GFX_SIZE_T SvmSize,
                                                BOOLEAN BDWL3Coherency) {
    return clientContext->ConfigureDeviceAddressSpace(
               {hAdapter},
               {hDevice},
               {pfnEscape},
               SvmSize,
               0,
               0,
               BDWL3Coherency,
               0,
               0) != 0;
}

bool GmmMemoryBase::configureDevice(GMM_ESCAPE_HANDLE hAdapter,
                                    GMM_ESCAPE_HANDLE hDevice,
                                    GMM_ESCAPE_FUNC_TYPE pfnEscape,
                                    GMM_GFX_SIZE_T SvmSize,
                                    BOOLEAN BDWL3Coherency,
                                    GMM_GFX_PARTITIONING &gfxPartition,
                                    uintptr_t &minAddress) {
    minAddress = windowsMinAddress;
    return configureDeviceAddressSpace(hAdapter, hDevice, pfnEscape, SvmSize, BDWL3Coherency);
}
}; // namespace OCLRT
