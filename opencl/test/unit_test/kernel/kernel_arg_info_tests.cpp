/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/fixtures/memory_management_fixture.h"
#include "shared/test/common/helpers/kernel_binary_helper.h"
#include "shared/test/common/test_macros/test.h"
#include <shared/test/common/mocks/mock_modules_zebin.h>

#include "opencl/source/kernel/kernel.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_kernel.h"
#include "opencl/test/unit_test/mocks/mock_program.h"
#include "opencl/test/unit_test/program/program_tests.h"
#include "opencl/test/unit_test/program/program_with_source.h"

using namespace NEO;

class KernelArgInfoTest : public ProgramFromSourceTest {
  public:
    KernelArgInfoTest() {
    }

    ~KernelArgInfoTest() override = default;

  protected:
    void SetUp() override {
        kbHelper = new KernelBinaryHelper("copybuffer", true);
        ProgramFromSourceTest::SetUp();
        ASSERT_NE(nullptr, pProgram);
        ASSERT_EQ(CL_SUCCESS, retVal);

        retVal = pProgram->build(
            pProgram->getDevices(),
            nullptr,
            false);
        ASSERT_EQ(CL_SUCCESS, retVal);

        // create a kernel
        pKernel = Kernel::create(
            pProgram,
            pProgram->getKernelInfoForKernel(kernelName),
            *pPlatform->getClDevice(0),
            &retVal);

        ASSERT_EQ(CL_SUCCESS, retVal);
        ASSERT_NE(nullptr, pKernel);
    }

    void TearDown() override {
        delete pKernel;
        pKernel = nullptr;
        ProgramFromSourceTest::TearDown();
        delete kbHelper;
    }

    template <typename T>
    void queryArgInfo(cl_kernel_arg_info paramName, T &paramValue) {
        size_t paramValueSize = 0;
        size_t paramValueSizeRet = 0;

        // get size
        retVal = pKernel->getArgInfo(
            0,
            paramName,
            paramValueSize,
            nullptr,
            &paramValueSizeRet);
        EXPECT_NE(0u, paramValueSizeRet);
        ASSERT_EQ(CL_SUCCESS, retVal);

        // get the name
        paramValueSize = paramValueSizeRet;

        retVal = pKernel->getArgInfo(
            0,
            paramName,
            paramValueSize,
            &paramValue,
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);
    }

    Kernel *pKernel = nullptr;
    cl_int retVal = CL_SUCCESS;
    KernelBinaryHelper *kbHelper = nullptr;
};

TEST_F(KernelArgInfoTest, GivenNullWhenGettingKernelInfoThenNullIsReturned) {
    auto kernelInfo = this->pProgram->getKernelInfo(nullptr, 0);
    EXPECT_EQ(nullptr, kernelInfo);
}

TEST_F(KernelArgInfoTest, GivenInvalidParametersWhenGettingKernelArgInfoThenValueSizeRetIsNotUpdated) {
    size_t paramValueSizeRet = 0x1234;

    retVal = pKernel->getArgInfo(
        0,
        0,
        0,
        nullptr,
        &paramValueSizeRet);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
    EXPECT_EQ(0x1234u, paramValueSizeRet);
}

TEST_F(KernelArgInfoTest, GivenKernelArgAccessQualifierWhenQueryingArgInfoThenKernelArgAcessNoneIsReturned) {
    auto &kernelDescriptor = const_cast<KernelDescriptor &>(pKernel->getDescriptor());
    auto &argTraits = kernelDescriptor.payloadMappings.explicitArgs[0].getTraits();
    argTraits.accessQualifier = KernelArgMetadata::AccessNone;

    cl_kernel_arg_access_qualifier paramValue = 0;
    queryArgInfo<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_QUALIFIER, paramValue);
    EXPECT_EQ(static_cast<cl_kernel_arg_access_qualifier>(CL_KERNEL_ARG_ACCESS_NONE), paramValue);
}

TEST_F(KernelArgInfoTest, GivenKernelArgAddressQualifierWhenQueryingArgInfoThenKernelArgAddressGlobalIsReturned) {
    auto &kernelDescriptor = const_cast<KernelDescriptor &>(pKernel->getDescriptor());
    auto &argTraits = kernelDescriptor.payloadMappings.explicitArgs[0].getTraits();
    argTraits.addressQualifier = KernelArgMetadata::AddrGlobal;

    cl_kernel_arg_address_qualifier paramValue = 0;
    queryArgInfo<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_QUALIFIER, paramValue);
    EXPECT_EQ(static_cast<cl_kernel_arg_address_qualifier>(CL_KERNEL_ARG_ADDRESS_GLOBAL), paramValue);
}

TEST_F(KernelArgInfoTest, GivenKernelArgTypeQualifierWhenQueryingArgInfoThenKernelArgTypeNoneIsReturned) {
    cl_kernel_arg_type_qualifier paramValue = 0;
    queryArgInfo<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_QUALIFIER, paramValue);
    EXPECT_EQ(static_cast<cl_kernel_arg_type_qualifier>(CL_KERNEL_ARG_TYPE_NONE), paramValue);
}

TEST_F(KernelArgInfoTest, GivenParamWhenGettingKernelTypeNameThenCorrectValueIsReturned) {
    cl_uint argInd = 0;
    const char expectedArgType[] = "uint*";
    auto &kernelDescriptor = const_cast<KernelDescriptor &>(pKernel->getDescriptor());
    kernelDescriptor.explicitArgsExtendedMetadata.at(argInd).type = expectedArgType;

    cl_kernel_arg_info paramName = CL_KERNEL_ARG_TYPE_NAME;
    char *paramValue = nullptr;
    size_t paramValueSize = 0;
    size_t paramValueSizeRet = 0;

    // get size
    retVal = pKernel->getArgInfo(
        argInd,
        paramName,
        paramValueSize,
        nullptr,
        &paramValueSizeRet);
    EXPECT_NE(0u, paramValueSizeRet);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // allocate space for name
    paramValue = new char[paramValueSizeRet];

    // get the name
    paramValueSize = paramValueSizeRet;

    retVal = pKernel->getArgInfo(
        0,
        paramName,
        paramValueSize,
        paramValue,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    auto result = strncmp(paramValue, expectedArgType, sizeof(expectedArgType));
    EXPECT_EQ(0, result);
    delete[] paramValue;
}

TEST_F(KernelArgInfoTest, GivenParamWhenGettingKernelArgNameThenCorrectValueIsReturned) {
    cl_uint argInd = 0;
    const char expectedArgName[] = "src";
    auto &kernelDescriptor = const_cast<KernelDescriptor &>(pKernel->getDescriptor());
    kernelDescriptor.explicitArgsExtendedMetadata.at(argInd).argName = expectedArgName;

    cl_kernel_arg_info paramName = CL_KERNEL_ARG_NAME;
    char *paramValue = nullptr;
    size_t paramValueSize = 0;
    size_t paramValueSizeRet = 0;

    // get size
    retVal = pKernel->getArgInfo(
        argInd,
        paramName,
        paramValueSize,
        nullptr,
        &paramValueSizeRet);
    EXPECT_NE(0u, paramValueSizeRet);
    ASSERT_EQ(CL_SUCCESS, retVal);

    // allocate space for name
    paramValue = new char[paramValueSizeRet];

    // get the name
    paramValueSize = paramValueSizeRet;

    retVal = pKernel->getArgInfo(
        0,
        paramName,
        paramValueSize,
        paramValue,
        nullptr);
    ASSERT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0, strcmp(paramValue, expectedArgName));
    delete[] paramValue;
}

TEST_F(KernelArgInfoTest, givenNonZebinBinaryAndNoExplicitArgsMetadataWhenQueryingArgsInfoThenReturnError) {
    constexpr auto mockDeviceBinarySize = 0x10;
    uint8_t mockDeviceBinary[mockDeviceBinarySize]{0};

    auto &buildInfo = pProgram->buildInfos[rootDeviceIndex];
    buildInfo.unpackedDeviceBinary.reset(reinterpret_cast<char *>(mockDeviceBinary));
    buildInfo.unpackedDeviceBinarySize = mockDeviceBinarySize;
    ASSERT_FALSE(NEO::isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<uint8_t>::fromAny(mockDeviceBinary, mockDeviceBinarySize)));

    auto &kernelDescriptor = const_cast<KernelDescriptor &>(pKernel->getDescriptor());
    kernelDescriptor.explicitArgsExtendedMetadata.clear();
    ASSERT_TRUE(kernelDescriptor.explicitArgsExtendedMetadata.empty());

    retVal = pKernel->getArgInfo(
        0,
        CL_KERNEL_ARG_NAME,
        0,
        nullptr,
        0);

    EXPECT_EQ(CL_KERNEL_ARG_INFO_NOT_AVAILABLE, retVal);
    buildInfo.unpackedDeviceBinary.release();
}

TEST_F(KernelArgInfoTest, givenZebinBinaryAndErrorOnRetrievingArgsMetadataFromKernelsMiscInfoWhenQueryingArgsInfoThenReturnError) {
    ZebinTestData::ValidEmptyProgram zebin;
    ASSERT_TRUE(isDeviceBinaryFormat<NEO::DeviceBinaryFormat::Zebin>(ArrayRef<const uint8_t>::fromAny(zebin.storage.data(), zebin.storage.size())));

    auto &buildInfo = pProgram->buildInfos[rootDeviceIndex];
    buildInfo.unpackedDeviceBinary.reset(reinterpret_cast<char *>(zebin.storage.data()));
    buildInfo.unpackedDeviceBinarySize = zebin.storage.size();
    ASSERT_EQ(std::string::npos, buildInfo.kernelMiscInfoPos);

    auto &kernelDescriptor = const_cast<KernelDescriptor &>(pKernel->getDescriptor());
    kernelDescriptor.explicitArgsExtendedMetadata.clear();
    ASSERT_TRUE(kernelDescriptor.explicitArgsExtendedMetadata.empty());

    retVal = pKernel->getArgInfo(
        0,
        CL_KERNEL_ARG_NAME,
        0,
        nullptr,
        0);

    EXPECT_EQ(CL_KERNEL_ARG_INFO_NOT_AVAILABLE, retVal);
    buildInfo.unpackedDeviceBinary.release();
}

TEST_F(KernelArgInfoTest, givenZebinBinaryWithProperKernelsMiscInfoAndNoExplicitArgsMetadataWhenQueryingArgInfoThenRetrieveItFromKernelsMiscInfo) {
    std::string zeInfo = R"===('
kernels:
  - name:            CopyBuffer
    execution_env:
      simd_size:       32
    payload_arguments:
      - arg_type:        arg_bypointer
        offset:          0
        size:            0
        arg_index:       0
        addrmode:        stateful
        addrspace:       global
        access_type:     readwrite
      - arg_type:        arg_bypointer
        offset:          32
        size:            8
        arg_index:       0
        addrmode:        stateless
        addrspace:       global
        access_type:     readwrite
      - arg_type:        enqueued_local_size
        offset:          40
        size:            12
kernels_misc_info:
  - name:            CopyBuffer
    args_info:
      - index:           0
        name:            a
        address_qualifier: __global
        access_qualifier: NONE
        type_name:       'int*;8'
        type_qualifiers: NONE
)===";
    std::vector<uint8_t> storage;
    MockElfEncoder<> elfEncoder;
    auto &elfHeader = elfEncoder.getElfFileHeader();
    elfHeader.type = NEO::Elf::ET_ZEBIN_EXE;
    elfHeader.machine = pProgram->getExecutionEnvironment().rootDeviceEnvironments[rootDeviceIndex]->getHardwareInfo()->platform.eProductFamily;
    const uint8_t testKernelData[0x10] = {0u};

    elfEncoder.appendSection(NEO::Elf::SHT_PROGBITS, NEO::Elf::SectionsNamesZebin::textPrefix.str() + "CopyBuffer", testKernelData);
    elfEncoder.appendSection(NEO::Elf::SHT_ZEBIN_ZEINFO, NEO::Elf::SectionsNamesZebin::zeInfo, zeInfo);
    storage = elfEncoder.encode();
    elfHeader = *reinterpret_cast<NEO::Elf::ElfFileHeader<NEO::Elf::EI_CLASS_64> *>(storage.data());

    auto &buildInfo = pProgram->buildInfos[rootDeviceIndex];

    //set kernels_misc_info pos manually, as we are not invoking decodeZebin() or processProgramInfo() in this test
    ProgramInfo programInfo;
    setKernelMiscInfoPosition(zeInfo, programInfo);
    buildInfo.kernelMiscInfoPos = programInfo.kernelMiscInfoPos;
    buildInfo.unpackedDeviceBinary.reset(reinterpret_cast<char *>(storage.data()));
    buildInfo.unpackedDeviceBinarySize = storage.size();

    auto &kernelDescriptor = const_cast<KernelDescriptor &>(pKernel->getDescriptor());
    kernelDescriptor.explicitArgsExtendedMetadata.clear();
    ASSERT_TRUE(kernelDescriptor.explicitArgsExtendedMetadata.empty());

    std::array<cl_kernel_arg_info, 5> paramNames = {
        CL_KERNEL_ARG_NAME,
        CL_KERNEL_ARG_ADDRESS_QUALIFIER,
        CL_KERNEL_ARG_ACCESS_QUALIFIER,
        CL_KERNEL_ARG_TYPE_NAME,
        CL_KERNEL_ARG_TYPE_QUALIFIER,
    };
    cl_uint argInd = 0;
    constexpr size_t maxParamValueSize{0x10};
    size_t paramValueSize = 0;
    size_t paramValueSizeRet = 0;

    for (const auto &paramName : paramNames) {
        char paramValue[maxParamValueSize]{0};

        retVal = pKernel->getArgInfo(
            argInd,
            paramName,
            paramValueSize,
            nullptr,
            &paramValueSizeRet);
        EXPECT_NE(0u, paramValueSizeRet);
        ASSERT_EQ(CL_SUCCESS, retVal);

        ASSERT_LT(paramValueSizeRet, maxParamValueSize);
        paramValueSize = paramValueSizeRet;

        retVal = pKernel->getArgInfo(
            argInd,
            paramName,
            paramValueSize,
            paramValue,
            nullptr);
        ASSERT_EQ(CL_SUCCESS, retVal);
        switch (paramName) {
        case (CL_KERNEL_ARG_NAME):
            EXPECT_EQ(0, strcmp(paramValue, "a"));
            break;
        case (CL_KERNEL_ARG_ADDRESS_QUALIFIER):
            EXPECT_EQ(*(reinterpret_cast<cl_kernel_arg_address_qualifier *>(paramValue)), static_cast<cl_uint>(CL_KERNEL_ARG_ADDRESS_GLOBAL));
            break;
        case (CL_KERNEL_ARG_ACCESS_QUALIFIER):
            EXPECT_EQ(*(reinterpret_cast<cl_kernel_arg_access_qualifier *>(paramValue)), static_cast<cl_uint>(CL_KERNEL_ARG_ACCESS_NONE));
            break;
        case (CL_KERNEL_ARG_TYPE_NAME):
            EXPECT_EQ(0, strcmp(paramValue, "'int*;8'"));
            break;
        case (CL_KERNEL_ARG_TYPE_QUALIFIER):
            EXPECT_EQ(*(reinterpret_cast<cl_kernel_arg_type_qualifier *>(paramValue)), static_cast<cl_ulong>(CL_KERNEL_ARG_TYPE_NONE));
            break;
        default:
            ASSERT_TRUE(false);
            break;
        }
    }
    buildInfo.unpackedDeviceBinary.release();
}