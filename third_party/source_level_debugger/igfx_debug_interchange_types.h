/* BSD LICENSE
*
* Copyright(c) 2014-2016 Intel Corporation.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
*   * Redistributions of source code must retain the above copyright
*     notice, this list of conditions and the following disclaimer.
*   * Redistributions in binary form must reproduce the above copyright
*     notice, this list of conditions and the following disclaimer in
*     the documentation and/or other materials provided with the
*     distribution.
*   * Neither the name of Intel Corporation nor the names of its
*     contributors may be used to endorse or promote products derived
*     from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#if !defined(__DEBUG_NOTIFY_TYPES_H_INCLUDED__)
#define __DEBUG_NOTIFY_TYPES_H_INCLUDED__

typedef enum
{
    IGFXDBG_SUCCESS         =  0,
    IGFXDBG_FAILURE         = -1,
    IGFXDBG_INVALID_VALUE   = -2,
    IGFXDBG_INVALID_VERSION = -3
} IgfxdbgRetVal;

#ifdef __cplusplus
namespace dbg_options
{
    static const char DBG_OPTION_TRUE[] = "1";
    static const char DBG_OPTION_FALSE[] = "0";

    static const char DBG_OPTION_COMPILE_NO_OPT[] = "Compile_NO_OPT";
    static const char DBG_OPTION_MONITOR_EOT[] = "eot";
    static const char DBG_OPTION_STRICT_SOLIB_EVENT[] = "strict-solib-event";
}
#endif

typedef void* GfxDbgHandle;
typedef GfxDbgHandle CmDeviceHandle;
typedef GfxDbgHandle CmUmdDeviceHandle;
typedef GfxDbgHandle CmUmdProgramHandle;
typedef GfxDbgHandle GenRtProgramHandle;
typedef GfxDbgHandle GfxDeviceHandle;

// -------------------------------------------------------------------------------------------
//
// -------------------------------------------------------------------------------------------
struct GfxDbgVisaElfFileData
{
    unsigned int version;
    GenRtProgramHandle gph;
    CmDeviceHandle dh;
    const char* elfFileName;
    const void* inMemoryBuffer;
    unsigned int inMemorySize;
};

// -------------------------------------------------------------------------------------------
//
// -------------------------------------------------------------------------------------------
struct GfxDbgVisaCodeLoadData
{
    unsigned int version;
    GenRtProgramHandle gph;
    CmDeviceHandle dh;
    const char* kernelNames;
    unsigned kernelNamesSize;
    const char* elfFileName;
    const void* inMemoryBuffer;
    unsigned inMemorySize;
};

// -------------------------------------------------------------------------------------------
//
// -------------------------------------------------------------------------------------------
struct GfxDbgNewDeviceData
{
    unsigned int version;
    CmDeviceHandle dh;
    CmUmdDeviceHandle udh;
};

// -------------------------------------------------------------------------------------------
//
// -------------------------------------------------------------------------------------------
struct GfxDbgDeviceDestructionData
{
    unsigned int version;
    CmDeviceHandle dh;
};

// -------------------------------------------------------------------------------------------
//
// -------------------------------------------------------------------------------------------
struct GfxDbgKernelBinaryData
{
    unsigned int version;
    CmUmdDeviceHandle udh;
    CmUmdProgramHandle uph;
    const char* kernelName;
    void* genBinary;
    unsigned int genBinarySize;
    void* genDebugInfo;
    unsigned int genDebugInfoSize;
    const char* debugInfoFileName;
};

// -------------------------------------------------------------------------------------------
//
// -------------------------------------------------------------------------------------------
struct GfxDbgKernelDebugData
{
    unsigned int        version;
    GfxDeviceHandle     hDevice;
    GenRtProgramHandle  hProgram;
    const char*         kernelName;
    void*               kernelBinBuffer;
    unsigned int        KernelBinSize;
    const void*         dbgVisaBuffer;
    unsigned int        dbgVisaSize;
    const void*         dbgGenIsaBuffer;
    unsigned int        dbgGenIsaSize;
};

// -------------------------------------------------------------------------------------------
//
// -------------------------------------------------------------------------------------------
struct GfxDbgSourceCode
{
    unsigned int    version;
    GfxDeviceHandle hDevice;
    const char*     sourceCode;
    unsigned int    sourceCodeSize;
    unsigned int    sourceNameMaxLen;
    char*           sourceName;
};

// -------------------------------------------------------------------------------------------
//
// -------------------------------------------------------------------------------------------
enum GfxDbgOptionNames
{
  DBG_OPTION_IS_OPTIMIZATION_DISABLED = 1
};

// -------------------------------------------------------------------------------------------
//
// -------------------------------------------------------------------------------------------
struct GfxDbgOption
{
    unsigned int       version;
    GfxDbgOptionNames  optionName;
    unsigned int       valueLen;
    char*              value;
};

// -------------------------------------------------------------------------------------------
//
// -------------------------------------------------------------------------------------------
struct GfxDbgTargetCaps
{
    unsigned int  version;
    bool          supportsLocalMemory;
};

#endif
