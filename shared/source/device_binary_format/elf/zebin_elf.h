/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/arrayref.h"
#include "shared/source/utilities/const_stringref.h"

#include <array>
#include <optional>
#include <vector>

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
inline constexpr ConstStringRef textPrefix = ".text.";
inline constexpr ConstStringRef functions = ".text.Intel_Symbol_Table_Void_Program";
inline constexpr ConstStringRef dataConst = ".data.const";
inline constexpr ConstStringRef dataGlobalConst = ".data.global_const";
inline constexpr ConstStringRef dataGlobal = ".data.global";
inline constexpr ConstStringRef dataConstString = ".data.const.string";
inline constexpr ConstStringRef symtab = ".symtab";
inline constexpr ConstStringRef relTablePrefix = ".rel.";
inline constexpr ConstStringRef relaTablePrefix = ".rela.";
inline constexpr ConstStringRef spv = ".spv";
inline constexpr ConstStringRef debugPrefix = ".debug_";
inline constexpr ConstStringRef debugInfo = ".debug_info";
inline constexpr ConstStringRef debugAbbrev = ".debug_abbrev";
inline constexpr ConstStringRef zeInfo = ".ze_info";
inline constexpr ConstStringRef gtpinInfo = ".gtpin_info";
inline constexpr ConstStringRef noteIntelGT = ".note.intelgt.compat";
inline constexpr ConstStringRef buildOptions = ".misc.buildOptions";
inline constexpr ConstStringRef vIsaAsmPrefix = ".visaasm.";
inline constexpr ConstStringRef externalFunctions = "Intel_Symbol_Table_Void_Program";
} // namespace SectionsNamesZebin

inline constexpr ConstStringRef IntelGtNoteOwnerName = "IntelGT";
enum IntelGTSectionType : uint32_t {
    ProductFamily = 1,
    GfxCore = 2,
    TargetMetadata = 3,
    ZebinVersion = 4,
    ProductConfig = 5,
    LastSupported = ProductConfig
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
inline constexpr ConstStringRef kernels("kernels");
inline constexpr ConstStringRef version("version");
inline constexpr ConstStringRef globalHostAccessTable("global_host_access_table");
inline constexpr ConstStringRef functions("functions");
inline constexpr ConstStringRef kernelMiscInfo("kernels_misc_info");

namespace Kernel {
inline constexpr ConstStringRef attributes("user_attributes");
inline constexpr ConstStringRef name("name");
inline constexpr ConstStringRef executionEnv("execution_env");
inline constexpr ConstStringRef debugEnv("debug_env");
inline constexpr ConstStringRef payloadArguments("payload_arguments");
inline constexpr ConstStringRef bindingTableIndices("binding_table_indices");
inline constexpr ConstStringRef perThreadPayloadArguments("per_thread_payload_arguments");
inline constexpr ConstStringRef perThreadMemoryBuffers("per_thread_memory_buffers");
inline constexpr ConstStringRef experimentalProperties("experimental_properties");
inline constexpr ConstStringRef inlineSamplers("inline_samplers");

namespace ExecutionEnv {
inline constexpr ConstStringRef barrierCount("barrier_count");
inline constexpr ConstStringRef disableMidThreadPreemption("disable_mid_thread_preemption");
inline constexpr ConstStringRef euThreadCount("eu_thread_count");
inline constexpr ConstStringRef grfCount("grf_count");
inline constexpr ConstStringRef has4gbBuffers("has_4gb_buffers");
inline constexpr ConstStringRef hasDpas("has_dpas");
inline constexpr ConstStringRef hasFenceForImageAccess("has_fence_for_image_access");
inline constexpr ConstStringRef hasGlobalAtomics("has_global_atomics");
inline constexpr ConstStringRef hasMultiScratchSpaces("has_multi_scratch_spaces");
inline constexpr ConstStringRef hasNoStatelessWrite("has_no_stateless_write");
inline constexpr ConstStringRef hasStackCalls("has_stack_calls");
inline constexpr ConstStringRef hwPreemptionMode("hw_preemption_mode");
inline constexpr ConstStringRef inlineDataPayloadSize("inline_data_payload_size");
inline constexpr ConstStringRef offsetToSkipPerThreadDataLoad("offset_to_skip_per_thread_data_load");
inline constexpr ConstStringRef offsetToSkipSetFfidGp("offset_to_skip_set_ffid_gp");
inline constexpr ConstStringRef requiredSubGroupSize("required_sub_group_size");
inline constexpr ConstStringRef requiredWorkGroupSize("required_work_group_size");
inline constexpr ConstStringRef requireDisableEUFusion("require_disable_eufusion");
inline constexpr ConstStringRef simdSize("simd_size");
inline constexpr ConstStringRef slmSize("slm_size");
inline constexpr ConstStringRef subgroupIndependentForwardProgress("subgroup_independent_forward_progress");
inline constexpr ConstStringRef workGroupWalkOrderDimensions("work_group_walk_order_dimensions");
inline constexpr ConstStringRef threadSchedulingMode("thread_scheduling_mode");
inline constexpr ConstStringRef hasSample("has_sample");
namespace ThreadSchedulingMode {
inline constexpr ConstStringRef ageBased("age_based");
inline constexpr ConstStringRef roundRobin("round_robin");
inline constexpr ConstStringRef roundRobinStall("round_robin_stall");
} // namespace ThreadSchedulingMode
inline constexpr ConstStringRef indirectStatelessCount("indirect_stateless_count");
} // namespace ExecutionEnv

namespace Attributes {
inline constexpr ConstStringRef intelReqdSubgroupSize("intel_reqd_sub_group_size");
inline constexpr ConstStringRef intelReqdWorkgroupWalkOrder("intel_reqd_workgroup_walk_order");
inline constexpr ConstStringRef reqdWorkgroupSize("reqd_work_group_size");
inline constexpr ConstStringRef invalidKernel("invalid_kernel");
inline constexpr ConstStringRef vecTypeHint("vec_type_hint");
inline constexpr ConstStringRef workgroupSizeHint("work_group_size_hint");
inline constexpr ConstStringRef hintSuffix("_hint");
} // namespace Attributes

namespace DebugEnv {
inline constexpr ConstStringRef debugSurfaceBTI("sip_surface_bti");
} // namespace DebugEnv

namespace PayloadArgument {
inline constexpr ConstStringRef argType("arg_type");
inline constexpr ConstStringRef argIndex("arg_index");
inline constexpr ConstStringRef offset("offset");
inline constexpr ConstStringRef size("size");
inline constexpr ConstStringRef addrmode("addrmode");
inline constexpr ConstStringRef addrspace("addrspace");
inline constexpr ConstStringRef accessType("access_type");
inline constexpr ConstStringRef samplerIndex("sampler_index");
inline constexpr ConstStringRef sourceOffset("source_offset");
inline constexpr ConstStringRef slmArgAlignment("slm_alignment");
inline constexpr ConstStringRef imageType("image_type");
inline constexpr ConstStringRef imageTransformable("image_transformable");
inline constexpr ConstStringRef samplerType("sampler_type");
inline constexpr ConstStringRef addrMode("sampler_desc_addrmode");
inline constexpr ConstStringRef filterMode("sampler_desc_filtermode");
inline constexpr ConstStringRef normalized("sampler_desc_normalized");
inline constexpr ConstStringRef isPipe("is_pipe");
inline constexpr ConstStringRef isPtr("is_ptr");
inline constexpr ConstStringRef btiValue("bti_value");

namespace ArgType {
inline constexpr ConstStringRef localSize("local_size");
inline constexpr ConstStringRef groupCount("group_count");
inline constexpr ConstStringRef globalIdOffset("global_id_offset");
inline constexpr ConstStringRef globalSize("global_size");
inline constexpr ConstStringRef enqueuedLocalSize("enqueued_local_size");
inline constexpr ConstStringRef privateBaseStateless("private_base_stateless");
inline constexpr ConstStringRef argByvalue("arg_byvalue");
inline constexpr ConstStringRef argBypointer("arg_bypointer");
inline constexpr ConstStringRef bufferAddress("buffer_address");
inline constexpr ConstStringRef bufferOffset("buffer_offset");
inline constexpr ConstStringRef printfBuffer("printf_buffer");
inline constexpr ConstStringRef workDimensions("work_dimensions");
inline constexpr ConstStringRef implicitArgBuffer("implicit_arg_buffer");
inline constexpr ConstStringRef syncBuffer("sync_buffer");
inline constexpr ConstStringRef rtGlobalBuffer("rt_global_buffer");
inline constexpr ConstStringRef dataConstBuffer("const_base");
inline constexpr ConstStringRef dataGlobalBuffer("global_base");

namespace Image {
inline constexpr ConstStringRef width("image_width");
inline constexpr ConstStringRef height("image_height");
inline constexpr ConstStringRef depth("image_depth");
inline constexpr ConstStringRef channelDataType("image_channel_data_type");
inline constexpr ConstStringRef channelOrder("image_channel_order");
inline constexpr ConstStringRef arraySize("image_array_size");
inline constexpr ConstStringRef numSamples("image_num_samples");
inline constexpr ConstStringRef numMipLevels("image_num_mip_levels");
inline constexpr ConstStringRef flatBaseOffset("flat_image_baseoffset");
inline constexpr ConstStringRef flatWidth("flat_image_width");
inline constexpr ConstStringRef flatHeight("flat_image_height");
inline constexpr ConstStringRef flatPitch("flat_image_pitch");
} // namespace Image
namespace Sampler {
inline constexpr ConstStringRef snapWa("sampler_snap_wa");
inline constexpr ConstStringRef normCoords("sampler_normalized");
inline constexpr ConstStringRef addrMode("sampler_address");
namespace Vme {
inline constexpr ConstStringRef blockType("vme_mb_block_type");
inline constexpr ConstStringRef subpixelMode("vme_subpixel_mode");
inline constexpr ConstStringRef sadAdjustMode("vme_sad_adjust_mode");
inline constexpr ConstStringRef searchPathType("vme_search_path_type");
} // namespace Vme
} // namespace Sampler
} // namespace ArgType
namespace ImageType {
inline constexpr ConstStringRef imageTypeBuffer("image_buffer");
inline constexpr ConstStringRef imageType1D("image_1d");
inline constexpr ConstStringRef imageType1DArray("image_1d_array");
inline constexpr ConstStringRef imageType2D("image_2d");
inline constexpr ConstStringRef imageType2DArray("image_2d_array");
inline constexpr ConstStringRef imageType3D("image_3d");
inline constexpr ConstStringRef imageTypeCube("image_cube_array");
inline constexpr ConstStringRef imageTypeCubeArray("image_buffer");
inline constexpr ConstStringRef imageType2DDepth("image_2d_depth");
inline constexpr ConstStringRef imageType2DArrayDepth("image_2d_array_depth");
inline constexpr ConstStringRef imageType2DMSAA("image_2d_msaa");
inline constexpr ConstStringRef imageType2DMSAADepth("image_2d_msaa_depth");
inline constexpr ConstStringRef imageType2DArrayMSAA("image_2d_array_msaa");
inline constexpr ConstStringRef imageType2DArrayMSAADepth("image_2d_array_msaa_depth");
inline constexpr ConstStringRef imageType2DMedia("image_2d_media");
inline constexpr ConstStringRef imageType2DMediaBlock("image_2d_media_block");
} // namespace ImageType

namespace SamplerType {
inline constexpr ConstStringRef samplerTypeTexture("texture");
inline constexpr ConstStringRef samplerType8x8("sample_8x8");
inline constexpr ConstStringRef samplerType2DConsolve8x8("sample_8x8_2dconvolve");
inline constexpr ConstStringRef samplerTypeErode8x8("sample_8x8_erode");
inline constexpr ConstStringRef samplerTypeDilate8x8("sample_8x8_dilate");
inline constexpr ConstStringRef samplerTypeMinMaxFilter8x8("sample_8x8_minmaxfilter");
inline constexpr ConstStringRef samplerTypeCentroid8x8("sample_8x8_centroid");
inline constexpr ConstStringRef samplerTypeBoolCentroid8x8("sample_8x8_bool_centroid");
inline constexpr ConstStringRef samplerTypeBoolSum8x8("sample_8x8_bool_sum");
inline constexpr ConstStringRef samplerTypeVD("vd");
inline constexpr ConstStringRef samplerTypeVE("ve");
inline constexpr ConstStringRef samplerTypeVME("vme");
} // namespace SamplerType

namespace MemoryAddressingMode {
inline constexpr ConstStringRef stateless("stateless");
inline constexpr ConstStringRef stateful("stateful");
inline constexpr ConstStringRef bindless("bindless");
inline constexpr ConstStringRef sharedLocalMemory("slm");
} // namespace MemoryAddressingMode

namespace AddrSpace {
inline constexpr ConstStringRef global("global");
inline constexpr ConstStringRef local("local");
inline constexpr ConstStringRef constant("constant");
inline constexpr ConstStringRef image("image");
inline constexpr ConstStringRef sampler("sampler");
} // namespace AddrSpace

namespace AccessType {
inline constexpr ConstStringRef readonly("readonly");
inline constexpr ConstStringRef writeonly("writeonly");
inline constexpr ConstStringRef readwrite("readwrite");
} // namespace AccessType
} // namespace PayloadArgument

namespace BindingTableIndex {
inline constexpr ConstStringRef btiValue("bti_value");
inline constexpr ConstStringRef argIndex("arg_index");
} // namespace BindingTableIndex

namespace PerThreadPayloadArgument {
inline constexpr ConstStringRef argType("arg_type");
inline constexpr ConstStringRef offset("offset");
inline constexpr ConstStringRef size("size");
namespace ArgType {
inline constexpr ConstStringRef packedLocalIds("packed_local_ids");
inline constexpr ConstStringRef localId("local_id");
} // namespace ArgType
} // namespace PerThreadPayloadArgument

namespace PerThreadMemoryBuffer {
inline constexpr ConstStringRef allocationType("type");
inline constexpr ConstStringRef memoryUsage("usage");
inline constexpr ConstStringRef size("size");
inline constexpr ConstStringRef isSimtThread("is_simt_thread");
inline constexpr ConstStringRef slot("slot");
namespace AllocationType {
inline constexpr ConstStringRef global("global");
inline constexpr ConstStringRef scratch("scratch");
inline constexpr ConstStringRef slm("slm");
} // namespace AllocationType
namespace MemoryUsage {
inline constexpr ConstStringRef privateSpace("private_space");
inline constexpr ConstStringRef spillFillSpace("spill_fill_space");
inline constexpr ConstStringRef singleSpace("single_space");
} // namespace MemoryUsage
} // namespace PerThreadMemoryBuffer
namespace ExperimentalProperties {
inline constexpr ConstStringRef hasNonKernelArgLoad("has_non_kernel_arg_load");
inline constexpr ConstStringRef hasNonKernelArgStore("has_non_kernel_arg_store");
inline constexpr ConstStringRef hasNonKernelArgAtomic("has_non_kernel_arg_atomic");
} // namespace ExperimentalProperties

namespace InlineSamplers {
inline constexpr ConstStringRef samplerIndex("sampler_index");
inline constexpr ConstStringRef addrMode("addrmode");
inline constexpr ConstStringRef filterMode("filtermode");
inline constexpr ConstStringRef normalized("normalized");

namespace AddrMode {
inline constexpr ConstStringRef none("none");
inline constexpr ConstStringRef repeat("repeat");
inline constexpr ConstStringRef clamp_edge("clamp_edge");
inline constexpr ConstStringRef clamp_border("clamp_border");
inline constexpr ConstStringRef mirror("mirror");
} // namespace AddrMode

namespace FilterMode {
inline constexpr ConstStringRef nearest("nearest");
inline constexpr ConstStringRef linear("linear");
} // namespace FilterMode

} // namespace InlineSamplers
} // namespace Kernel

namespace GlobalHostAccessTable {
inline constexpr ConstStringRef deviceName("device_name");
inline constexpr ConstStringRef hostName("host_name");
} // namespace GlobalHostAccessTable

namespace Function {
inline constexpr ConstStringRef name("name");
inline constexpr ConstStringRef executionEnv("execution_env");
using namespace Kernel::ExecutionEnv;
} // namespace Function

namespace KernelMiscInfo {
inline constexpr ConstStringRef name("name");
inline constexpr ConstStringRef argsInfo("args_info");
namespace ArgsInfo {
inline constexpr ConstStringRef index("index");
inline constexpr ConstStringRef name("name");
inline constexpr ConstStringRef addressQualifier("address_qualifier");
inline constexpr ConstStringRef accessQualifier("access_qualifier");
inline constexpr ConstStringRef typeName("type_name");
inline constexpr ConstStringRef typeQualifiers("type_qualifiers");
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
using EuThreadCountT = int32_t;
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
using HasSampleT = bool;

namespace Defaults {
inline constexpr BarrierCountT barrierCount = 0;
inline constexpr DisableMidThreadPreemptionT disableMidThreadPreemption = false;
inline constexpr EuThreadCountT euThreadCount = 0;
inline constexpr Has4GBBuffersT has4GBBuffers = false;
inline constexpr HasDpasT hasDpas = false;
inline constexpr HasFenceForImageAccessT hasFenceForImageAccess = false;
inline constexpr HasGlobalAtomicsT hasGlobalAtomics = false;
inline constexpr HasMultiScratchSpacesT hasMultiScratchSpaces = false;
inline constexpr HasNonKernelArgAtomicT hasNonKernelArgAtomic = false;
inline constexpr HasNonKernelArgLoadT hasNonKernelArgLoad = false;
inline constexpr HasNonKernelArgStoreT hasNonKernelArgStore = false;
inline constexpr HasNoStatelessWriteT hasNoStatelessWrite = false;
inline constexpr HasStackCallsT hasStackCalls = false;
inline constexpr HwPreemptionModeT hwPreemptionMode = -1;
inline constexpr InlineDataPayloadSizeT inlineDataPayloadSize = 0;
inline constexpr OffsetToSkipPerThreadDataLoadT offsetToSkipPerThreadDataLoad = 0;
inline constexpr OffsetToSkipSetFfidGpT offsetToSkipSetFfidGp = 0;
inline constexpr RequiredSubGroupSizeT requiredSubGroupSize = 0;
inline constexpr RequiredWorkGroupSizeT requiredWorkGroupSize = {0, 0, 0};
inline constexpr RequireDisableEUFusionT requireDisableEUFusion = false;
inline constexpr SlmSizeT slmSize = 0;
inline constexpr SubgroupIndependentForwardProgressT subgroupIndependentForwardProgress = false;
inline constexpr WorkgroupWalkOrderDimensionsT workgroupWalkOrderDimensions = {0, 1, 2};
inline constexpr ThreadSchedulingModeT threadSchedulingMode = ThreadSchedulingModeUnknown;
inline constexpr IndirectStatelessCountT indirectStatelessCount = 0;
inline constexpr HasSampleT hasSample = false;
} // namespace Defaults

inline constexpr ConstStringRef required[] = {
    Tags::Kernel::ExecutionEnv::grfCount,
    Tags::Kernel::ExecutionEnv::simdSize};

struct ExecutionEnvBaseT {
    BarrierCountT barrierCount = Defaults::barrierCount;
    DisableMidThreadPreemptionT disableMidThreadPreemption = Defaults::disableMidThreadPreemption;
    EuThreadCountT euThreadCount = Defaults::euThreadCount;
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
    HasSampleT hasSample = Defaults::hasSample;
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
inline constexpr IntelReqdSubgroupSizeT intelReqdSubgroupSize = 0;
inline constexpr IntelReqdWorkgroupWalkOrder intelReqdWorkgroupWalkOrder = {0, 0, 0};
inline constexpr ReqdWorkgroupSizeT reqdWorkgroupSize = {0, 0, 0};
inline constexpr WorkgroupSizeHint workgroupSizeHint = {0, 0, 0};
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
inline constexpr DebugSurfaceBTIT debugSurfaceBTI = -1;
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
    ArgTypeSyncBuffer,
    ArgTypeRtGlobalBuffer,
    ArgTypeDataConstBuffer,
    ArgTypeDataGlobalBuffer,
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
using OffsetT = int32_t;
using SourceOffseT = int32_t;
using SizeT = int32_t;
using ArgIndexT = int32_t;
using AddrmodeT = MemoryAddressingMode;
using AddrspaceT = AddressSpace;
using AccessTypeT = AccessType;
using SlmAlignmentT = uint8_t;
using SamplerIndexT = int32_t;
using BtiValueT = int32_t;

namespace Defaults {
inline constexpr ArgIndexT argIndex = -1;
inline constexpr SlmAlignmentT slmArgAlignment = 16U;
inline constexpr SamplerIndexT samplerIndex = -1;
inline constexpr SourceOffseT sourceOffset = -1;
inline constexpr OffsetT offset = -1;
inline constexpr BtiValueT btiValue = -1;
} // namespace Defaults

struct PayloadArgumentBaseT {
    ArgTypeT argType = ArgTypeUnknown;
    OffsetT offset = Defaults::offset;
    SourceOffseT sourceOffset = Defaults::sourceOffset;
    SizeT size = 0;
    ArgIndexT argIndex = Defaults::argIndex;
    BtiValueT btiValue = Defaults::btiValue;
    AddrmodeT addrmode = MemoryAddressingModeUnknown;
    AddrspaceT addrspace = AddressSpaceUnknown;
    AccessTypeT accessType = AccessTypeUnknown;
    SamplerIndexT samplerIndex = Defaults::samplerIndex;
    SlmAlignmentT slmArgAlignment = Defaults::slmArgAlignment;
    ImageType imageType = ImageTypeUnknown;
    SamplerType samplerType = SamplerTypeUnknown;
    bool imageTransformable = false;
    bool isPipe = false;
    bool isPtr = false;
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
inline constexpr IsSimtThreadT isSimtThread = false;
inline constexpr Slot slot = 0U;
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
inline constexpr SamplerIndexT samplerIndex = -1;
inline constexpr AddrModeT addrMode = AddrMode::Unknown;
inline constexpr FilterModeT filterMode = FilterMode::Unknown;
inline constexpr NormalizedT normalized = false;
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
using ArgIndexT = int32_t;
struct KernelArgMiscInfoT {
    ArgIndexT index = -1;
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
