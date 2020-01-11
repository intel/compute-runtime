/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/program/kernel_arg_info.h"

#include "CL/cl.h"

namespace NEO {

constexpr cl_kernel_arg_access_qualifier asClKernelArgAccessQualifier(KernelArgMetadata::AccessQualifier accessQualifier) {
    using namespace KernelArgMetadata;
    switch (accessQualifier) {
    default:
        return 0U;
    case AccessQualifier::None:
        return CL_KERNEL_ARG_ACCESS_NONE;
    case AccessQualifier::ReadOnly:
        return CL_KERNEL_ARG_ACCESS_READ_ONLY;
    case AccessQualifier::WriteOnly:
        return CL_KERNEL_ARG_ACCESS_WRITE_ONLY;
    case AccessQualifier::ReadWrite:
        return CL_KERNEL_ARG_ACCESS_READ_WRITE;
    }
}

constexpr cl_kernel_arg_address_qualifier asClKernelArgAddressQualifier(KernelArgMetadata::AddressSpaceQualifier addressQualifier) {
    using namespace KernelArgMetadata;
    switch (addressQualifier) {
    default:
        return 0U;
    case AddressSpaceQualifier::Global:
        return CL_KERNEL_ARG_ADDRESS_GLOBAL;
    case AddressSpaceQualifier::Local:
        return CL_KERNEL_ARG_ADDRESS_LOCAL;
    case AddressSpaceQualifier::Private:
        return CL_KERNEL_ARG_ADDRESS_PRIVATE;
    case AddressSpaceQualifier::Constant:
        return CL_KERNEL_ARG_ADDRESS_CONSTANT;
    }
}

constexpr cl_kernel_arg_type_qualifier asClKernelArgTypeQualifier(KernelArgMetadata::TypeQualifiers typeQualifiers) {
    using namespace KernelArgMetadata;
    cl_kernel_arg_type_qualifier ret = 0U;
    ret |= (typeQualifiers.constQual) ? CL_KERNEL_ARG_TYPE_CONST : 0U;
    ret |= (typeQualifiers.volatileQual) ? CL_KERNEL_ARG_TYPE_VOLATILE : 0U;
    ret |= (typeQualifiers.restrictQual) ? CL_KERNEL_ARG_TYPE_RESTRICT : 0U;
    ret |= (typeQualifiers.pipeQual) ? CL_KERNEL_ARG_TYPE_PIPE : 0U;
    return ret;
}
} // namespace NEO
