/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "driver_diagnostics.h"

namespace NEO {

DriverDiagnostics::DriverDiagnostics(cl_diagnostics_verbose_level level) {
    this->verboseLevel = level;
}

bool DriverDiagnostics::validFlags(cl_diagnostics_verbose_level flags) const {
    return !!(verboseLevel & flags);
}

const char *DriverDiagnostics::hintFormat[] = {
    "Performance hint: clCreateBuffer with pointer %p and size %u doesn't meet alignment restrictions. Size should be aligned to %u bytes and pointer should be aligned to %u. Buffer is not sharing the same physical memory with CPU.", //CL_BUFFER_DOESNT_MEET_ALIGNMENT_RESTRICTIONS
    "Performance hint: clCreateBuffer with pointer %p and size %u meets alignment restrictions and buffer will share the same physical memory with CPU.",                                                                                 //CL_BUFFER_MEETS_ALIGNMENT_RESTRICTIONS
    "Performance hint: clCreateBuffer needs to allocate memory for buffer. For subsequent operations the buffer will share the same physical memory with CPU.",                                                                           //CL_BUFFER_NEEDS_ALLOCATE_MEMORY
    "Performance hint: clCreateImage with pointer %p meets alignment restrictions and image will share the same physical memory with CPU.",                                                                                               //CL_IMAGE_MEETS_ALIGNMENT_RESTRICTIONS
    "Performance hint: Driver calls internal clFlush on the command queue each time 1 command is enqueued.",                                                                                                                              //DRIVER_CALLS_INTERNAL_CL_FLUSH
    "Performance hint: Profiling adds overhead on all enqueue commands with events.",                                                                                                                                                     //PROFILING_ENABLED
    "Performance hint: Profiled kernels will be executed with disabled preemption.",                                                                                                                                                      //PROFILING_ENABLED_WITH_DISABLED_PREEMPTION
    "Performance hint: Subbuffer created from buffer %p shares the same memory with buffer.",                                                                                                                                             //SUBBUFFER_SHARES_MEMORY
    "Performance hint: clSVMAlloc with pointer %p and size %u meets alignment restrictions.",                                                                                                                                             //CL_SVM_ALLOC_MEETS_ALIGNMENT_RESTRICTIONS
    "Performance hint: clEnqueueReadBuffer call on a buffer %p with pointer %p will require driver to copy the data.Consider using clEnqueueMapBuffer with buffer that shares the same physical memory with CPU.",                        //CL_ENQUEUE_READ_BUFFER_REQUIRES_COPY_DATA
    "Performance hint: clEnqueueReadBuffer call on a buffer %p with pointer %p will not require any data copy as the buffer shares the same physical memory with CPU.",                                                                   //CL_ENQUEUE_READ_BUFFER_DOESNT_REQUIRE_COPY_DATA
    "Performance hint: Pointer %p and size %u passed to clEnqueueReadBuffer doesn't meet alignment restrictions. Size should be aligned to %u bytes and pointer should be aligned to %u. Driver needs to disable L3 caching.",            //CL_ENQUEUE_READ_BUFFER_DOESNT_MEET_ALIGNMENT_RESTRICTIONS
    "Performance hint: clEnqueueReadBufferRect call on a buffer %p with pointer %p will require driver to copy the data.Consider using clEnqueueMapBuffer with buffer that shares the same physical memory with CPU.",                    //CL_ENQUEUE_READ_BUFFER_RECT_REQUIRES_COPY_DATA
    "Performance hint: clEnqueueReadBufferRect call on a buffer %p with pointer %p will not require any data copy as the buffer shares the same physical memory with CPU.",                                                               //CL_ENQUEUE_READ_BUFFER_RECT_DOESNT_REQUIRES_COPY_DATA
    "Performance hint: Pointer %p and size %u passed to clEnqueueReadBufferRect doesn't meet alignment restrictions. Size should be aligned to %u bytes and pointer should be aligned to %u. Driver needs to disable L3 caching.",        //CL_ENQUEUE_READ_BUFFER_RECT_DOESNT_MEET_ALIGNMENT_RESTRICTIONS
    "Performance hint: clEnqueueWriteBuffer call on a buffer %p require driver to copy the data. Consider using clEnqueueMapBuffer with buffer that shares the same physical memory with CPU.",                                           //CL_ENQUEUE_WRITE_BUFFER_REQUIRES_COPY_DATA
    "Performance hint: clEnqueueWriteBuffer call on a buffer %p with pointer %p will not require any data copy as the buffer shares the same physical memory with CPU.",                                                                  //CL_ENQUEUE_WRITE_BUFFER_DOESNT_REQUIRE_COPY_DATA
    "Performance hint: clEnqueueWriteBufferRect call on a buffer %p require driver to copy the data. Consider using clEnqueueMapBuffer with buffer that shares the same physical memory with CPU.",                                       //CL_ENQUEUE_WRITE_BUFFER_RECT_REQUIRES_COPY_DATA
    "Performance hint: clEnqueueWriteBufferRect call on a buffer %p will not require any data copy as the buffer shares the same physical memory with CPU.",                                                                              //CL_ENQUEUE_WRITE_BUFFER_RECT_DOESNT_REQUIRE_COPY_DATA
    "Performance hint: Pointer %p and size %u passed to clEnqueueReadImage doesn't meet alignment restrictions. Size should be aligned to %u bytes and pointer should be aligned to %u. Driver needs to disable L3 caching.",             //CL_ENQUEUE_READ_IMAGE_DOESNT_MEET_ALIGNMENT_RESTRICTIONS
    "Performance hint: clEnqueueReadImage call on an image %p will not require any data copy as the image shares the same physical memory with CPU.",                                                                                     //CL_ENQUEUE_READ_IMAGE_DOESNT_REQUIRES_COPY_DATA
    "Performance hint: clEnqueueWriteImage call on an image %p require driver to copy the data.",                                                                                                                                         //CL_ENQUEUE_WRITE_IMAGE_REQUIRES_COPY_DATA
    "Performance hint: clEnqueueWriteImage call on an image %p will not require any data copy as the image shares the same physical memory with CPU.",                                                                                    //CL_ENQUEUE_WRITE_IMAGE_DOESNT_REQUIRES_COPY_DATA
    "Performance hint: clEnqueueMapBuffer call on a buffer %p will require driver to make a copy as buffer is not sharing the same physical memory with CPU.",                                                                            //CL_ENQUEUE_MAP_BUFFER_REQUIRES_COPY_DATA
    "Performance hint: clEnqueueMapBuffer call on a buffer %p will not require any data copy as buffer shares the same physical memory with CPU.",                                                                                        //CL_ENQUEUE_MAP_BUFFER_DOESNT_REQUIRE_COPY_DATA
    "Performance hint: clEnqueueMapImage call on an image %p will require driver to make a copy, as image is not sharing the same physical memory with CPU.",                                                                             //CL_ENQUEUE_MAP_IMAGE_REQUIRES_COPY_DATA
    "Performance hint: clEnqueueMapImage call on an image %p will not require any data copy as image shares the same physical memory with CPU.",                                                                                          //CL_ENQUEUE_MAP_IMAGE_DOESNT_REQUIRE_COPY_DATA
    "Performance hint: clEnqueueUnmapMemObject call with pointer %p will not require any data copy.",                                                                                                                                     //CL_ENQUEUE_UNMAP_MEM_OBJ_DOESNT_REQUIRE_COPY_DATA
    "Performance hint: clEnqueueUnmapMemObject call with pointer %p will require driver to copy the data to memory object %p.",                                                                                                           //CL_ENQUEUE_UNMAP_MEM_OBJ_REQUIRES_COPY_DATA
    "Performance hint: clEnqueueSVMMap call with pointer %p will not require any data copy.",                                                                                                                                             //CL_ENQUEUE_SVM_MAP_DOESNT_REQUIRE_COPY_DATA
    "Performance hint: Printf detected in kernel %s, it may cause overhead.",                                                                                                                                                             //PRINTF_DETECTED_IN_KERNEL
    "Performance hint: Null local workgroup size detected ( kernel name: %s ); following sizes will be used for execution : { %u, %u, %u }.",                                                                                             //NULL_LOCAL_WORKGROUP_SIZE
    "Performance hint: Local workgroup sizes { %u, %u, %u } selected for this workload ( kernel name: %s ) may not be optimal, consider using following local workgroup size: { %u, %u, %u }.",                                           //BAD_LOCAL_WORKGROUP_SIZE
    "Performance hint: Kernel %s register pressure is too high, spill fills will be generated, additional surface needs to be allocated of size %u, consider simplifying your kernel.",                                                   //REGISTER_PRESSURE_TOO_HIGH
    "Performance hint: Kernel %s private memory usage is too high and exhausts register space, additional surface needs to be allocated of size %u, consider reducing amount of private memory used, avoid using private memory arrays.", //PRIVATE_MEMORY_USAGE_TOO_HIGH
    "Performance hint: Kernel %s submission requires coherency with CPU; this will impact performance.",                                                                                                                                  //KERNEL_REQUIRES_COHERENCY
    "Performance hint: Kernel %s requires aux translation on argument [%u] = \"%s\""                                                                                                                                                      //KERNEL_ARGUMENT_AUX_TRANSLATION
};
} // namespace NEO
