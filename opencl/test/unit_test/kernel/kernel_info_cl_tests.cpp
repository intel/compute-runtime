/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/kernel/kernel_info_cl.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

TEST(AsClConvertersTest, whenConvertingAccessQualifiersThenProperEnumValuesAreReturned) {
    using namespace NEO::KernelArgMetadata;
    EXPECT_EQ(static_cast<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_NONE), NEO::asClKernelArgAccessQualifier(AccessNone));
    EXPECT_EQ(static_cast<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_READ_ONLY), NEO::asClKernelArgAccessQualifier(AccessReadOnly));
    EXPECT_EQ(static_cast<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_WRITE_ONLY), NEO::asClKernelArgAccessQualifier(AccessWriteOnly));
    EXPECT_EQ(static_cast<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_READ_WRITE), NEO::asClKernelArgAccessQualifier(AccessReadWrite));
    EXPECT_EQ(0U, NEO::asClKernelArgAccessQualifier(AccessUnknown));
}

TEST(AsClConvertersTest, whenConvertingAddressQualifiersThenProperEnumValuesAreReturned) {
    using namespace NEO::KernelArgMetadata;
    EXPECT_EQ(static_cast<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_GLOBAL), NEO::asClKernelArgAddressQualifier(AddrGlobal));
    EXPECT_EQ(static_cast<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_LOCAL), NEO::asClKernelArgAddressQualifier(AddrLocal));
    EXPECT_EQ(static_cast<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_PRIVATE), NEO::asClKernelArgAddressQualifier(AddrPrivate));
    EXPECT_EQ(static_cast<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_CONSTANT), NEO::asClKernelArgAddressQualifier(AddrConstant));
    EXPECT_EQ(0U, NEO::asClKernelArgAddressQualifier(AddrUnknown));
}

TEST(AsClConvertersTest, whenConvertingTypeQualifiersThenProperBitfieldsAreSet) {
    using namespace NEO::KernelArgMetadata;
    TypeQualifiers typeQualifiers = {};
    typeQualifiers.constQual = true;
    EXPECT_EQ(static_cast<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_CONST), NEO::asClKernelArgTypeQualifier(typeQualifiers));

    typeQualifiers = {};
    typeQualifiers.volatileQual = true;
    EXPECT_EQ(static_cast<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_VOLATILE), NEO::asClKernelArgTypeQualifier(typeQualifiers));

    typeQualifiers = {};
    typeQualifiers.restrictQual = true;
    EXPECT_EQ(static_cast<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_RESTRICT), NEO::asClKernelArgTypeQualifier(typeQualifiers));

    typeQualifiers = {};
    typeQualifiers.pipeQual = true;
    EXPECT_EQ(static_cast<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_PIPE), NEO::asClKernelArgTypeQualifier(typeQualifiers));

    typeQualifiers = {};
    typeQualifiers.constQual = true;
    typeQualifiers.volatileQual = true;
    EXPECT_EQ(static_cast<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_CONST | CL_KERNEL_ARG_TYPE_VOLATILE), NEO::asClKernelArgTypeQualifier(typeQualifiers));

    typeQualifiers = {};
    typeQualifiers.constQual = true;
    typeQualifiers.volatileQual = true;
    typeQualifiers.pipeQual = true;
    typeQualifiers.restrictQual = true;
    EXPECT_EQ(static_cast<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_CONST | CL_KERNEL_ARG_TYPE_VOLATILE | CL_KERNEL_ARG_TYPE_RESTRICT | CL_KERNEL_ARG_TYPE_PIPE),
              NEO::asClKernelArgTypeQualifier(typeQualifiers));
}
