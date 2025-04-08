/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

namespace NEO {
template <PRODUCT_FAMILY productFamily>
class MockCompilerProductHelperHeaplessHw : public CompilerProductHelperHw<productFamily> {
  public:
    bool isHeaplessModeEnabled(const HardwareInfo &hwInfo) const override {
        return heaplessModeEnabled;
    }

    MockCompilerProductHelperHeaplessHw(bool heaplessModeEnabled) : CompilerProductHelperHw<productFamily>(), heaplessModeEnabled(heaplessModeEnabled) {}
    bool heaplessModeEnabled = false;
};

class MockCompilerProductHelper : public CompilerProductHelper {
  public:
    using BaseClass = CompilerProductHelper;
    using BaseClass::getDefaultHwIpVersion;
    ADDMETHOD_CONST_NOBASE(isMidThreadPreemptionSupported, bool, false, (const HardwareInfo &hwInfo));

    ADDMETHOD_CONST_NOBASE(isForceEmuInt32DivRemSPRequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isStatelessToStatefulBufferOffsetSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isMatrixMultiplyAccumulateSupported, bool, false, (const ReleaseHelper *releaseHelper));
    ADDMETHOD_CONST_NOBASE(isMatrixMultiplyAccumulateTF32Supported, bool, false, (const HardwareInfo &hwInfo));
    ADDMETHOD_CONST_NOBASE(isSplitMatrixMultiplyAccumulateSupported, bool, false, (const ReleaseHelper *releaseHelper));
    ADDMETHOD_CONST_NOBASE(isBFloat16ConversionSupported, bool, false, (const ReleaseHelper *releaseHelper));
    ADDMETHOD_CONST_NOBASE(isSubgroupLocalBlockIoSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isDotAccumulateSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isDotProductAccumulateSystolicSupported, bool, false, (const ReleaseHelper *releaseHelper));
    ADDMETHOD_CONST_NOBASE(isCreateBufferWithPropertiesSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isSubgroupNamedBarrierSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isSubgroupExtendedBlockReadSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isSubgroup2DBlockIOSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isSubgroupBufferPrefetchSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isForceToStatelessRequired, bool, false, ());
    ADDMETHOD_CONST_NOBASE(failBuildProgramWithStatefulAccessPreference, bool, false, ());
    ADDMETHOD_CONST_NOBASE(isDotIntegerProductExtensionSupported, bool, false, ());
    ADDMETHOD_CONST_NOBASE(oclocEnforceZebinFormat, bool, false, ());
    ADDMETHOD_CONST_NOBASE_VOIDRETURN(setProductConfigForHwInfo, (HardwareInfo & hwInfo, HardwareIpVersion config));
    ADDMETHOD_CONST_NOBASE(getCachingPolicyOptions, const char *, nullptr, (bool isDebuggerActive));
    ADDMETHOD_CONST_NOBASE(getHwInfoConfig, uint64_t, 0, (const HardwareInfo &hwInfo));
    ADDMETHOD_CONST_NOBASE(getDefaultHwIpVersion, uint32_t, 0, ());
    ADDMETHOD_CONST_NOBASE(matchRevisionIdWithProductConfig, uint32_t, 0, (HardwareIpVersion ipVersion, uint32_t revisionID));
    ADDMETHOD_CONST_NOBASE(getDeviceExtensions, std::string, {}, (const HardwareInfo &hwInfo, const ReleaseHelper *releaseHelper));
    using getDeviceOpenCLCVersionsRetType = StackVec<OclCVersion, 5>;
    ADDMETHOD_CONST_NOBASE(getDeviceOpenCLCVersions, getDeviceOpenCLCVersionsRetType, {}, (const HardwareInfo &hwInfo, OclCVersion max));
    ADDMETHOD_CONST_NOBASE_VOIDRETURN(adjustHwInfoForIgc, (HardwareInfo & hwInfo));
    ADDMETHOD_CONST_NOBASE(isHeaplessModeEnabled, bool, false, (const HardwareInfo &hwInfo));
    ADDMETHOD_CONST_NOBASE(isHeaplessStateInitEnabled, bool, false, (bool heaplessModeEnabled));
    ADDMETHOD_CONST_NOBASE_VOIDRETURN(getKernelFp16AtomicCapabilities, (const ReleaseHelper *releaseHelper, uint32_t &fp16Caps));
    ADDMETHOD_CONST_NOBASE_VOIDRETURN(getKernelFp32AtomicCapabilities, (uint32_t & fp32Caps));
    ADDMETHOD_CONST_NOBASE_VOIDRETURN(getKernelFp64AtomicCapabilities, (uint32_t & fp64Caps));
    ADDMETHOD_CONST_NOBASE_VOIDRETURN(getKernelCapabilitiesExtra, (const ReleaseHelper *releaseHelper, uint32_t &extraCaps));
    ADDMETHOD_CONST_NOBASE(isBindlessAddressingDisabled, bool, false, (const ReleaseHelper *releaseHelper));
    ADDMETHOD_CONST_NOBASE(isForceBindlessRequired, bool, false, (const HardwareInfo &hwInfo));
    ADDMETHOD_CONST_NOBASE(getProductConfigFromHwInfo, uint32_t, 0, (const HardwareInfo &hwInfo));
    ADDMETHOD_CONST_NOBASE(getCustomIgcLibraryName, const char *, nullptr, ());
    ADDMETHOD_CONST_NOBASE(getFinalizerLibraryName, const char *, nullptr, ());
    ADDMETHOD_CONST_NOBASE(useIgcAsFcl, bool, false, ());
    ADDMETHOD_CONST_NOBASE(getPreferredIntermediateRepresentation, IGC::CodeType::CodeType_t, IGC::CodeType::undefined, ());
};

} // namespace NEO
