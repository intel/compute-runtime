<!---

Copyright (C) 2023 Intel Corporation

SPDX-License-Identifier: MIT

-->

# GPU Metrics collection in Level Zero

* [Introduction](#Introduction)
* [Dependencies](#Dependencies)
* [Environment Setup](#Environment-Setup)
* [GPU performance metrics](#GPU-Performance-Metrics)
* [EU Stall Sampling](#EU-Stall-Sampling)
* [Limitations](#Limitations)


# Introduction

Implementation independent details of Level-Zero metrics are described in the Level-Zero specification [Metrics Section](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/tools/PROG.html#metrics). This implementation supports Time-based and Event-based sampling. Two domains are supported, one for collecting GPU performance metrics and one for collecting EU stall sampling data (type ZET_METRIC_TYPE_IP).

# Dependencies

Metrics collection depends on:

* Intel(R) Metrics Discovery (MDAPI) - https://github.com/intel/metrics-discovery
* Intel(R) Metrics Library for MDAPI - https://github.com/intel/metrics-library

# Environment Setup

As described in Level-Zero specification [Tools Section](https://oneapi-src.github.io/level-zero-spec/level-zero/latest/tools/PROG.html#environment-variables) environment variable `ZET_ENABLE_METRICS` must be set to 1.

## Linux
Additionally in Linux environment, is is required to disable the kernel module driver i915 performance stream paranoid mode. This can be done with command

```
 sudo sysctl dev.i915.perf_stream_paranoid=0
```

# GPU performance metrics
Level-Zero uses Intel(R) Metrics Discovery (MDAPI) to read GPU metrics for performance analysis. A detailed list of metrics available per platform can be found in the [Metrics Discovery Documentation](https://github.com/intel/metrics-discovery/tree/master/docs).  

# EU Stall Sampling 
HW-assisted EU stall sampling allows statistically correlating Xe-Vector Engine (XVE) stall events to the executed instructions and breaks down the stall events by different stall reasons. Using the Instruction Pointer it is possible to point to the GPU kernel source code line causing the most stalls. 

## Requirements and Limitations
1.	The intermediate language build (SPIR-V) used to build the GPU Kernel must contain debug data. 
2.	Level-Zero modules must be created in Ze Binary format. This is supported in compute-runtime version [22.49.25018.24](https://github.com/intel/compute-runtime/releases) or newer.  
3.	IGALibrary is needed for disassembling GPU Kernel binary. Library is included in [Intel(R) Graphics Compiler](https://github.com/intel/intel-graphics-compiler)

## Collection Instructions
### Register Tracing Epilogs
Using Level-Zero Loader Tracing Layer register epilog callbacks for L0 APIs. See details at https://github.com/oneapi-src/level-zero/blob/master/source/layers/tracing/README.md 
1.	Epilog callback for zeModuleCreate(): 
- In the epilog read the return status of zeModuleCreate() and if executed successfully call zeModuleGetNativeBinary() and store the returned binary. 
- Kernels inside the native module binary are separated in sections and can be extracted with any ELF reading utility. For example with  ```readelf -x .text.<kernelName>```
2.	Epilog callback for zeKernelCreate(): 
- In the epilog use zeDriverGetExtensionFunctionAddress() get the function pointer to the L0 GPU implementation of zexKernelGetBaseAddress().  See details at https://github.com/intel/compute-runtime/blob/master/level_zero/api/driver_experimental/public/zex_module.h and https://github.com/intel/compute-runtime/blob/master/level_zero/doc/experimental_extensions/DRIVER_EXPERIMENTAL_EXTENSIONS.md
- Read the return status of zeKernelCreate() and if executed successfully, call zexKernelGetBaseAddress() and save the load address of the GPU kernel. This is the base virtual address where the kernel is loaded in the GPU memory. This is an example of how the epilog for zeKernelCreate() should look like:

    ```cpp
    void onExitFromzeKernelCreate (ze_kernel_create_params_t* params, ze_result_t result, void* pTracerUserData, void** ppTracerInstanceUserData){

        //zeKernelCreate() was successful
        if (result == ZE_RESULT_SUCCESS) {
            typedef ze_result_t (*pFnzexKernelGetBaseAddress)(ze_kernel_handle_t hKernel, uint64_t *baseAddress);
            pFnzexKernelGetBaseAddress zexKernelGetBaseAddressPointer = nullptr;
            // Handle to the driver must be passed in by the user using tracing pointers
            ze_driver_handle_t hDriver = reinterpret_cast<ze_driver_handle_t> (pTracerUserData);
            ze_result_t internalResult;
            internalResult = zeDriverGetExtensionFunctionAddress(
                hDriver, "zexKernelGetBaseAddress",
                reinterpret_cast<void **>(&zexKernelGetBaseAddressPointer));
            if (internalResult == ZE_RESULT_SUCCESS) {
                unit64_t baseAddress;
                // Recently created kernel taken from the API parameters
                internalResult = zexKernelGetBaseAddressPointer(**params->pphkernel, &baseAddress);
                if (internalResult == ZE_RESULT_SUCCESS) {
                    // Return base address to the caller with the per instance tracing data
                    uint64_t *returnBaseAddress = <uint64_t*> (ppTracerInstanceUserData);
                    *returnBaseAddress = baseAddress;
                }
            }
        }
    }
    ```


### Disassemble GPU Kernel:
1.	Using KernelView class (included in IGALibrary) disassemble the GPU kernel. Details of the class are in [kv.hpp](https://github.com/intel/intel-graphics-compiler)
2.	Store the disassembled GPU kernel code. 

### Metrics collection:
1.	Enumerate available metric groups and query properties for metrics in each group. Match the metric group named "EU stall sampling", which contains the metric type ZET_METRIC_TYPE_IP. See high level generic steps at https://oneapi-src.github.io/level-zero-spec/level-zero/latest/tools/PROG.html#enumeration
2.	Open a streamer to collect metrics in "EU stall sampling" metric group.   See high level generic steps at  https://oneapi-src.github.io/level-zero-spec/level-zero/latest/tools/PROG.html#metric-streamer 
3.	Run workload using Level-Zero Core APIs. 
4.	Read metrics data (zetMetricStreamerReadData()) and calculate metrics values  (zetMetricGroupCalculateMultipleMetricValuesExp()).

### Mapping instructions:
1.	Address shift: Given that the address returned by zexKernelGetBaseAddress() (base address) has 32 bit of resolution and the instruction pointer returned in metric type ZET_METRIC_TYPE_IP (instruction address) has 29 bits of resolution, it is necessary to shift left 3 bits the first address. So, the comparison is done on 29 bits. 
2.	The difference between the instruction address (returned by ZET_METRIC_TYPE_IP) and the base address can be used as the offset into the dissembled binary to find the instruction producing the stall. 
3.	Having the GPU kernel instruction identified it is possible to map it to the source code using readelf utility to dump the debug data: ``` readelf --debug-dump=decodedline <path/to/binary> ```

### Stall reasons
The metric type ZET_METRIC_TYPE_IP has a structure with all the possible stall reasons and a count of each reason. These can be queried at runtime using zetMetricGetProperties(). Counter reasons are:

- ACTIVE - Actively executing in at least one pipeline
- INST_FETCH - Stalled due to an instruction fetch operation
- SYNC - Stalled due to sync operation 
- SCOREBOARD ID - Stalled due to memory dependency or internal XVE pipeline dependency
- DIST or ACC - Stalled due to internal pipeline dependency
- SEND - Stalled due to memory dependency or internal pipeline dependency for send
- PIPESTALL - Stalled due to XVE pipeline
- CONTROL - Stalled due to branch
- OTHER - Stalled due to any other reason


# Limitations

## EU Stall Sampling 
* Only supported on Linux
* Does not support streamer markers
* The inherent nature of the samples only make sense for Time-based sampling. Therefore, Event-based sampling is not supported. 

## GPU performance metrics
* To obtain the most recent metric values using Time-based sampling, it is necessary to read all metrics reports from the hardware buffer and calculate them all. This may be costly operation if the hardware buffer is not read at frequent intervals. Therefore, it is recommended to call zetMetricStreamerReadData() at a time interval that does not require processing big number of reports. This can be calculated based on the sampling rate decided when opening the metrics streamer (zet_metric_streamer_desc_t.samplingPeriod).

## notifyEveryNReports
* Linux support for notifyEveryNReports on performance metrics will always return true when one metric report is available.
