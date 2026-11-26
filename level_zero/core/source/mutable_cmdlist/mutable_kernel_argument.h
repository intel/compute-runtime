/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/kernel/kernel_arg_descriptor.h"

namespace L0::MCL {

inline auto isMutableKernelArgument(const NEO::ArgDescriptor &argDescriptor) -> bool {
    if (argDescriptor.is<NEO::ArgDescriptor::argTValue>()) {
        return true;
    }
    if (false == argDescriptor.is<NEO::ArgDescriptor::argTPointer>()) {
        return false;
    }
    const auto addressQualifier = argDescriptor.getTraits().getAddressQualifier();
    return (addressQualifier == NEO::KernelArgMetadata::AddrGlobal) ||
           (addressQualifier == NEO::KernelArgMetadata::AddrConstant) ||
           (addressQualifier == NEO::KernelArgMetadata::AddrUnknown) ||
           (addressQualifier == NEO::KernelArgMetadata::AddrLocal);
}

inline auto isSlmKernelArgument(const NEO::ArgDescriptor &argDescriptor) -> bool {
    return argDescriptor.is<NEO::ArgDescriptor::argTPointer>() &&
           (argDescriptor.getTraits().getAddressQualifier() == NEO::KernelArgMetadata::AddrLocal);
}

} // namespace L0::MCL
