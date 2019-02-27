/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/sip.h"

#include "runtime/device/device.h"
#include "runtime/helpers/debug_helpers.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/helpers/ptr_math.h"
#include "runtime/helpers/string.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/program/program.h"

namespace OCLRT {

const size_t SipKernel::maxDbgSurfaceSize = 0x49c000; // proper value should be taken from compiler when it's ready

const char *getSipKernelCompilerInternalOptions(SipKernelType kernel) {
    switch (kernel) {
    default:
        DEBUG_BREAK_IF(true);
        return "";
    case SipKernelType::Csr:
        return "-cl-include-sip-csr";
    case SipKernelType::DbgCsr:
        return "-cl-include-sip-kernel-debug -cl-include-sip-csr -cl-set-bti:0";
    case SipKernelType::DbgCsrLocal:
        return "-cl-include-sip-kernel-local-debug -cl-include-sip-csr -cl-set-bti:0";
    }
}

const char *getSipLlSrc(const Device &device) {
#define M_DUMMY_LL_SRC                              \
    "define void @f()  {                        \n" \
    "    ret void                               \n" \
    "}                                          \n" \
    "!opencl.compiler.options = !{!0}           \n" \
    "!opencl.kernels = !{!1}                    \n" \
    "!0 = !{}                                   \n" \
    "!1 = !{void()* @f, !2, !3, !4, !5, !6, !7} \n" \
    "!2 = !{!\"kernel_arg_addr_space\"}         \n" \
    "!3 = !{!\"kernel_arg_access_qual\"}        \n" \
    "!4 = !{!\"kernel_arg_type\"}               \n" \
    "!5 = !{!\"kernel_arg_type_qual\"}          \n" \
    "!6 = !{!\"kernel_arg_base_type\"}          \n" \
    "!7 = !{!\"kernel_arg_name\"}               \n"

    constexpr const char *llDummySrc32 =
        "target datalayout = \"e-p:32:32:32\"       \n"
        "target triple = \"spir\"                   \n" M_DUMMY_LL_SRC;

    constexpr const char *llDummySrc64 =
        "target datalayout = \"e-p:64:64:64\"       \n"
        "target triple = \"spir64\"                 \n" M_DUMMY_LL_SRC;

#undef M_DUMMY_LL_SRC

    const uint32_t ptrSize = device.getDeviceInfo().force32BitAddressess ? 4 : sizeof(void *);

    return (ptrSize == 8) ? llDummySrc64 : llDummySrc32;
}

SipKernel::SipKernel(SipKernelType type, Program *sipProgram)
    : type(type) {
    program = sipProgram;
}
SipKernel::~SipKernel() {
    program->release();
}

GraphicsAllocation *SipKernel::getSipAllocation() const {
    return program->getKernelInfo(size_t{0})->getGraphicsAllocation();
}

const char *SipKernel::getBinary() const {
    auto kernelInfo = program->getKernelInfo(size_t{0});
    return reinterpret_cast<const char *>(ptrOffset(kernelInfo->heapInfo.pKernelHeap, kernelInfo->systemKernelOffset));
}
size_t SipKernel::getBinarySize() const {
    auto kernelInfo = program->getKernelInfo(size_t{0});
    return kernelInfo->heapInfo.pKernelHeader->KernelHeapSize - kernelInfo->systemKernelOffset;
}

SipKernelType SipKernel::getSipKernelType(GFXCORE_FAMILY family, bool debuggingActive) {
    auto &hwHelper = HwHelper::get(family);
    return hwHelper.getSipKernelType(debuggingActive);
}
} // namespace OCLRT
