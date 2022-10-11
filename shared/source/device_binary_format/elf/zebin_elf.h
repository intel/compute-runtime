/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/device_binary_format/elf/elf.h"
#include "shared/source/device_binary_format/elf/elf_decoder.h"
#include "shared/source/utilities/const_stringref.h"

#include <array>
#include <cinttypes>
#include <cstddef>
#include <optional>

namespace NEO {

namespace Elf {

enum ELF_TYPE_ZEBIN : uint16_t {
    ET_ZEBIN_REL = 0xff11, // A relocatable ZE binary file
    ET_ZEBIN_EXE = 0xff12, // An executable ZE binary file
    ET_ZEBIN_DYN = 0xff13, // A shared object ZE binary file
};

enum SHT_ZEBIN : uint32_t {
    SHT_ZEBIN_SPIRV = 0xff000009,      // .spv.kernel section, value the same as SHT_OPENCL_SPIRV
    SHT_ZEBIN_ZEINFO = 0xff000011,     // .ze_info section
    SHT_ZEBIN_GTPIN_INFO = 0xff000012, // .gtpin_info section
    SHT_ZEBIN_VISA_ASM = 0xff000013,   // .visaasm sections
    SHT_ZEBIN_MISC = 0xff000014        // .misc section
};

enum RELOC_TYPE_ZEBIN : uint32_t {
    R_ZE_NONE,
    R_ZE_SYM_ADDR,
    R_ZE_SYM_ADDR_32,
    R_ZE_SYM_ADDR_32_HI,
    R_PER_THREAD_PAYLOAD_OFFSET
};

namespace SectionsNamesZebin {
constexpr ConstStringRef textPrefix = ".text.";
constexpr ConstStringRef functions = ".text.Intel_Symbol_Table_Void_Program";
constexpr ConstStringRef dataConst = ".data.const";
constexpr ConstStringRef dataGlobalConst = ".data.global_const";
constexpr ConstStringRef dataGlobal = ".data.global";
constexpr ConstStringRef dataConstString = ".data.const.string";
constexpr ConstStringRef symtab = ".symtab";
constexpr ConstStringRef relTablePrefix = ".rel.";
constexpr ConstStringRef relaTablePrefix = ".rela.";
constexpr ConstStringRef spv = ".spv";
constexpr ConstStringRef debugPrefix = ".debug_";
constexpr ConstStringRef debugInfo = ".debug_info";
constexpr ConstStringRef debugAbbrev = ".debug_abbrev";
constexpr ConstStringRef zeInfo = ".ze_info";
constexpr ConstStringRef gtpinInfo = ".gtpin_info";
constexpr ConstStringRef noteIntelGT = ".note.intelgt.compat";
constexpr ConstStringRef buildOptions = ".misc.buildOptions";
constexpr ConstStringRef vIsaAsmPrefix = ".visaasm.";
constexpr ConstStringRef externalFunctions = "Intel_Symbol_Table_Void_Program";
} // namespace SectionsNamesZebin

constexpr ConstStringRef IntelGtNoteOwnerName = "IntelGT";
enum IntelGTSectionType : uint32_t {
    ProductFamily = 1,
    GfxCore = 2,
    TargetMetadata = 3,
    ZebinVersion = 4,
    LastSupported = ZebinVersion
};
struct IntelGTNote {
    IntelGTSectionType type;
    ArrayRef<const uint8_t> data;
};
struct ZebinTargetFlags {
    union {
        struct {
            // bit[7:0]: dedicated for specific generator (meaning based on generatorId)
            uint8_t generatorSpecificFlags : 8;

            // bit[12:8]: values [0-31], min compatbile device revision Id (stepping)
            uint8_t minHwRevisionId : 5;

            // bit[13:13]:
            // 0 - full validation during decoding (safer decoding)
            // 1 - no validation (faster decoding - recommended for known generators)
            bool validateRevisionId : 1;

            // bit[14:14]:
            // 0 - ignore minHwRevisionId and maxHwRevisionId
            // 1 - underlying device must match specified revisionId info
            bool disableExtendedValidation : 1;

            // bit[15:15]:
            // 0 - elfFileHeader::machine is PRODUCT_FAMILY
            // 1 - elfFileHeader::machine is GFXCORE_FAMILY
            bool machineEntryUsesGfxCoreInsteadOfProductFamily : 1;

            // bit[20:16]:  max compatbile device revision Id (stepping)
            uint8_t maxHwRevisionId : 5;

            // bit[23:21]: generator of this device binary
            // 0 - Unregistered
            // 1 - IGC
            uint8_t generatorId : 3;

            // bit[31:24]: MBZ, reserved for future use
        };
        uint32_t packed = 0U;
    };
};
static_assert(sizeof(ZebinTargetFlags) == sizeof(uint32_t), "");

namespace ZebinKernelMetadata {
namespace Tags {
constexpr ConstStringRef kernels("kernels");
constexpr ConstStringRef version("version");
constexpr ConstStringRef globalHostAccessTable("global_host_access_table");
constexpr ConstStringRef functions("functions");
constexpr ConstStringRef kernelMiscInfo("kernels_misc_info");

namespace Kernel {
constexpr ConstStringRef attributes("user_attributes");
constexpr ConstStringRef name("name");
constexpr ConstStringRef executionEnv("execution_env");
constexpr ConstStringRef debugEnv("debug_env");
constexpr ConstStringRef payloadArguments("payload_arguments");
constexpr ConstStringRef bindingTableIndices("binding_table_indices");
constexpr ConstStringRef perThreadPayloadArguments("per_thread_payload_arguments");
constexpr ConstStringRef perThreadMemoryBuffers("per_thread_memory_buffers");
constexpr ConstStringRef experimentalProperties("experimental_properties");
constexpr ConstStringRef inlineSamplers("inline_samplers");

namespace ExecutionEnv {
constexpr ConstStringRef barrierCount("barrier_count");
constexpr ConstStringRef disableMidThreadPreemption("disable_mid_thread_preemption");
constexpr ConstStringRef grfCount("grf_count");
constexpr ConstStringRef has4gbBuffers("has_4gb_buffers");
constexpr ConstStringRef hasDpas("has_dpas");
constexpr ConstStringRef hasFenceForImageAccess("has_fence_for_image_access");
constexpr ConstStringRef hasGlobalAtomics("has_global_atomics");
constexpr ConstStringRef hasMultiScratchSpaces("has_multi_scratch_spaces");
constexpr ConstStringRef hasNoStatelessWrite("has_no_stateless_write");
constexpr ConstStringRef hasStackCalls("has_stack_calls");
constexpr ConstStringRef hwPreemptionMode("hw_preemption_mode");
constexpr ConstStringRef inlineDataPayloadSize("inline_data_payload_size");
constexpr ConstStringRef offsetToSkipPerThreadDataLoad("offset_to_skip_per_thread_data_load");
constexpr ConstStringRef offsetToSkipSetFfidGp("offset_to_skip_set_ffid_gp");
constexpr ConstStringRef requiredSubGroupSize("required_sub_group_size");
constexpr ConstStringRef requiredWorkGroupSize("required_work_group_size");
constexpr ConstStringRef requireDisableEUFusion("require_disable_eufusion");
constexpr ConstStringRef simdSize("simd_size");
constexpr ConstStringRef slmSize("slm_size");
constexpr ConstStringRef subgroupIndependentForwardProgress("subgroup_independent_forward_progress");
constexpr ConstStringRef workGroupWalkOrderDimensions("work_group_walk_order_dimensions");
constexpr ConstStringRef threadSchedulingMode("thread_scheduling_mode");
namespace ThreadSchedulingMode {
constexpr ConstStringRef ageBased("age_based");
constexpr ConstStringRef roundRobin("round_robin");
constexpr ConstStringRef roundRobinStall("round_robin_stall");
} // namespace ThreadSchedulingMode
constexpr ConstStringRef indirectStatelessCount("indirect_stateless_count");
} // namespace ExecutionEnv

namespace Attributes {
constexpr ConstStringRef intelReqdSubgroupSize("intel_reqd_sub_group_size");
constexpr ConstStringRef intelReqdWorkgroupWalkOrder("intel_reqd_workgroup_walk_order");
constexpr ConstStringRef reqdWorkgroupSize("reqd_work_group_size");
constexpr ConstStringRef invalidKernel("invalid_kernel");
constexpr ConstStringRef vecTypeHint("vec_type_hint");
constexpr ConstStringRef workgroupSizeHint("work_group_size_hint");
constexpr ConstStringRef hintSuffix("_hint");
} // namespace Attributes

namespace DebugEnv {
constexpr ConstStringRef debugSurfaceBTI("sip_surface_bti");
} // namespace DebugEnv

namespace PayloadArgument {
constexpr ConstStringRef argType("arg_type");
constexpr ConstStringRef argIndex("arg_index");
constexpr ConstStringRef offset("offset");
constexpr ConstStringRef size("size");
constexpr ConstStringRef addrmode("addrmode");
constexpr ConstStringRef addrspace("addrspace");
constexpr ConstStringRef accessType("access_type");
constexpr ConstStringRef samplerIndex("sampler_index");
constexpr ConstStringRef sourceOffset("source_offset");
constexpr ConstStringRef slmArgAlignment("slm_alignment");
constexpr ConstStringRef imageType("image_type");
constexpr ConstStringRef imageTransformable("image_transformable");
constexpr ConstStringRef samplerType("sampler_type");
constexpr ConstStringRef addrMode("sampler_desc_addrmode");
constexpr ConstStringRef filterMode("sampler_desc_filtermode");
constexpr ConstStringRef normalized("sampler_desc_normalized");

namespace ArgType {
constexpr ConstStringRef localSize("local_size");
constexpr ConstStringRef groupCount("group_count");
constexpr ConstStringRef globalIdOffset("global_id_offset");
constexpr ConstStringRef globalSize("global_size");
constexpr ConstStringRef enqueuedLocalSize("enqueued_local_size");
constexpr ConstStringRef privateBaseStateless("private_base_stateless");
constexpr ConstStringRef argByvalue("arg_byvalue");
constexpr ConstStringRef argBypointer("arg_bypointer");
constexpr ConstStringRef bufferAddress("buffer_address");
constexpr ConstStringRef bufferOffset("buffer_offset");
constexpr ConstStringRef printfBuffer("printf_buffer");
constexpr ConstStringRef workDimensions("work_dimensions");
constexpr ConstStringRef implicitArgBuffer("implicit_arg_buffer");
constexpr ConstStringRef inlineSampler("arg_inline_sampler");
namespace Image {
constexpr ConstStringRef width("image_width");
constexpr ConstStringRef height("image_height");
constexpr ConstStringRef depth("image_depth");
constexpr ConstStringRef channelDataType("image_channel_data_type");
constexpr ConstStringRef channelOrder("image_channel_order");
constexpr ConstStringRef arraySize("image_array_size");
constexpr ConstStringRef numSamples("image_num_samples");
constexpr ConstStringRef numMipLevels("image_num_mip_levels");
constexpr ConstStringRef flatBaseOffset("flat_image_baseoffset");
constexpr ConstStringRef flatWidth("flat_image_width");
constexpr ConstStringRef flatHeight("flat_image_height");
constexpr ConstStringRef flatPitch("flat_image_pitch");
} // namespace Image
namespace Sampler {
constexpr ConstStringRef snapWa("sampler_snap_wa");
constexpr ConstStringRef normCoords("sampler_normalized");
constexpr ConstStringRef addrMode("sampler_address");
namespace Vme {
constexpr ConstStringRef blockType("vme_mb_block_type");
constexpr ConstStringRef subpixelMode("vme_subpixel_mode");
constexpr ConstStringRef sadAdjustMode("vme_sad_adjust_mode");
constexpr ConstStringRef searchPathType("vme_search_path_type");
} // namespace Vme
} // namespace Sampler
} // namespace ArgType
namespace ImageType {
constexpr ConstStringRef imageTypeBuffer("image_buffer");
constexpr ConstStringRef imageType1D("image_1d");
constexpr ConstStringRef imageType1DArray("image_1d_array");
constexpr ConstStringRef imageType2D("image_2d");
constexpr ConstStringRef imageType2DArray("image_2d_array");
constexpr ConstStringRef imageType3D("image_3d");
constexpr ConstStringRef imageTypeCube("image_cube_array");
constexpr ConstStringRef imageTypeCubeArray("image_buffer");
constexpr ConstStringRef imageType2DDepth("image_2d_depth");
constexpr ConstStringRef imageType2DArrayDepth("image_2d_array_depth");
constexpr ConstStringRef imageType2DMSAA("image_2d_msaa");
constexpr ConstStringRef imageType2DMSAADepth("image_2d_msaa_depth");
constexpr ConstStringRef imageType2DArrayMSAA("image_2d_array_msaa");
constexpr ConstStringRef imageType2DArrayMSAADepth("image_2d_array_msaa_depth");
constexpr ConstStringRef imageType2DMedia("image_2d_media");
constexpr ConstStringRef imageType2DMediaBlock("image_2d_media_block");
} // namespace ImageType

namespace SamplerType {
constexpr ConstStringRef samplerTypeTexture("texture");
constexpr ConstStringRef samplerType8x8("sample_8x8");
constexpr ConstStringRef samplerType2DConsolve8x8("sample_8x8_2dconvolve");
constexpr ConstStringRef samplerTypeErode8x8("sample_8x8_erode");
constexpr ConstStringRef samplerTypeDilate8x8("sample_8x8_dilate");
constexpr ConstStringRef samplerTypeMinMaxFilter8x8("sample_8x8_minmaxfilter");
constexpr ConstStringRef samplerTypeCentroid8x8("sample_8x8_centroid");
constexpr ConstStringRef samplerTypeBoolCentroid8x8("sample_8x8_bool_centroid");
constexpr ConstStringRef samplerTypeBoolSum8x8("sample_8x8_bool_sum");
constexpr ConstStringRef samplerTypeVD("vd");
constexpr ConstStringRef samplerTypeVE("ve");
constexpr ConstStringRef samplerTypeVME("vme");
} // namespace SamplerType

namespace MemoryAddressingMode {
constexpr ConstStringRef stateless("stateless");
constexpr ConstStringRef stateful("stateful");
constexpr ConstStringRef bindless("bindless");
constexpr ConstStringRef sharedLocalMemory("slm");
} // namespace MemoryAddressingMode

namespace AddrSpace {
constexpr ConstStringRef global("global");
constexpr ConstStringRef local("local");
constexpr ConstStringRef constant("constant");
constexpr ConstStringRef image("image");
constexpr ConstStringRef sampler("sampler");
} // namespace AddrSpace

namespace AccessType {
constexpr ConstStringRef readonly("readonly");
constexpr ConstStringRef writeonly("writeonly");
constexpr ConstStringRef readwrite("readwrite");
} // namespace AccessType
} // namespace PayloadArgument

namespace BindingTableIndex {
constexpr ConstStringRef btiValue("bti_value");
constexpr ConstStringRef argIndex("arg_index");
} // namespace BindingTableIndex

namespace PerThreadPayloadArgument {
constexpr ConstStringRef argType("arg_type");
constexpr ConstStringRef offset("offset");
constexpr ConstStringRef size("size");
namespace ArgType {
constexpr ConstStringRef packedLocalIds("packed_local_ids");
constexpr ConstStringRef localId("local_id");
} // namespace ArgType
} // namespace PerThreadPayloadArgument

namespace PerThreadMemoryBuffer {
constexpr ConstStringRef allocationType("type");
constexpr ConstStringRef memoryUsage("usage");
constexpr ConstStringRef size("size");
constexpr ConstStringRef isSimtThread("is_simt_thread");
constexpr ConstStringRef slot("slot");
namespace AllocationType {
constexpr ConstStringRef global("global");
constexpr ConstStringRef scratch("scratch");
constexpr ConstStringRef slm("slm");
} // namespace AllocationType
namespace MemoryUsage {
constexpr ConstStringRef privateSpace("private_space");
constexpr ConstStringRef spillFillSpace("spill_fill_space");
constexpr ConstStringRef singleSpace("single_space");
} // namespace MemoryUsage
} // namespace PerThreadMemoryBuffer
namespace ExperimentalProperties {
constexpr ConstStringRef hasNonKernelArgLoad("has_non_kernel_arg_load");
constexpr ConstStringRef hasNonKernelArgStore("has_non_kernel_arg_store");
constexpr ConstStringRef hasNonKernelArgAtomic("has_non_kernel_arg_atomic");
} // namespace ExperimentalProperties

namespace InlineSamplers {
constexpr ConstStringRef samplerIndex("sampler_index");
constexpr ConstStringRef addrMode("addrmode");
constexpr ConstStringRef filterMode("filtermode");
constexpr ConstStringRef normalized("normalized");

namespace AddrMode {
constexpr ConstStringRef none("none");
constexpr ConstStringRef repeat("repeat");
constexpr ConstStringRef clamp_edge("clamp_edge");
constexpr ConstStringRef clamp_border("clamp_border");
constexpr ConstStringRef mirror("mirror");
} // namespace AddrMode

namespace FilterMode {
constexpr ConstStringRef nearest("nearest");
constexpr ConstStringRef linear("linear");
} // namespace FilterMode

} // namespace InlineSamplers
} // namespace Kernel

namespace GlobalHostAccessTable {
constexpr ConstStringRef deviceName("device_name");
constexpr ConstStringRef hostName("host_name");
} // namespace GlobalHostAccessTable

namespace Function {
constexpr ConstStringRef name("name");
constexpr ConstStringRef executionEnv("execution_env");
using namespace Kernel::ExecutionEnv;
} // namespace Function

namespace KernelMiscInfo {
constexpr ConstStringRef name("name");
constexpr ConstStringRef argsInfo("args_info");
namespace ArgsInfo {
constexpr ConstStringRef index("index");
constexpr ConstStringRef name("name");
constexpr ConstStringRef addressQualifier("address_qualifier");
constexpr ConstStringRef accessQualifier("access_qualifier");
constexpr ConstStringRef typeName("type_name");
constexpr ConstStringRef typeQualifiers("type_qualifiers");
} // namespace ArgsInfo
} // namespace KernelMiscInfo

} // namespace Tags

namespace Types {

struct Version {
    uint32_t major = 0U;
    uint32_t minor = 0U;
};

namespace Kernel {
namespace ExecutionEnv {
enum ThreadSchedulingMode : uint8_t {
    ThreadSchedulingModeUnknown,
    ThreadSchedulingModeAgeBased,
    ThreadSchedulingModeRoundRobin,
    ThreadSchedulingModeRoundRobinStall,
    ThreadSchedulingModeMax
};

using ActualKernelStartOffsetT = int32_t;
using BarrierCountT = int32_t;
using DisableMidThreadPreemptionT = bool;
using GrfCountT = int32_t;
using Has4GBBuffersT = bool;
using HasDpasT = bool;
using HasFenceForImageAccessT = bool;
using HasGlobalAtomicsT = bool;
using HasMultiScratchSpacesT = bool;
using HasNonKernelArgAtomicT = int32_t;
using HasNonKernelArgLoadT = int32_t;
using HasNonKernelArgStoreT = int32_t;
using HasNoStatelessWriteT = bool;
using HasStackCallsT = bool;
using HwPreemptionModeT = int32_t;
using InlineDataPayloadSizeT = int32_t;
using OffsetToSkipPerThreadDataLoadT = int32_t;
using OffsetToSkipSetFfidGpT = int32_t;
using RequiredSubGroupSizeT = int32_t;
using RequiredWorkGroupSizeT = int32_t[3];
using RequireDisableEUFusionT = bool;
using SimdSizeT = int32_t;
using SlmSizeT = int32_t;
using SubgroupIndependentForwardProgressT = bool;
using WorkgroupWalkOrderDimensionsT = int32_t[3];
using ThreadSchedulingModeT = ThreadSchedulingMode;
using IndirectStatelessCountT = int32_t;

namespace Defaults {
constexpr BarrierCountT barrierCount = 0;
constexpr DisableMidThreadPreemptionT disableMidThreadPreemption = false;
constexpr Has4GBBuffersT has4GBBuffers = false;
constexpr HasDpasT hasDpas = false;
constexpr HasFenceForImageAccessT hasFenceForImageAccess = false;
constexpr HasGlobalAtomicsT hasGlobalAtomics = false;
constexpr HasMultiScratchSpacesT hasMultiScratchSpaces = false;
constexpr HasNonKernelArgAtomicT hasNonKernelArgAtomic = false;
constexpr HasNonKernelArgLoadT hasNonKernelArgLoad = false;
constexpr HasNonKernelArgStoreT hasNonKernelArgStore = false;
constexpr HasNoStatelessWriteT hasNoStatelessWrite = false;
constexpr HasStackCallsT hasStackCalls = false;
constexpr HwPreemptionModeT hwPreemptionMode = -1;
constexpr InlineDataPayloadSizeT inlineDataPayloadSize = 0;
constexpr OffsetToSkipPerThreadDataLoadT offsetToSkipPerThreadDataLoad = 0;
constexpr OffsetToSkipSetFfidGpT offsetToSkipSetFfidGp = 0;
constexpr RequiredSubGroupSizeT requiredSubGroupSize = 0;
constexpr RequiredWorkGroupSizeT requiredWorkGroupSize = {0, 0, 0};
constexpr RequireDisableEUFusionT requireDisableEUFusion = false;
constexpr SlmSizeT slmSize = 0;
constexpr SubgroupIndependentForwardProgressT subgroupIndependentForwardProgress = false;
constexpr WorkgroupWalkOrderDimensionsT workgroupWalkOrderDimensions = {0, 1, 2};
constexpr ThreadSchedulingModeT threadSchedulingMode = ThreadSchedulingModeUnknown;
constexpr IndirectStatelessCountT indirectStatelessCount = 0;
} // namespace Defaults

constexpr ConstStringRef required[] = {
    Tags::Kernel::ExecutionEnv::grfCount,
    Tags::Kernel::ExecutionEnv::simdSize};

struct ExecutionEnvBaseT {
    BarrierCountT barrierCount = Defaults::barrierCount;
    DisableMidThreadPreemptionT disableMidThreadPreemption = Defaults::disableMidThreadPreemption;
    GrfCountT grfCount = -1;
    Has4GBBuffersT has4GBBuffers = Defaults::has4GBBuffers;
    HasDpasT hasDpas = Defaults::hasDpas;
    HasFenceForImageAccessT hasFenceForImageAccess = Defaults::hasFenceForImageAccess;
    HasGlobalAtomicsT hasGlobalAtomics = Defaults::hasGlobalAtomics;
    HasMultiScratchSpacesT hasMultiScratchSpaces = Defaults::hasMultiScratchSpaces;
    HasNoStatelessWriteT hasNoStatelessWrite = Defaults::hasNoStatelessWrite;
    HasStackCallsT hasStackCalls = Defaults::hasStackCalls;
    HwPreemptionModeT hwPreemptionMode = Defaults::hwPreemptionMode;
    InlineDataPayloadSizeT inlineDataPayloadSize = Defaults::inlineDataPayloadSize;
    OffsetToSkipPerThreadDataLoadT offsetToSkipPerThreadDataLoad = Defaults::offsetToSkipPerThreadDataLoad;
    OffsetToSkipSetFfidGpT offsetToSkipSetFfidGp = Defaults::offsetToSkipSetFfidGp;
    RequiredSubGroupSizeT requiredSubGroupSize = Defaults::requiredSubGroupSize;
    RequiredWorkGroupSizeT requiredWorkGroupSize = {Defaults::requiredWorkGroupSize[0], Defaults::requiredWorkGroupSize[1], Defaults::requiredWorkGroupSize[2]};
    RequireDisableEUFusionT requireDisableEUFusion = Defaults::requireDisableEUFusion;
    SimdSizeT simdSize = -1;
    SlmSizeT slmSize = Defaults::slmSize;
    SubgroupIndependentForwardProgressT subgroupIndependentForwardProgress = Defaults::subgroupIndependentForwardProgress;
    WorkgroupWalkOrderDimensionsT workgroupWalkOrderDimensions{Defaults::workgroupWalkOrderDimensions[0], Defaults::workgroupWalkOrderDimensions[1], Defaults::workgroupWalkOrderDimensions[2]};
    ThreadSchedulingModeT threadSchedulingMode = Defaults::threadSchedulingMode;
    IndirectStatelessCountT indirectStatelessCount = Defaults::indirectStatelessCount;
};

struct ExperimentalPropertiesBaseT {
    HasNonKernelArgLoadT hasNonKernelArgLoad = Defaults::hasNonKernelArgLoad;
    HasNonKernelArgStoreT hasNonKernelArgStore = Defaults::hasNonKernelArgStore;
    HasNonKernelArgAtomicT hasNonKernelArgAtomic = Defaults::hasNonKernelArgAtomic;
};

} // namespace ExecutionEnv

namespace Attributes {
using IntelReqdSubgroupSizeT = int32_t;
using IntelReqdWorkgroupWalkOrder = std::array<int32_t, 3>;
using ReqdWorkgroupSizeT = std::array<int32_t, 3>;
using InvalidKernelT = ConstStringRef;
using WorkgroupSizeHint = std::array<int32_t, 3>;
using VecTypeHintT = ConstStringRef;

namespace Defaults {
constexpr IntelReqdSubgroupSizeT intelReqdSubgroupSize = 0;
constexpr IntelReqdWorkgroupWalkOrder intelReqdWorkgroupWalkOrder = {0, 0, 0};
constexpr ReqdWorkgroupSizeT reqdWorkgroupSize = {0, 0, 0};
constexpr WorkgroupSizeHint workgroupSizeHint = {0, 0, 0};
} // namespace Defaults

struct AttributesBaseT {
    std::optional<IntelReqdSubgroupSizeT> intelReqdSubgroupSize;
    std::optional<IntelReqdWorkgroupWalkOrder> intelReqdWorkgroupWalkOrder;
    std::optional<ReqdWorkgroupSizeT> reqdWorkgroupSize;
    std::optional<InvalidKernelT> invalidKernel;
    std::optional<WorkgroupSizeHint> workgroupSizeHint;
    std::optional<VecTypeHintT> vecTypeHint;
    std::vector<std::pair<ConstStringRef, ConstStringRef>> otherHints;
};
} // namespace Attributes

namespace DebugEnv {
using DebugSurfaceBTIT = int32_t;

namespace Defaults {
constexpr DebugSurfaceBTIT debugSurfaceBTI = -1;
} // namespace Defaults

struct DebugEnvBaseT {
    DebugSurfaceBTIT debugSurfaceBTI = Defaults::debugSurfaceBTI;
};
} // namespace DebugEnv

enum ArgType : uint8_t {
    ArgTypeUnknown = 0,
    ArgTypePackedLocalIds = 1,
    ArgTypeLocalId,
    ArgTypeLocalSize,
    ArgTypeGroupCount,
    ArgTypeGlobalSize,
    ArgTypeEnqueuedLocalSize,
    ArgTypeGlobalIdOffset,
    ArgTypePrivateBaseStateless,
    ArgTypeArgByvalue,
    ArgTypeArgBypointer,
    ArgTypeBufferAddress,
    ArgTypeBufferOffset,
    ArgTypePrintfBuffer,
    ArgTypeWorkDimensions,
    ArgTypeImplicitArgBuffer,
    ArgTypeImageWidth,
    ArgTypeImageHeight,
    ArgTypeImageDepth,
    ArgTypeImageChannelDataType,
    ArgTypeImageChannelOrder,
    ArgTypeImageArraySize,
    ArgTypeImageNumSamples,
    ArgTypeImageMipLevels,
    ArgTypeImageFlatBaseOffset,
    ArgTypeImageFlatWidth,
    ArgTypeImageFlatHeight,
    ArgTypeImageFlatPitch,
    ArgTypeSamplerSnapWa,
    ArgTypeSamplerNormCoords,
    ArgTypeSamplerAddrMode,
    ArgTypeVmeMbBlockType,
    ArgTypeVmeSubpixelMode,
    ArgTypeVmeSadAdjustMode,
    ArgTypeVmeSearchPathType,
    ArgTypeMax
};

namespace PerThreadPayloadArgument {

using OffsetT = int32_t;
using SizeT = int32_t;
using ArgTypeT = ArgType;

namespace Defaults {
}

struct PerThreadPayloadArgumentBaseT {
    ArgTypeT argType = ArgTypeUnknown;
    OffsetT offset = -1;
    SizeT size = -1;
};

} // namespace PerThreadPayloadArgument

namespace PayloadArgument {

enum MemoryAddressingMode : uint8_t {
    MemoryAddressingModeUnknown = 0,
    MemoryAddressingModeStateful = 1,
    MemoryAddressingModeStateless,
    MemoryAddressingModeBindless,
    MemoryAddressingModeSharedLocalMemory,
    MemoryAddressIngModeMax
};

enum AddressSpace : uint8_t {
    AddressSpaceUnknown = 0,
    AddressSpaceGlobal = 1,
    AddressSpaceLocal,
    AddressSpaceConstant,
    AddressSpaceImage,
    AddressSpaceSampler,
    AddressSpaceMax
};

enum AccessType : uint8_t {
    AccessTypeUnknown = 0,
    AccessTypeReadonly = 1,
    AccessTypeWriteonly,
    AccessTypeReadwrite,
    AccessTypeMax
};

enum ImageType : uint8_t {
    ImageTypeUnknown,
    ImageTypeBuffer,
    ImageType1D,
    ImageType1DArray,
    ImageType2D,
    ImageType2DArray,
    ImageType3D,
    ImageTypeCube,
    ImageTypeCubeArray,
    ImageType2DDepth,
    ImageType2DArrayDepth,
    ImageType2DMSAA,
    ImageType2DMSAADepth,
    ImageType2DArrayMSAA,
    ImageType2DArrayMSAADepth,
    ImageType2DMedia,
    ImageType2DMediaBlock,
    ImageTypeMax
};

enum SamplerType : uint8_t {
    SamplerTypeUnknown,
    SamplerTypeTexture,
    SamplerType8x8,
    SamplerType2DConvolve8x8,
    SamplerTypeErode8x8,
    SamplerTypeDilate8x8,
    SamplerTypeMinMaxFilter8x8,
    SamplerTypeCentroid8x8,
    SamplerTypeBoolCentroid8x8,
    SamplerTypeBoolSum8x8,
    SamplerTypeVME,
    SamplerTypeVE,
    SamplerTypeVD,
    SamplerTypeMax
};

using ArgTypeT = ArgType;
using OffseT = int32_t;
using SourceOffseT = int32_t;
using SizeT = int32_t;
using ArgIndexT = int32_t;
using AddrmodeT = MemoryAddressingMode;
using AddrspaceT = AddressSpace;
using AccessTypeT = AccessType;
using SlmAlignmentT = uint8_t;
using SamplerIndexT = int32_t;

namespace Defaults {
constexpr ArgIndexT argIndex = -1;
constexpr SlmAlignmentT slmArgAlignment = 16U;
constexpr SamplerIndexT samplerIndex = -1;
constexpr SourceOffseT sourceOffset = -1;
} // namespace Defaults

struct PayloadArgumentBaseT {
    ArgTypeT argType = ArgTypeUnknown;
    OffseT offset = 0;
    SourceOffseT sourceOffset = Defaults::sourceOffset;
    SizeT size = 0;
    ArgIndexT argIndex = Defaults::argIndex;
    AddrmodeT addrmode = MemoryAddressingModeUnknown;
    AddrspaceT addrspace = AddressSpaceUnknown;
    AccessTypeT accessType = AccessTypeUnknown;
    SamplerIndexT samplerIndex = Defaults::samplerIndex;
    SlmAlignmentT slmArgAlignment = Defaults::slmArgAlignment;
    ImageType imageType = ImageTypeUnknown;
    bool imageTransformable = false;
    SamplerType samplerType = SamplerTypeUnknown;
};

} // namespace PayloadArgument

namespace BindingTableEntry {
using BtiValueT = int32_t;
using ArgIndexT = int32_t;
struct BindingTableEntryBaseT {
    BtiValueT btiValue = 0U;
    ArgIndexT argIndex = 0U;
};
} // namespace BindingTableEntry

namespace PerThreadMemoryBuffer {
enum AllocationType : uint8_t {
    AllocationTypeUnknown = 0,
    AllocationTypeGlobal,
    AllocationTypeScratch,
    AllocationTypeSlm,
    AllocationTypeMax
};

enum MemoryUsage : uint8_t {
    MemoryUsageUnknown = 0,
    MemoryUsagePrivateSpace,
    MemoryUsageSpillFillSpace,
    MemoryUsageSingleSpace,
    MemoryUsageMax
};

using SizeT = int32_t;
using AllocationTypeT = AllocationType;
using MemoryUsageT = MemoryUsage;
using IsSimtThreadT = bool;
using Slot = int32_t;

namespace Defaults {
constexpr IsSimtThreadT isSimtThread = false;
constexpr Slot slot = 0U;
} // namespace Defaults

struct PerThreadMemoryBufferBaseT {
    AllocationType allocationType = AllocationTypeUnknown;
    MemoryUsageT memoryUsage = MemoryUsageUnknown;
    SizeT size = 0U;
    IsSimtThreadT isSimtThread = Defaults::isSimtThread;
    Slot slot = Defaults::slot;
};
} // namespace PerThreadMemoryBuffer

namespace InlineSamplers {
enum class AddrMode : uint8_t {
    Unknown,
    None,
    Repeat,
    ClampEdge,
    ClampBorder,
    Mirror,
    Max
};

enum FilterMode {
    Unknown,
    Nearest,
    Linear,
    Max
};

using SamplerIndexT = int32_t;
using AddrModeT = AddrMode;
using FilterModeT = FilterMode;
using NormalizedT = bool;

namespace Defaults {
constexpr SamplerIndexT samplerIndex = -1;
constexpr AddrModeT addrMode = AddrMode::Unknown;
constexpr FilterModeT filterMode = FilterMode::Unknown;
constexpr NormalizedT normalized = false;
}; // namespace Defaults

struct InlineSamplerBaseT {
    SamplerIndexT samplerIndex = Defaults::samplerIndex;
    AddrModeT addrMode = Defaults::addrMode;
    FilterModeT filterMode = Defaults::filterMode;
    NormalizedT normalized = Defaults::normalized;
};
} // namespace InlineSamplers

} // namespace Kernel

namespace GlobalHostAccessTable {
struct globalHostAccessTableT {
    std::string deviceName;
    std::string hostName;
};
} // namespace GlobalHostAccessTable

namespace Function {
namespace ExecutionEnv {
using namespace Kernel::ExecutionEnv;
}
} // namespace Function

namespace Miscellaneous {
using ArgIndexT = uint32_t;
struct KernelArgMiscInfoT {
    ArgIndexT index;
    std::string kernelName;
    std::string argName;
    std::string accessQualifier;
    std::string addressQualifier;
    std::string typeName;
    std::string typeQualifiers;
};
} // namespace Miscellaneous

} // namespace Types

} // namespace ZebinKernelMetadata

} // namespace Elf

} // namespace NEO
