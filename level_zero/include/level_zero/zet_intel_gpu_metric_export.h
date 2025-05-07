/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef _ZET_INTEL_GPU_METRIC_EXPORT_H
#define _ZET_INTEL_GPU_METRIC_EXPORT_H

#include <level_zero/ze_api.h>

#if defined(__cplusplus)
#pragma once
extern "C" {
#endif

#include <stdint.h>

#define ZET_INTEL_GPU_METRIC_EXPORT_VERSION_MAJOR 0
#define ZET_INTEL_GPU_METRIC_EXPORT_VERSION_MINOR 1

#pragma pack(push, 1)

#define ZET_INTEL_GPU_METRIC_INVALID_OFFSET ((ptrdiff_t)0u)
#define zet_intel_metric_df_gpu_offset_to_ptr(type, offset, base) \
    type##_to_pointer(offset, base);

typedef ptrdiff_t uint8_offset_t;
typedef ptrdiff_t cstring_offset_t;
typedef ptrdiff_t zet_intel_metric_df_gpu_equation_element_0_1_offset_t;
typedef ptrdiff_t zet_intel_metric_df_gpu_information_params_0_1_offset_t;
typedef ptrdiff_t zet_intel_metric_df_gpu_metric_params_0_1_offset_t;
typedef ptrdiff_t zet_intel_metric_df_gpu_global_symbol_0_1_offset_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_apiversion_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_apiversion_0_1_t {
    uint32_t majorNumber;
    uint32_t minorNumber;
    uint32_t buildNumber;
} zet_intel_metric_df_gpu_apiversion_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_adapter_id_type_t
//////////////////////////////////////////////////////////////////////////////////
typedef enum _zet_intel_metric_df_gpu_adapter_id_type_t {
    ZET_INTEL_METRIC_DF_ADAPTER_ID_TYPE_UNDEFINED = 0,
    ZET_INTEL_METRIC_DF_ADAPTER_ID_TYPE_LUID,
    ZET_INTEL_METRIC_DF_ADAPTER_ID_TYPE_MAJOR_MINOR,
    ZET_INTEL_METRIC_DF_ADAPTER_ID_TYPE_FORCE_UINT32 = 0x7fffffff
} zet_intel_metric_df_gpu_adapter_id_type_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_adapter_id_luid_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_adapter_id_luid_0_1_t {
    uint32_t lowPart;
    int32_t highPart;
} zet_intel_metric_df_gpu_adapter_id_luid_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_adapter_id_major_minor_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_adapter_id_major_minor_0_1_t {
    int32_t major;
    int32_t minor;
} zet_intel_metric_df_gpu_adapter_id_major_minor_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_adapter_id_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_adapter_id_0_1_t {
    zet_intel_metric_df_gpu_adapter_id_type_t type;
    union {
        zet_intel_metric_df_gpu_adapter_id_luid_0_1_t luid;
        zet_intel_metric_df_gpu_adapter_id_major_minor_0_1_t majorMinor;
    };
} zet_intel_metric_df_gpu_adapter_id_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_adapter_params_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_adapter_params_0_1_t {
    zet_intel_metric_df_gpu_adapter_id_0_1_t systemId; // Operating system specific adapter id
    uint32_t busNumber;
    uint32_t deviceNumber;
    uint32_t functionNumber;
    uint32_t domainNumber;
} zet_intel_metric_df_gpu_adapter_params_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_metrics_device_params_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_metrics_device_params_0_1_t {
    // API version
    zet_intel_metric_df_gpu_apiversion_0_1_t version;
    uint32_t globalSymbolsCount;
} zet_intel_metric_df_gpu_metrics_device_params_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_value_type_t
//////////////////////////////////////////////////////////////////////////////////
typedef enum _zet_intel_metric_df_gpu_value_type_t {
    ZET_INTEL_METRIC_DF_VALUE_TYPE_UINT32 = 0,
    ZET_INTEL_METRIC_DF_VALUE_TYPE_UINT64,
    ZET_INTEL_METRIC_DF_VALUE_TYPE_FLOAT,
    ZET_INTEL_METRIC_DF_VALUE_TYPE_BOOL,
    ZET_INTEL_METRIC_DF_VALUE_TYPE_CSTRING,
    ZET_INTEL_METRIC_DF_VALUE_TYPE_BYTEARRAY,
    ZET_INTEL_METRIC_DF_VALUE_TYPE_FORCE_UINT32 = 0x7fffffff
} zet_intel_metric_df_gpu_value_type_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_byte_array_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_byte_array_0_1_t {
    uint8_offset_t data;
    uint32_t size;
} zet_intel_metric_df_gpu_byte_array_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_typed_value_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_typed_value_0_1_t {
    union {
        uint32_t valueUInt32;
        uint64_t valueUInt64;
        struct {
            uint32_t low;
            uint32_t high;
        } valueUInt64Fields;
        float valueFloat;
        bool valueBool;
        cstring_offset_t valueCString;
        zet_intel_metric_df_gpu_byte_array_0_1_t valueByteArray;
    };
    zet_intel_metric_df_gpu_value_type_t valueType;
} zet_intel_metric_df_gpu_typed_value_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_global_symbol_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_global_symbol_0_1_t {
    cstring_offset_t symbolName;
    zet_intel_metric_df_gpu_typed_value_0_1_t symbolTypedValue;
} zet_intel_metric_df_gpu_global_symbol_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_equation_element_type_t
//////////////////////////////////////////////////////////////////////////////////
typedef enum _zet_intel_metric_df_gpu_equation_element_type_t {
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_OPERATION = 0,             // See Tequation_operation enumeration
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_BITFIELD,               //
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT8,                  //
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT16,                 //
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT32,                 //
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_UINT64,                 //
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_FLOAT,                  //
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_RD_40BIT_CNTR,             // Assemble 40 bit counter that is in two locations, result in unsigned integer 64b
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_IMM_UINT64,                //
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_IMM_FLOAT,                 //
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_SELF_COUNTER_VALUE,        // Defined by $Self token, the UINT64 result of delta_function for IO or QueryReadEquation
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_GLOBAL_SYMBOL,             // Defined by $"SymbolName", available in MetricsDevice SymbolTable
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_LOCAL_COUNTER_SYMBOL,      // Defined by $"SymbolName", refers to counter delta value in the local set
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_OTHER_SET_COUNTER_SYMBOL,  // Defined by concatenated string of $"setSymbolName/SymbolName", refers to counter
                                                                 // Delta value in the other set
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_LOCAL_METRIC_SYMBOL,       // Defined by $$"SymbolName", refers to metric normalized value in the local set
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_OTHER_SET_METRIC_SYMBOL,   // Defined by concatenated string of $$"setSymbolName/SymbolName", refers to metric
                                                                 // Normalized value in the other set
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_INFORMATION_SYMBOL,        // Defined by i$"SymbolName", refers to information value type only
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_STD_NORM_GPU_DURATION,     // Action is $Self $GpuCoreClocks FDIV 100 FMUL
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_STD_NORM_EU_AGGR_DURATION, // Action is $Self $GpuCoreClocks $EuCoresTotalCount UMUL FDIV 100 FMUL
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_MASK,
    ZET_INTEL_METRIC_DF_EQUATION_ELEM_FORCE_UINT32 = 0x7fffffff
} zet_intel_metric_df_gpu_equation_element_type_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_equation_operation_t
//////////////////////////////////////////////////////////////////////////////////
typedef enum _zet_intel_metric_df_gpu_equation_operation_t {
    ZET_INTEL_METRIC_DF_EQUATION_OPER_RSHIFT = 0, // 64b unsigned integer right shift
    ZET_INTEL_METRIC_DF_EQUATION_OPER_LSHIFT,     // 64b unsigned integer left shift
    ZET_INTEL_METRIC_DF_EQUATION_OPER_AND,        // Bitwise AND of two unsigned integers, 64b each
    ZET_INTEL_METRIC_DF_EQUATION_OPER_OR,         // Bitwise OR of two unsigned integers, 64b each
    ZET_INTEL_METRIC_DF_EQUATION_OPER_XOR,        // Bitwise XOR of two unsigned integers, 64b each
    ZET_INTEL_METRIC_DF_EQUATION_OPER_XNOR,       // Bitwise XNOR of two unsigned integers, 64b each
    ZET_INTEL_METRIC_DF_EQUATION_OPER_AND_L,      // Logical AND (C-like "&&") of two unsigned integers, 64b each, result is true(1) if both values are true(greater than 0)
    ZET_INTEL_METRIC_DF_EQUATION_OPER_EQUALS,     // Equality (C-like "==") of two unsigned integers, 64b each, result is true(1) or false(0)
    ZET_INTEL_METRIC_DF_EQUATION_OPER_UADD,       // Unsigned integer add, arguments are casted to be 64b unsigned integers, result is unsigned integer 64b
    ZET_INTEL_METRIC_DF_EQUATION_OPER_USUB,       // Unsigned integer subtract, arguments are casted to be 64b unsigned integers, result is unsigned integer 64b
    ZET_INTEL_METRIC_DF_EQUATION_OPER_UMUL,       // Unsigned integer mul, arguments are casted to be 64b unsigned integers, result is unsigned integer 64b
    ZET_INTEL_METRIC_DF_EQUATION_OPER_UDIV,       // Unsigned integer div, arguments are casted to be 64b unsigned integers, result is unsigned integer 64b
    ZET_INTEL_METRIC_DF_EQUATION_OPER_FADD,       // Floating point add, arguments are casted to be 32b floating points, result is a 32b float
    ZET_INTEL_METRIC_DF_EQUATION_OPER_FSUB,       // Floating point subtract, arguments are casted to be 32b floating points, result is a 32b float
    ZET_INTEL_METRIC_DF_EQUATION_OPER_FMUL,       // Floating point multiply, arguments are casted to be 32b floating points, result is a 32b float
    ZET_INTEL_METRIC_DF_EQUATION_OPER_FDIV,       // Floating point divide, arguments are casted to be 32b floating points, result is a 32b float
    ZET_INTEL_METRIC_DF_EQUATION_OPER_UGT,        // 64b unsigned integers comparison of is greater than, result is bool true(1) or false(0)
    ZET_INTEL_METRIC_DF_EQUATION_OPER_ULT,        // 64b unsigned integers comparison of is less than, result is bool true(1) or false(0)
    ZET_INTEL_METRIC_DF_EQUATION_OPER_UGTE,       // 64b unsigned integers comparison of is greater than or equal, result is bool true(1) or false(0)
    ZET_INTEL_METRIC_DF_EQUATION_OPER_ULTE,       // 64b unsigned integers comparison of is less than or equal, result is bool true(1) or false(0)
    ZET_INTEL_METRIC_DF_EQUATION_OPER_FGT,        // 32b floating point numbers comparison of is greater than, result is bool true(1) or false(0)
    ZET_INTEL_METRIC_DF_EQUATION_OPER_FLT,        // 32b floating point numbers comparison of is less than, result is bool true(1) or false(0)
    ZET_INTEL_METRIC_DF_EQUATION_OPER_FGTE,       // 32b floating point numbers comparison of is greater than or equal, result is bool true(1) or false(0)
    ZET_INTEL_METRIC_DF_EQUATION_OPER_FLTE,       // 32b floating point numbers comparison of is less than or equal, result is bool true(1) or false(0)
    ZET_INTEL_METRIC_DF_EQUATION_OPER_UMIN,       // Unsigned integer MIN function, arguments are casted to be 64b unsigned integers, result is unsigned integer 64b
    ZET_INTEL_METRIC_DF_EQUATION_OPER_UMAX,       // Unsigned integer MAX function, arguments are casted to be 64b unsigned integers, result is unsigned integer 64b
    ZET_INTEL_METRIC_DF_EQUATION_OPER_FMIN,       // Floating point MIN function, arguments are casted to be 32b floating points, result is a 32b float
    ZET_INTEL_METRIC_DF_EQUATION_OPER_FMAX,       // Floating point MAX function, arguments are casted to be 32b floating points, result is a 32b float
    ZET_INTEL_METRIC_DF_EQUATION_OPER_FORCE_UINT32 = 0x7fffffff
} zet_intel_metric_df_gpu_equation_operation_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_read_params_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_read_params_0_1_t {
    uint32_t byteOffset;
    uint32_t bitOffset;
    uint32_t bitsCount;
    uint32_t byteOffsetExt;
} zet_intel_metric_df_gpu_read_params_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_equation_element_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_equation_element_0_1_t {
    cstring_offset_t symbolName;
    union {
        uint64_t immediateUInt64;
        float immediateFloat;
        zet_intel_metric_df_gpu_byte_array_0_1_t mask;
        zet_intel_metric_df_gpu_equation_operation_t operation;
        zet_intel_metric_df_gpu_read_params_0_1_t readParams;
    };
    zet_intel_metric_df_gpu_equation_element_type_t type;
} zet_intel_metric_df_gpu_equation_element_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_equation_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_equation_0_1_t {
    uint32_t elementCount;
    zet_intel_metric_df_gpu_equation_element_0_1_offset_t elements;
} zet_intel_metric_df_gpu_equation_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_metric_result_type_t
//////////////////////////////////////////////////////////////////////////////////
typedef enum _zet_intel_metric_df_gpu_metric_result_type_t {
    ZET_INTEL_METRIC_DF_RESULT_UINT32 = 0,
    ZET_INTEL_METRIC_DF_RESULT_UINT64,
    ZET_INTEL_METRIC_DF_RESULT_BOOL,
    ZET_INTEL_METRIC_DF_RESULT_FLOAT,
    ZET_INTEL_METRIC_DF_RESULT_FORCE_UINT32 = 0x7fffffff
} zet_intel_metric_df_gpu_metric_result_type_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_delta_function_type_t
//////////////////////////////////////////////////////////////////////////////////
typedef enum _zet_intel_metric_df_gpu_delta_function_type_t {
    ZET_INTEL_METRIC_DF_DELTA_FUNCTION_NULL = 0,
    ZET_INTEL_METRIC_DF_DELTA_N_BITS,
    ZET_INTEL_METRIC_DF_DELTA_BOOL_OR,      // Logic OR - good for exceptions
    ZET_INTEL_METRIC_DF_DELTA_BOOL_XOR,     // Logic XOR - good to check if bits were changed
    ZET_INTEL_METRIC_DF_DELTA_GET_PREVIOUS, // Preserve previous value
    ZET_INTEL_METRIC_DF_DELTA_GET_LAST,     // Preserve last value
    ZET_INTEL_METRIC_DF_DELTA_NS_TIME,      // Delta for nanosecond timestamps (GPU timestamp wraps at 32 bits but was value multiplied by 80)
    ZET_INTEL_METRIC_DF_DELTA_FORCE_UINT32 = 0x7fffffff
} zet_intel_metric_df_gpu_delta_function_type_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_metric_type_t
//////////////////////////////////////////////////////////////////////////////////
typedef enum _zet_intel_metric_df_gpu_metric_type_t {
    ZET_INTEL_METRIC_DF_METRIC_TYPE_DURATION = 0,
    ZET_INTEL_METRIC_DF_METRIC_TYPE_EVENT,
    ZET_INTEL_METRIC_DF_METRIC_TYPE_EVENT_WITH_RANGE,
    ZET_INTEL_METRIC_DF_METRIC_TYPE_THROUGHPUT,
    ZET_INTEL_METRIC_DF_METRIC_TYPE_TIMESTAMP,
    ZET_INTEL_METRIC_DF_METRIC_TYPE_FLAG,
    ZET_INTEL_METRIC_DF_METRIC_TYPE_RATIO,
    ZET_INTEL_METRIC_DF_METRIC_TYPE_RAW,
    ZET_INTEL_METRIC_DF_METRIC_TYPE_FORCE_UINT32 = 0x7fffffff
} zet_intel_metric_df_gpu_metric_type_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_delta_function_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_delta_function_0_1_t {
    zet_intel_metric_df_gpu_delta_function_type_t functionType;
    union {
        uint32_t bitsCount; // Used for DELTA_N_BITS to specify bits count
    };
} zet_intel_metric_df_gpu_delta_function_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_metric_params_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_metric_params_0_1_t {
    cstring_offset_t symbolName;                                // Symbol name, used in equations
    cstring_offset_t shortName;                                 // Consistent metric name, not changed platform to platform
    cstring_offset_t groupName;                                 // VertexShader for example
    cstring_offset_t longName;                                  // Hint about the metric shown to users
    uint32_t idInSet;                                           // Position in set (may change after SetApiFiltering)
    uint32_t apiMask;                                           //
    cstring_offset_t metricResultUnits;                         //
    zet_intel_metric_df_gpu_metric_type_t metricType;           //
    zet_intel_metric_df_gpu_equation_0_1_t ioReadEquation;      // Read equation specification for IO stream (accessing raw values potentially spread in report in several locations)
    zet_intel_metric_df_gpu_equation_0_1_t queryReadEquation;   // Read equation specification for query (accessing calculated delta values)
    zet_intel_metric_df_gpu_delta_function_0_1_t deltaFunction; //
    zet_intel_metric_df_gpu_equation_0_1_t normEquation;        // Normalization equation to get normalized value to bytes transfered or to a percentage of utilization
    zet_intel_metric_df_gpu_equation_0_1_t maxValueEquation;    // To calculate metrics max value as a function of other metrics and device parameters (e.g. 100 for percentage)
    uint32_t usageFlagsMask;
    zet_intel_metric_df_gpu_metric_result_type_t resultType;
} zet_intel_metric_df_gpu_metric_params_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_information_type_t
//////////////////////////////////////////////////////////////////////////////////
typedef enum _zet_intel_metric_df_gpu_information_type_t {
    ZET_INTEL_METRIC_DF_INFORMATION_TYPE_REPORT_REASON = 0,
    ZET_INTEL_METRIC_DF_INFORMATION_TYPE_VALUE,
    ZET_INTEL_METRIC_DF_INFORMATION_TYPE_FLAG,
    ZET_INTEL_METRIC_DF_INFORMATION_TYPE_TIMESTAMP,
    ZET_INTEL_METRIC_DF_INFORMATION_TYPE_CONTEXT_ID_TAG,
    ZET_INTEL_METRIC_DF_INFORMATION_TYPE_SAMPLE_PHASE,
    ZET_INTEL_METRIC_DF_INFORMATION_TYPE_GPU_NODE,
    ZET_INTEL_METRIC_DF_INFORMATION_TYPE_FORCE_UINT32 = 0x7fffffff
} zet_intel_metric_df_gpu_information_type_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_information_params_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_information_params_0_1_t {
    cstring_offset_t symbolName; // Symbol name, used in equations
    cstring_offset_t shortName;  // Consistent name, not changed platform to platform
    cstring_offset_t groupName;  // Some more global context of the information
    cstring_offset_t longName;   // Hint about the information shown to users
    uint32_t idInSet;
    uint32_t apiMask;                                              //
    zet_intel_metric_df_gpu_information_type_t infoType;           //
    cstring_offset_t infoUnits;                                    //
    zet_intel_metric_df_gpu_equation_0_1_t ioReadEquation;         // Read equation specification for IO stream (accessing raw values potentially spread in report in several locations)
    zet_intel_metric_df_gpu_equation_0_1_t queryReadEquation;      // Read equation specification for query (accessing calculated delta values)
    zet_intel_metric_df_gpu_delta_function_0_1_t overflowFunction; //
} zet_intel_metric_df_gpu_information_params_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_metric_set_params_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_metric_set_params_0_1_t {
    cstring_offset_t symbolName; // For example "Dx11Tessellation"
    cstring_offset_t shortName;  // For example "DX11 Tessellation Metrics Set"
    uint32_t apiMask;
    uint32_t metricsCount;
    uint32_t informationCount;
    cstring_offset_t availabilityEquation;
} zet_intel_metric_df_gpu_metric_set_params_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_concurrent_group_params_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_concurrent_group_params_0_1_t {
    cstring_offset_t symbolName;  // For example "OA" or "OAM0" or "PipeStats"
    cstring_offset_t description; // For example "OA Unit Metrics"
    uint32_t ioMeasurementInformationCount;
    uint32_t ioGpuContextInformationCount;
} zet_intel_metric_df_gpu_concurrent_group_params_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_concurrent_group_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_concurrent_group_0_1_t {
    zet_intel_metric_df_gpu_concurrent_group_params_0_1_t params;
    zet_intel_metric_df_gpu_information_params_0_1_offset_t ioMeasurementInformation;
    zet_intel_metric_df_gpu_information_params_0_1_offset_t ioGpuContextInformation;
} zet_intel_metric_df_gpu_concurrent_group_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_metric_set_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_metric_set_0_1_t {
    zet_intel_metric_df_gpu_metric_set_params_0_1_t params;
    zet_intel_metric_df_gpu_information_params_0_1_offset_t informationParams;
    zet_intel_metric_df_gpu_metric_params_0_1_offset_t metricParams;
} zet_intel_metric_df_gpu_metric_set_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_metric_oa_calc_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_metric_oa_calc_0_1_t {
    zet_intel_metric_df_gpu_metrics_device_params_0_1_t deviceParams;
    zet_intel_metric_df_gpu_global_symbol_0_1_offset_t globalSymbols;
    zet_intel_metric_df_gpu_adapter_params_0_1_t adapterParams;
    zet_intel_metric_df_gpu_concurrent_group_0_1_t concurrentGroup;
    zet_intel_metric_df_gpu_metric_set_0_1_t metricSet;
} zet_intel_metric_df_gpu_metric_oa_calc_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_export_data_format_0_1_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_export_data_format_0_1_t {
    union {
        zet_intel_metric_df_gpu_metric_oa_calc_0_1_t oaData;
    };
} zet_intel_metric_df_gpu_export_data_format_0_1_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_metric_source_type_t
//////////////////////////////////////////////////////////////////////////////////
typedef enum _zet_intel_metric_df_gpu_metric_source_type_t {
    ZET_INTEL_METRIC_DF_SOURCE_TYPE_OA = 0,
    ZET_INTEL_METRIC_DF_SOURCE_TYPE_IPSAMPLING,
    ZET_INTEL_METRIC_DF_SOURCE_TYPE_FORCE_UINT32 = 0x7fffffff
} zet_intel_metric_df_gpu_metric_source_type_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_metric_version_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_metric_version_t {
    uint32_t major;
    uint32_t minor;
    uint32_t reserved[2];
} zet_intel_metric_df_gpu_metric_version_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_header_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_header_t {
    zet_intel_metric_df_gpu_metric_source_type_t type;
    zet_intel_metric_df_gpu_metric_version_t version;
    ze_device_type_t deviceType;
    uint64_t rawDataOffset;
    uint64_t rawDataSize;
    uint8_t reserved[256];
} zet_intel_metric_df_gpu_header_t;

//////////////////////////////////////////////////////////////////////////////////
// zet_intel_metric_df_gpu_export_data_format_t
//////////////////////////////////////////////////////////////////////////////////
typedef struct _zet_intel_metric_df_gpu_export_data_format_t {
    zet_intel_metric_df_gpu_header_t header;
    zet_intel_metric_df_gpu_export_data_format_0_1_t format01;
} zet_intel_metric_df_gpu_export_data_format_t;

#define offset_to_pointer(offset, base) (ZET_INTEL_GPU_METRIC_INVALID_OFFSET == offset ? nullptr : (uint8_t *)base + offset)
#define uint8_offset_t_to_pointer(offset, base) (uint8_t *)offset_to_pointer(offset, base)
#define cstring_offset_t_to_pointer(offset, base) (const char *)offset_to_pointer(offset, base)
#define zet_intel_metric_df_gpu_equation_element_0_1_offset_t_to_pointer(offset, base) (zet_intel_metric_df_gpu_equation_element_0_1_t *)offset_to_pointer(offset, base)
#define zet_intel_metric_df_gpu_information_params_0_1_offset_t_to_pointer(offset, base) (zet_intel_metric_df_gpu_information_params_0_1_t *)offset_to_pointer(offset, base)
#define zet_intel_metric_df_gpu_metric_params_0_1_offset_t_to_pointer(offset, base) (zet_intel_metric_df_gpu_metric_params_0_1_t *)offset_to_pointer(offset, base)
#define zet_intel_metric_df_gpu_global_symbol_0_1_offset_t_to_pointer(offset, base) (zet_intel_metric_df_gpu_global_symbol_0_1_t *)offset_to_pointer(offset, base)

#pragma pack(pop)

#if defined(__cplusplus)
} // extern "C"
#endif

#endif // _ZET_INTEL_GPU_METRIC_EXPORT_H