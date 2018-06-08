/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * File Name: scheduler.cl
 *
 */

#ifndef SCHEDULER_EMULATION
#include "device_enqueue.h"
#endif

// float passed as int
extern float __intel__getProfilingTimerResolution();

#ifndef EMULATION_ENTER_FUNCTION
#define EMULATION_ENTER_FUNCTION( )
#endif

#ifndef NULL
#define NULL                                    0
#endif

#define SIMD8                                   0
#define SIMD16                                  1
#define SIMD32                                  2

#define SHORT_SIZE_IN_BYTES                     2
#define DWORD_SIZE_IN_BYTES                     4
#define QWORD_SIZE_IN_BYTES                     8

#define MAX_GLOBAL_ARGS                         255

#define MASK_LOW16_BITS                         0xFFFF
//Currently setting to 8
#define MAX_WALKERS_IN_PARALELL                 PARALLEL_SCHEDULER_HW_GROUPS
//Need 4 uints per walker ( command packet offset, slb offest, dsh offset, idt offset,  + 1 to store total
#define PARALLEL_SCHEDULER_OFFSETS_NUMBER       4
#define PARALLEL_SCHEDULER_LOCAL_MEM_SIZE       ( MAX_WALKERS_IN_PARALELL * PARALLEL_SCHEDULER_OFFSETS_NUMBER + 1 )
//Last index
#define TOTAL_ENQUEUES_FOUND                    ( PARALLEL_SCHEDULER_LOCAL_MEM_SIZE - 1 )

//CURBE STUFF, only entries that really needs to be patched
#define SCHEDULER_DATA_PARAMETER_KERNEL_ARGUMENT                                    1
#define SCHEDULER_DATA_PARAMETER_LOCAL_WORK_SIZE                                    2
#define SCHEDULER_DATA_PARAMETER_GLOBAL_WORK_SIZE                                   3
#define SCHEDULER_DATA_PARAMETER_NUM_WORK_GROUPS                                    4
#define SCHEDULER_DATA_PARAMETER_WORK_DIMENSIONS                                    5
#define SCHEDULER_DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES          8
#define SCHEDULER_DATA_PARAMETER_GLOBAL_WORK_OFFSET                                 16
#define SCHEDULER_DATA_PARAMETER_NUM_HARDWARE_THREADS                               17
#define SCHEDULER_DATA_PARAMETER_PARENT_EVENT                                       22
#define SCHEDULER_DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE                           28

#define SCHEDULER_DATA_PARAMETER_IMAGE_WIDTH                                        ( 9 + SCHEDULER_DATA_PARAMETER_IMAGES_CURBE_SHIFT )
#define SCHEDULER_DATA_PARAMETER_IMAGE_HEIGHT                                       ( 10 + SCHEDULER_DATA_PARAMETER_IMAGES_CURBE_SHIFT )
#define SCHEDULER_DATA_PARAMETER_IMAGE_DEPTH                                        ( 11 + SCHEDULER_DATA_PARAMETER_IMAGES_CURBE_SHIFT )
#define SCHEDULER_DATA_PARAMETER_IMAGE_CHANNEL_DATA_TYPE                            ( 12 + SCHEDULER_DATA_PARAMETER_IMAGES_CURBE_SHIFT )
#define SCHEDULER_DATA_PARAMETER_IMAGE_CHANNEL_ORDER                                ( 13 + SCHEDULER_DATA_PARAMETER_IMAGES_CURBE_SHIFT )
#define SCHEDULER_DATA_PARAMETER_IMAGE_ARRAY_SIZE                                   ( 18 + SCHEDULER_DATA_PARAMETER_IMAGES_CURBE_SHIFT )
#define SCHEDULER_DATA_PARAMETER_IMAGE_NUM_SAMPLES                                  ( 20 + SCHEDULER_DATA_PARAMETER_IMAGES_CURBE_SHIFT )
#define SCHEDULER_DATA_PARAMETER_IMAGE_NUM_MIP_LEVELS                               ( 27 + SCHEDULER_DATA_PARAMETER_IMAGES_CURBE_SHIFT )
#define SCHEDULER_DATA_PARAMETER_IMAGE_OBJECT_ID                                    ( 35 + SCHEDULER_DATA_PARAMETER_IMAGES_CURBE_SHIFT )
#define SCHEDULER_DATA_PARAMETER_IMAGE_SRGB_CHANNEL_ORDER                           ( 39 + SCHEDULER_DATA_PARAMETER_IMAGES_CURBE_SHIFT )

#define DATA_PARAMETER_SAMPLER_ADDRESS_MODE                                         ( 14 + SCHEDULER_DATA_PARAMETER_SAMPLER_ADDED_VALUE )
#define DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS                                    ( 15 + SCHEDULER_DATA_PARAMETER_SAMPLER_ADDED_VALUE )
#define DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED                          ( 21 + SCHEDULER_DATA_PARAMETER_SAMPLER_ADDED_VALUE )
#define SCHEDULER_DATA_PARAMETER_SAMPLER_OBJECT_ID                                  ( 35 + SCHEDULER_DATA_PARAMETER_SAMPLER_ADDED_VALUE )

//CURBE STUFF, only entries that really needs to be patched
#define SCHEDULER_DATA_PARAMETER_KERNEL_ARGUMENT_MASK                               ( 1 << 1 )
#define SCHEDULER_DATA_PARAMETER_LOCAL_WORK_SIZE_MASK                               ( 1 << 2 )
#define SCHEDULER_DATA_PARAMETER_GLOBAL_WORK_SIZE_MASK                              ( 1 << 3 )
#define SCHEDULER_DATA_PARAMETER_NUM_WORK_GROUPS_MASK                               ( 1 << 4 )
#define SCHEDULER_DATA_PARAMETER_WORK_DIMENSIONS_MASK                               ( 1 << 5 )
#define SCHEDULER_DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES_MASK     ( 1 << 8 )
#define SCHEDULER_DATA_PARAMETER_GLOBAL_WORK_OFFSET_MASK                            ( 1 << 16 )
#define SCHEDULER_DATA_PARAMETER_NUM_HARDWARE_THREADS_MASK                          ( 1 << 17 )
#define SCHEDULER_DATA_PARAMETER_PARENT_EVENT_MASK                                  ( 1 << 22 )
#define SCHEDULER_DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE_MASK                      ( 1 << 28 )
#define SCHEDULER_DATA_PARAMETER_IMAGE_CURBE_ENTRIES                                ( ( ulong ) 1 << SCHEDULER_DATA_PARAMETER_IMAGES_CURBE_SHIFT )
#define SCHEDULER_DATA_PARAMETER_GLOBAL_POINTER                                     ( ( ( ulong ) 1 ) << SCHEDULER_DATA_PARAMETER_GLOBAL_POINTER_SHIFT )


#define SCHEDULER_DATA_PARAMETER_SAMPLER_MASK                                       ( ( ( ulong ) 1 ) << SCHEDULER_DATA_PARAMETER_SAMPLER_SHIFT )
//Error codes
#define SCHEDULER_CURBE_TOKEN_MISSED                                                10
#define SCHEDULER_CURBE_ARGUMENTS_SIZE_MISMATCH                                     11

#define CAN_BE_RECLAIMED                                                            123456

#define SCHEDULER_MSF_INITIAL                                                       1
#define SCHEDULER_MSF_SECOND                                                        2

//Uncomment to enable logging debug data
//#define    ENABLE_DEBUG_BUFFER    1

#ifdef ENABLE_DEBUG_BUFFER
//Update DebugDataInfo types in device_enqueue.h and PrintDebugDataBuffer() in cmd_queue_device.cpp

//Flags
#define    DDB_HAS_DATA_INFO        ( 1 << 0 )
#define    DDB_SCHEDULER_PROFILING  ( 1 << 1 )

#define    DDB_ALL                  ( 0xffffffff )

#endif

//Turn this to 1 to turn on debug calls, notice that it will cause up to 10 x longer time to build scheduler
//#define    SCHEDULER_DEBUG_MODE 1

//#define    DISABLE_RESOURCE_RECLAMATION                                                 1
/*
Resource reclamation procedure
1. Move all new command packets from queue_t to qstorage
2. In case there is place in storage for whole queue, reclaim space on queue
3. Construct stack basing on new commands added in the qstorage
4. Browse stack to find next item for execution
5. When you take item from the stack and schedule it , reclaim place on qstorage buffer
*/

typedef struct
{
    uint3           ActualLocalSize;
    uint3           WalkerDimSize;
    uint3           WalkerStartPoint;
} IGIL_WalkerData;

typedef struct
{
    uint3           LocalWorkSize;
    uint3           TotalDimSize;
    IGIL_WalkerData WalkerArray[ 8 ];
} IGIL_WalkerEnumeration;

inline void patchDword( __global uint* patchedDword, uint startOffset, uint endOffset, uint value )
{
    uint LeftMask  = ALL_BITS_SET_DWORD_MASK >> ( DWORD_SIZE_IN_BITS - endOffset - 1 );
    uint RightMask = ALL_BITS_SET_DWORD_MASK << ( startOffset );
    uint CleanMask = ~( RightMask & LeftMask );
    *patchedDword &= CleanMask;
    *patchedDword |= ( value << startOffset );
}

inline __global IGIL_KernelAddressData* IGIL_GetKernelAddressData( __global IGIL_KernelDataHeader* pKernelReflection, uint blockId )
{
    return ( __global IGIL_KernelAddressData* ) ( &pKernelReflection->m_data[ blockId ] );
}
__global IGIL_KernelData* IGIL_GetKernelData( __global IGIL_KernelDataHeader* pKernelReflection, uint blockId )
{
    __global IGIL_KernelAddressData* pKernelAddressData = IGIL_GetKernelAddressData( pKernelReflection, blockId );
    uint Offset                                         = pKernelAddressData->m_KernelDataOffset;
    __global char* pKernelReflectionRaw                 = ( __global char * ) pKernelReflection;
    return ( __global IGIL_KernelData* ) ( &pKernelReflectionRaw[ Offset ] );
}


inline __global IGIL_CommandHeader* TEMP_IGIL_GetCommandHeader( queue_t q, uint offset )
{
    __global uchar *pQueueRaw = __builtin_astype( q, __global uchar* );

    __global IGIL_CommandHeader* pCommand = ( __global IGIL_CommandHeader* )( pQueueRaw + offset );

    return pCommand;
}
//Make sure enough command packets are in command queue before calling this function.
__global IGIL_CommandHeader* TEMP_IGIL_GetNthCommandHeader( queue_t q, uint initialOffset, uint number )
{
    __global uchar *pQueueRaw = __builtin_astype( q, __global uchar* );

    __global IGIL_CommandHeader* pCommand = ( __global IGIL_CommandHeader* )( pQueueRaw + initialOffset );
    uint Offset = initialOffset;
    //Traverse queue_t unless nth command packet is found
    while( number > 0 )
    {
        Offset  += pCommand->m_commandSize;
        pCommand = TEMP_IGIL_GetCommandHeader( q, Offset );
        number--;
    }

    return pCommand;
}

//Make sure enough command packets are in command queue before calling this function.
uint TEMP_IGIL_GetNthCommandHeaderOffset( queue_t q, uint initialOffset, uint number )
{
    __global uchar *pQueueRaw = __builtin_astype( q, __global uchar* );

    __global IGIL_CommandHeader* pCommand = ( __global IGIL_CommandHeader* )( pQueueRaw + initialOffset );
    uint Offset = initialOffset;
    //Traverse queue_t unless nth command packet is found
    while( number > 0 )
    {
        Offset  += pCommand->m_commandSize;
        pCommand = TEMP_IGIL_GetCommandHeader( q, Offset );
        number--;
    }

    return Offset;
}

__global IGIL_CommandHeader* GetCommandHeaderFromStorage( __global uint* queueStorage, uint offset )
{
    __global uchar *pQueueRaw = ( __global uchar * ) queueStorage;

    __global IGIL_CommandHeader* pCommand = ( __global IGIL_CommandHeader* )( pQueueRaw + offset );

    return pCommand;
}

inline queue_t TEMP_IGIL_GetQueueT( IGIL_CommandHeader * queuePtr )
{
    return __builtin_astype( queuePtr, queue_t );
}

inline __global IGIL_DeviceEvent* TEMP_IGIL_GetDeviceEvents( __global IGIL_EventPool *pool )
{
    return ( __global IGIL_DeviceEvent * )( pool + 1 );
}

inline __global IGIL_DeviceEvent* TEMP_IGIL_GetDeviceEvent( __global IGIL_EventPool *pool, uint eventId )
{
    __global IGIL_DeviceEvent * pEvent = ( __global IGIL_DeviceEvent * )( pool + 1 );

    return ( __global IGIL_DeviceEvent * )( pEvent + eventId );
}
bool SetEventState( __global IGIL_EventPool *pool, uint eventId, int state )
{
    __global IGIL_DeviceEvent* pDeviceEvent = TEMP_IGIL_GetDeviceEvent( pool, eventId );
    pDeviceEvent->m_state = state;
    return true;
}

void TEMP_IGIL_FreeEvent( clk_event_t event, __global IGIL_EventPool *pool )
{
    //Offset into the event data in the pool
    __global IGIL_DeviceEvent *events = TEMP_IGIL_GetDeviceEvents( pool );

    atomic_xchg( &events[ ( uint )(size_t)__builtin_astype( event, void* ) ].m_state, IGIL_EVENT_UNUSED );
}

void  IGILLOCAL_MEMCPY_GTOG( __global void* pDst, __global void* pSrc, int numBytes )
{
    numBytes = numBytes >> 2;
    for( int i = 0; i < numBytes; i++ ) 
    {
        ( ( __global uint* )pDst ) [ i ] = ( ( __global int* )pSrc )[ i ];
    }
}

//Global memcpy running on all witems possible, make sure it's run from all hw threads.
void GLOBAL_MEMCPYUINT( __global void* pDst, __global void* pSrc, int numBytes )
{
    uint total_local_size   = get_local_size( 0 );
    uint LoopCtr            = numBytes / ( total_local_size * DWORD_SIZE_IN_BYTES );
    uint LeftOver           = numBytes % ( total_local_size * DWORD_SIZE_IN_BYTES );
    uint Lid                = get_local_id( 0 );
    uint i                  = 0;

    //Main copy
    for( i = 0; i < LoopCtr; i++ )
    {
         ( ( __global uint* )pDst ) [ Lid + total_local_size * i ] = ( ( __global uint* )pSrc )[ Lid + total_local_size * i ];
    }
    //Copy what's left
    if( LeftOver != 0 )
    {
        if( Lid * DWORD_SIZE_IN_BYTES < LeftOver )
        {
            ( ( __global uint* )pDst ) [ Lid + total_local_size * i ] = ( ( __global uint* )pSrc )[ Lid + total_local_size * i ];
        }
    }
}

//In SIMD8 to fully use cachelines copy in portions of uint2 , 8 bytes x 8 witems = 64 bytes cacheline size.
//Global memcpy running on all witems possible, make sure it's run from all hw threads.
void GLOBAL_MEMCPY( __global void* pDst, __global void* pSrc, int numBytes )
{
    //In case I need dword copy use uint version of this function.
    if( ( numBytes % ( DWORD_SIZE_IN_BYTES * 2 ) ) != 0 )
    {
       GLOBAL_MEMCPYUINT( pDst, pSrc, numBytes );
    }
    else
    {
        uint total_local_size   = get_local_size( 0 );
        uint LoopCtr            = numBytes / ( total_local_size * DWORD_SIZE_IN_BYTES * 2 );
        uint LeftOver           = numBytes % ( total_local_size * DWORD_SIZE_IN_BYTES * 2 );
        uint Lid                = get_local_id( 0 );
        uint i                  = 0;

        //Main copy
        for( i = 0; i < LoopCtr; i++ )
        {
             ( ( __global uint2* )pDst ) [ Lid + total_local_size * i ] = ( ( __global uint2* )pSrc )[ Lid + total_local_size * i ];
        }
        //Copy what's left
        if( LeftOver != 0 )
        {
            if( Lid * DWORD_SIZE_IN_BYTES * 2 < LeftOver )
            {
                ( ( __global uint2* )pDst ) [ Lid + total_local_size * i ] = ( ( __global uint2* )pSrc )[ Lid + total_local_size * i ];
            }
        }
    }
}

//This works only for 32 bit types
uint GetNextPowerof2( uint number )
{
    number --;
    number |= number >> 1;
    number |= number >> 2;
    number |= number >> 4;
    number |= number >> 8;
    number |= number >> 16;
    number ++;
    return number;
}

#define OCLRT_ALIGN( a, b )                 ( ( ( ( a ) % ( b ) ) != 0 ) ?  ( ( a ) - ( ( a ) % ( b ) ) + ( b ) ) : ( a ) )
#define OCLRT_MAX( a, b )                   ( ( ( a ) > ( b ) ) ?  ( a ) : ( b ) )

#ifndef SCHEDULER_EMULATION
#include "scheduler_definitions.h"
#endif

#ifdef ENABLE_DEBUG_BUFFER
//Adds private uint* data to debug buffer
//  ddb - global buffer to keep data
//  src - source data
//  numberOfElements
//  localID - WI ID , when 0xffffffff all WI copy data
//  returns 0 when data was copied, else -1
int AddToDebugBufferParallel( __global DebugDataBuffer* ddb, uint* pSrc, uint numberOfElements, uint localID )
{
    if( ddb->m_flags == 0 )
    {
        //All work items in local group copies data
        if( localID == 0xffffffff )
        {
            //Check if there is enough place for new data in ddb for every workitem in local group
            if( ( ( ddb->m_size / sizeof( uint ) - ddb->m_offset ) * get_local_size(0) ) >= numberOfElements )
            {
                uint startIndex = atomic_add( &ddb->m_offset, numberOfElements );
                uint i;
                int srcPos;
                for( i = startIndex, srcPos = 0; i < startIndex + numberOfElements; i++, srcPos++ ) 
                {
                    ddb->m_data[ i ] = pSrc[ srcPos ];
                }
                return 0;
            }
        }
        else
        {
            //Check if there is enough place for new data in ddb for one workitem in local group
            if( ( ddb->m_size / sizeof( uint ) - ddb->m_offset ) >= numberOfElements )
            {
                if( get_local_id(0) == localID )
                {
                    uint startIndex = atomic_add( &ddb->m_offset, numberOfElements );
                    uint i;
                    int srcPos;
                    for( i = startIndex, srcPos = 0; i < startIndex + numberOfElements; i++, srcPos++ ) 
                    {
                        ddb->m_data[ i ] = pSrc[ srcPos ];
                    }
                    return 0;
                }
            }
        }
    }
    return -1;
}

//Adds private uint to debug buffer
int AddToDBParallel(__global DebugDataBuffer* ddb, uint pSrc, uint localID)
{
    return AddToDebugBufferParallel(ddb, &pSrc, 1, localID);
}

//Adds global uint* data to debug buffer
//  ddb - global buffer to keep data
//  src - source data
//  numberOfElements
//  localID - WI ID , when 0xffffffff all WI copy data
//  returns 0 when data was copied, else -1
int AddGlobalToDebugBufferParallel( __global DebugDataBuffer* ddb, __global uint* pSrc, uint numberOfElements, uint localID )
{
    if( ddb->m_flags == 0 )
    {
        //All work items in local group copies data
        if( localID == 0xffffffff )
        {
            //Check if there is enough place for new data in ddb for every workitem in local group
            if( ( ( ddb->m_size / sizeof( uint ) - ddb->m_offset ) * get_local_size(0) ) >= numberOfElements )
            {
                uint startIndex = atomic_add( &ddb->m_offset, numberOfElements );
                uint i;
                int srcPos;
                for( i = startIndex, srcPos = 0; i < startIndex + numberOfElements; i++, srcPos++ ) 
                {
                    ddb->m_data[ i ] = pSrc[ srcPos ];
                }
                return 0;
            }
        }
        else
        {
            //Check if there is enough place for new data in ddb for one workitem in local group
            if( ( ddb->m_size / sizeof( uint ) - ddb->m_offset ) >= numberOfElements )
            {
                if( get_local_id(0) == localID )
                {
                    uint startIndex = atomic_add( &ddb->m_offset, numberOfElements );
                    uint i;
                    int srcPos;
                    for( i = startIndex, srcPos = 0; i < startIndex + numberOfElements; i++, srcPos++ ) 
                    {
                        ddb->m_data[ i ] = pSrc[ srcPos ];
                    }
                    return 0;
                }
            }
        }
    }
    return -1;
}



//Adds private uint data to debug buffer
//  ddb - global buffer to keep data
//  src - source data
//  dataType - enum defining added data type
//  returns 0 when data was copied, else -1
int AddToDebugBuffer( __global DebugDataBuffer* ddb, __private ulong src, uint dataType, uint localID )
{
    if( ddb->m_flags == 0 || ddb->m_flags == DDB_HAS_DATA_INFO )
    {
        //All work items in local group copies data
        if( localID == 0xffffffff )
        {
            //Check if there is enough place for new data in ddb
            if( ( ( ddb->m_stackTop - ddb->m_dataInfoTop ) >= 4 * get_local_size( 0 ) ) && ( ddb->m_dataInfoTop > ddb->m_stackTop ) )
            {
                //Check flags
                if( ddb->m_flags == 0 || ddb->m_flags == DDB_HAS_DATA_INFO )
                {
                    uint startIndex                 = atomic_add( &ddb->m_offset, 1 );
                    uint dataIndex                  = atomic_sub( &ddb->m_dataInfoTop, ddb->m_dataInfoSize );
                    uint stackTop                   = atomic_add( &ddb->m_stackTop, 8 );

                    __global uchar* pCharDebugQueue = ( __global uchar * )ddb;
                    __global uint* dest             = ( __global uint* )&pCharDebugQueue[ stackTop ];
                    __global DebugDataInfo* debugInfo;

                    dest[ 0 ]                 = ( uint )src & 0xffffffff;
                    dest[ 1 ]               = ( src & 0xffffffff00000000 ) >> 32;
                    debugInfo               = ( __global DebugDataInfo* )( &pCharDebugQueue[ dataIndex ] );
                    debugInfo->m_dataType   = ( DebugDataTypes )dataType;
                    debugInfo->m_dataSize   = 8;
                    ddb->m_flags            |= DDB_HAS_DATA_INFO;
                    return 0;
                }
            }
        }
        else
        {
            //Check if there is enough place for new data in ddb
            if( ( ( ddb->m_stackTop - ddb->m_dataInfoTop ) >= 8 ) && ( ddb->m_dataInfoTop > ddb->m_stackTop ) )
            {
                if( get_local_id( 0 ) == localID )
                {
                    uint startIndex                 = atomic_add( &ddb->m_offset, 1 );
                    uint dataIndex                  = atomic_sub( &ddb->m_dataInfoTop, ddb->m_dataInfoSize );
                    uint stackTop                   = atomic_add( &ddb->m_stackTop, 8 );

                    __global uchar* pCharDebugQueue = ( __global uchar * )ddb;
                    __global uint* dest             = ( __global uint* )&pCharDebugQueue[ stackTop ];
                    __global DebugDataInfo* debugInfo;

                    dest[ 0 ]                 = ( uint )src & 0xffffffff;
                    dest[ 1 ]               = ( src & 0xffffffff00000000 ) >> 32;
                    debugInfo               = ( __global DebugDataInfo* )( &pCharDebugQueue[ dataIndex ] );
                    debugInfo->m_dataType   = ( DebugDataTypes )dataType;
                    debugInfo->m_dataSize   = 8;
                    ddb->m_flags            |= DDB_HAS_DATA_INFO;
                    return 0;
                }

            }
        }
    }
    return -1;
}

//Adds data to debug buffer
//  ddb - global buffer to keep data
//  src - source
//  bytes - number of bytes from src to put into ddb
//  dataType - enum defining added data type
//  returns 0 when data was copied, else -1
int AddGlobalToDebugBuffer( __global DebugDataBuffer* ddb, __global uchar* src, uint bytes, uint dataType )
{
    if( get_local_id( 0 ) )
    {
        //Check if there is enough place for new data in ddb
        if( ( ( ddb->m_stackTop - ddb->m_dataInfoTop ) >= bytes ) && ( ddb->m_dataInfoTop > ddb->m_stackTop ) )
        {
            //Check flags
            if( ddb->m_flags == 0 || ddb->m_flags == DDB_HAS_DATA_INFO )
            {
                __global uchar* pCharDebugQueue = ( __global uchar * )ddb;
                __global DebugDataInfo* debugInfo;
                IGILLOCAL_MEMCPY_GTOG( ( &pCharDebugQueue[ ddb->m_stackTop ] ), ( src ), ( int )bytes );
                debugInfo = ( __global DebugDataInfo* )( &pCharDebugQueue[ ddb->m_dataInfoTop ] );
                debugInfo->m_dataType = ( DebugDataTypes )dataType;
                debugInfo->m_dataSize = bytes;

                ddb->m_dataInfoTop  = ddb->m_dataInfoTop - ddb->m_dataInfoSize;
                ddb->m_stackTop     = ddb->m_stackTop + bytes;
                ddb->m_offset       = ddb->m_offset + ( bytes / 4 );
                ddb->m_flags        |= DDB_HAS_DATA_INFO;
                return 0;
            }
        }
    }
    return -1;
}

int AddGlobalToDebugBufferAllIds( __global DebugDataBuffer* ddb, __global uchar* src, uint bytes, uint dataType, uint localID )
{
    if( ddb->m_flags == 0 || ddb->m_flags == DDB_HAS_DATA_INFO )
    {
        //Check if there is enough place for new data in ddb
        if( ( ( ddb->m_stackTop - ddb->m_dataInfoTop ) >= bytes ) && ( ddb->m_dataInfoTop > ddb->m_stackTop ) )
        {
            //Check flags
            if( ddb->m_flags == 0 || ddb->m_flags == DDB_HAS_DATA_INFO )
            {
                __global uchar* pCharDebugQueue = ( __global uchar * )ddb;
                __global DebugDataInfo* debugInfo;
                IGILLOCAL_MEMCPY_GTOG( ( &pCharDebugQueue[ ddb->m_stackTop ] ), ( src ), ( int )bytes );
                debugInfo = ( __global DebugDataInfo* )( &pCharDebugQueue[ ddb->m_dataInfoTop ] );
                debugInfo->m_dataType = ( DebugDataTypes )dataType;
                debugInfo->m_dataSize = bytes;

                ddb->m_dataInfoTop  = ddb->m_dataInfoTop - ddb->m_dataInfoSize;
                ddb->m_stackTop     = ddb->m_stackTop + bytes;
                ddb->m_offset       = ddb->m_offset + ( bytes / 4 );
                ddb->m_flags        |= DDB_HAS_DATA_INFO;
                return 0;
            }
        }
    }
    return -1;
}

#endif

#define MAX_SLB_OFFSET ( SECOND_LEVEL_BUFFER_SPACE_FOR_EACH_ENQUEUE * SECOND_LEVEL_BUFFER_NUMBER_OF_ENQUEUES )

#ifndef SCHEDULER_EMULATION
#include "scheduler_igdrcl_built_in.inl"
#endif

//SOME COMMON CODE FUNCTIONS
//COMMON CODE STARTS HERE
//Not thread safe - make sure it's called in thread safe fashion.

void patchIDData( __global char* dsh,
                  uint blockId,
                  uint numberOfHwThreads,
                  uint slmSize )
{
    __global char* DSHIData                      = ( __global char* )( dsh + SIZEOF_COLOR_CALCULATOR_STATE + ( ( blockId + 1 ) * SIZEOF_INTERFACE_DESCRIPTOR_DATA ) );
    __global uint* DSHIntData                    = ( __global uint* )( DSHIData );
    //Barrier enable is pre-patched on the host.
    patchDword( ( &DSHIntData[ INTERFACE_DESCRIPTOR_HWTHREADS_NUMBER_DWORD ] ), 0, INTERFACE_DESCRIPTOR_HWTHREADS_UPPER_BIT, numberOfHwThreads );
    
    //Patch SLM.
    uint SLMPatchValue = GetPatchValueForSLMSize( slmSize );
    patchDword( ( &DSHIntData[ INTERFACE_DESCRIPTOR_HWTHREADS_NUMBER_DWORD ] ), 16, 20, SLMPatchValue );
}
/*
this is how it works :
When constructing primary batch, first IDT table is also constructed, for all blocks, it is constructed as follows:
[0] - parent id
[1 .. x ] block id
[last aligned ] scheduler

now when we enter SLB, we forgot about first IDT, and we point all interface descriptor loads to point at scheduler which was last in the first IDT, to be first in the new IDT.

This way we can copy Interface Descriptors for blocks from the first IDT and assign Interface Descriptors dynamically in scheduler.
*/

void CopyAndPatchIDData( __global char* dsh,
                         uint blockId,
                         uint numberOfHwThreads,
                         uint slmSize,
                         uint interfaceDescriptorOffset,
                         uint blockStartId )
{
    __global char* DSHIData                      = ( __global char* )( dsh + SIZEOF_COLOR_CALCULATOR_STATE + ( ( blockId + blockStartId ) * SIZEOF_INTERFACE_DESCRIPTOR_DATA ) );
    __global uint* DSHIntData                    = ( __global uint* )( DSHIData );

    //Copy to ID InterfaceDescriptorOffset
    __global char* DSHDestIData                  = ( __global char* )( dsh + SIZEOF_COLOR_CALCULATOR_STATE + ( ( IDT_BREAKDOWN + interfaceDescriptorOffset ) * SIZEOF_INTERFACE_DESCRIPTOR_DATA ) );
    __global uint* DSHDestIntData                = ( __global uint* )( DSHDestIData );
    __global uint* DSHDestIntStartData           = DSHDestIntData;

    for( int i = 0; i < ( SIZEOF_INTERFACE_DESCRIPTOR_DATA / 4 ); i++ )
    {
        DSHDestIntData[ i ] = DSHIntData[ i ];
    }

    //Barrier enable is pre-patched on the host.
    patchDword( ( &DSHDestIntStartData[ INTERFACE_DESCRIPTOR_HWTHREADS_NUMBER_DWORD ] ), 0, INTERFACE_DESCRIPTOR_HWTHREADS_UPPER_BIT, numberOfHwThreads );
    
    //Patch SLM.
    uint SLMPatchValue = GetPatchValueForSLMSize( slmSize );
    patchDword( ( &DSHDestIntStartData[ INTERFACE_DESCRIPTOR_HWTHREADS_NUMBER_DWORD ] ), 16, 20, SLMPatchValue );
}

void CopyAndPatchIDData20( __global char* dsh,
                           uint blockId,
                           uint numberOfHwThreads,
                           uint slmSize,
                           uint interfaceDescriptorOffset,
                           uint blockStartId,
                           uint bToffset,
                           uint dshOffset,
                           uint numOfSamplers
#ifdef ENABLE_DEBUG_BUFFER                                              
                           , __global DebugDataBuffer* DebugQueue
#endif
                           )
{
    EMULATION_ENTER_FUNCTION( );

    __global char* DSHIData                      = ( __global char* )( dsh + SIZEOF_COLOR_CALCULATOR_STATE + ( ( blockId + blockStartId ) * SIZEOF_INTERFACE_DESCRIPTOR_DATA ) );
    __global uint* DSHIntData                    = ( __global uint* )( DSHIData );

    //Copy to ID InterfaceDescriptorOffset
    __global char* DSHDestIData                  = ( __global char* )( dsh + SIZEOF_COLOR_CALCULATOR_STATE + ( ( IDT_BREAKDOWN + interfaceDescriptorOffset ) * SIZEOF_INTERFACE_DESCRIPTOR_DATA ) );
    __global uint* DSHDestIntData                = ( __global uint* )( DSHDestIData );
    __global uint* DSHDestIntStartData           = DSHDestIntData;

    for( int i = 0; i < ( SIZEOF_INTERFACE_DESCRIPTOR_DATA / 4 ); i++ ) 
    {
        DSHDestIntData[ i ] = DSHIntData[ i ];
    }

    //Barrier enable is pre-patched on the host.
    patchDword( ( &DSHDestIntStartData[ INTERFACE_DESCRIPTOR_HWTHREADS_NUMBER_DWORD ] ), 0, INTERFACE_DESCRIPTOR_HWTHREADS_UPPER_BIT, numberOfHwThreads );
    
    //Patch BT offset
    patchDword( ( &DSHDestIntStartData[ INTERFACE_DESCRIPTOR_BINDING_TABLE_POINTER_DWORD ] ), 5, 15, ( bToffset >> 5 ) );

    //Patch SLM.
    uint PatchValue = GetPatchValueForSLMSize( slmSize );
    patchDword( ( &DSHDestIntStartData[ INTERFACE_DESCRIPTOR_HWTHREADS_NUMBER_DWORD ] ), 16, 20, PatchValue );

    PatchValue = ( DSHDestIntStartData[ INTERFACE_DESCRIPTOR_SAMPLER_STATE_TABLE_DWORD ] & 0xffffffe0 ) + ( dshOffset );
    patchDword( ( &DSHDestIntStartData[ INTERFACE_DESCRIPTOR_SAMPLER_STATE_TABLE_DWORD ] ), 5, 31, ( ( PatchValue ) >> 5 ) );

    //Samplers in multiple of 4
    numOfSamplers = ( numOfSamplers + 3 ) / 4;
    patchDword( ( &DSHDestIntStartData[ INTERFACE_DESCRIPTOR_SAMPLER_STATE_TABLE_DWORD ] ), 2, 4, numOfSamplers );
}


void patchGpGpuWalker(
    uint secondLevelBatchOffset,
    __global uint* secondaryBatchBuffer,
    uint interfaceDescriptorOffset,
    uint simdSize,
    uint totalLocalWorkSize,
    uint3 dimSize,
    uint3 startPoint,
    uint numberOfHwThreadsPerWg,
    uint indirectPayloadSize,
    uint ioHoffset )
{
    EMULATION_ENTER_FUNCTION( );

    //SlbOffset is expressed in bytes and for cmd it is needed to convert it to dwords
    uint CmdPacketStart = secondLevelBatchOffset / DWORD_SIZE_IN_BYTES;
    //INTERFACE_DESCRIPTOR for GPGPU_WALKER
    //INTERFACE DESCRIPTOR is one plus the block id
    uint PatchOffset = CmdPacketStart + GPGPU_WALKER_INTERFACE_DESCRIPTOR_ID_OFFSET;
    //Patch id data
    patchDword( &( secondaryBatchBuffer[ PatchOffset ] ),
                0, 5, ( interfaceDescriptorOffset ) );
    PatchOffset = CmdPacketStart + GPGPU_WALKER_THREAD_WIDTH_DWORD;
    //THREAD_WIDTH for GPGPU_WALKER
    patchDword( &( secondaryBatchBuffer[ PatchOffset ] ),
                0, 5, ( numberOfHwThreadsPerWg - 1 ) );

    PatchOffset = CmdPacketStart + GPGPU_WALKER_SIMDSIZE_DWORD;
    
    //SIMD SIZE for GPGPU_WALKER
    //Double Check the bits for SIMDSize
    if( simdSize == 8 )
    {
        patchDword( &( secondaryBatchBuffer[ PatchOffset ] ),
                    30, 31, SIMD8 );
    }
    else if ( simdSize == 16 )
    {
        patchDword( &( secondaryBatchBuffer[ PatchOffset ] ),
                    30, 31, SIMD16 );
    }
    else 
    {
        patchDword( &( secondaryBatchBuffer[ PatchOffset ] ),
                    30, 31, SIMD32 );
    }
    
    //XDIM for GPGPU_WALKER
    secondaryBatchBuffer[ CmdPacketStart + GPGPU_WALKER_XDIM_DWORD ] = dimSize.x;
    secondaryBatchBuffer[ CmdPacketStart + GPGPU_WALKER_GROUP_ID_START_X ] = startPoint.x;
    //YDIM
    secondaryBatchBuffer[ CmdPacketStart + GPGPU_WALKER_YDIM_DWORD ] = dimSize.y;
    secondaryBatchBuffer[ CmdPacketStart + GPGPU_WALKER_GROUP_ID_START_Y ] = startPoint.y;
    //ZDIM for GPGPU_WALKER
    secondaryBatchBuffer[ CmdPacketStart + GPGPU_WALKER_ZDIM_DWORD ] = dimSize.z;
    secondaryBatchBuffer[ CmdPacketStart + GPGPU_WALKER_GROUP_ID_START_Z ] = startPoint.z;

    //XMASK for GPGPU_WALKER
    uint mask = ( 1 << ( totalLocalWorkSize % simdSize ) ) - 1;
    if( mask == 0 )
        mask = ~0;

    secondaryBatchBuffer[ CmdPacketStart + GPGPU_WALKER_XMASK_DWORD ] = mask;

    //YMASK for GPGPU_WALKER
    uint YMask = ~0;

    secondaryBatchBuffer[ CmdPacketStart + GPGPU_WALKER_YMASK_DWORD ] = YMask;


    patchDword( &( secondaryBatchBuffer[ CmdPacketStart + GPGPU_WALKER_INDIRECT_DATA_LENGTH_OFFSET ] ),
                0, 16, indirectPayloadSize );

    patchDword( &( secondaryBatchBuffer[ CmdPacketStart + GPGPU_WALKER_INDIRECT_START_ADDRESS_OFFSET ] ),
                0, 31, ioHoffset );
}

int PatchMediaStateFlush(
    uint secondLevelBatchOffset,
    __global uint* secondaryBatchBuffer,
    uint interfaceDescriptorOffset,
    uint msfNumber )
{
    //SlbOffset is expressed in bytes and for cmd it is needed to convert it to dwords
    uint CmdPacketStart = secondLevelBatchOffset / DWORD_SIZE_IN_BYTES;
    uint MsfOffset;

    if( msfNumber == SCHEDULER_MSF_INITIAL )
    {
        MsfOffset = MEDIA_STATE_FLUSH_INITIAL_INTERFACE_DESCRIPTOR_OFFSET;
    }
    else if ( msfNumber == SCHEDULER_MSF_SECOND )
    {
        MsfOffset = MEDIA_STATE_FLUSH_INTERFACE_DESCRIPTOR_OFFSET;
    }
    else
    {
        return -1;
    }
    patchDword( &( secondaryBatchBuffer[ CmdPacketStart + MsfOffset ] ), 0, 5, interfaceDescriptorOffset );

    return 0;
}

#if defined WA_LRI_COMMANDS_EXIST
void PatchMiLoadRegisterImm(
    uint secondLevelBatchOffset,
    __global uint* secondaryBatchBuffer,
    uint enqueueOffset,
    uint registerAddress,
    uint value )
{
    //SlbOffset is expressed in bytes and for cmd it is needed to convert it to dwords
    uint CmdPacketStart = secondLevelBatchOffset / DWORD_SIZE_IN_BYTES;

    secondaryBatchBuffer[ CmdPacketStart + enqueueOffset ] = OCLRT_LOAD_REGISTER_IMM_CMD;
    patchDword( &( secondaryBatchBuffer[ CmdPacketStart + enqueueOffset + IMM_LOAD_REGISTER_ADDRESS_DWORD_OFFSET ] ), 2, 22, registerAddress >> 2 );
    secondaryBatchBuffer[ CmdPacketStart + enqueueOffset + IMM_LOAD_REGISTER_VALUE_DWORD_OFFSET ] = value;
}

void AddMiLoadRegisterImm(
    __global uint* secondaryBatchBuffer,
    __private uint* dwordOffset,
    uint value )
{
    secondaryBatchBuffer[ *dwordOffset ] = OCLRT_LOAD_REGISTER_IMM_CMD;
    ( *dwordOffset )++;
    secondaryBatchBuffer[ *dwordOffset ] = 0;
    patchDword( &( secondaryBatchBuffer[ *dwordOffset ] ), 2, 22, CTXT_PREMP_DBG_ADDRESS_VALUE >> 2 );
    ( *dwordOffset )++;
    secondaryBatchBuffer[ *dwordOffset ] = value; //CTXT_PREMP_ON_MI_ARB_CHECK_ONLY or CTXT_PREMP_DEFAULT_VALUE
    ( *dwordOffset )++;
}

void SetDisablePreemptionRegister(
    uint secondLevelBatchOffset,
    __global uint* secondaryBatchBuffer )
{
    PatchMiLoadRegisterImm( secondLevelBatchOffset,
                            secondaryBatchBuffer,
                            IMM_LOAD_REGISTER_FOR_DISABLE_PREEMPTION_OFFSET,
                            CTXT_PREMP_DBG_ADDRESS_VALUE,
                            CTXT_PREMP_ON_MI_ARB_CHECK_ONLY );
}

void SetEnablePreemptionRegister(
    uint secondLevelBatchOffset,
    __global uint* secondaryBatchBuffer )
{
    PatchMiLoadRegisterImm( secondLevelBatchOffset,
                            secondaryBatchBuffer,
                            IMM_LOAD_REGISTER_FOR_ENABLE_PREEMPTION_OFFSET,
                            CTXT_PREMP_DBG_ADDRESS_VALUE,
                            CTXT_PREMP_DEFAULT_VALUE );
}

void NoopPreemptionCommand(
    uint secondLevelBatchOffset,
    uint cmdOffset,
    __global uint* secondaryBatchBuffer )
{
    uint CmdPacketStart = cmdOffset + secondLevelBatchOffset / DWORD_SIZE_IN_BYTES;
    for( int i = 0; i < OCLRT_IMM_LOAD_REGISTER_CMD_DEVICE_CMD_DWORD_OFFSET; i++ )
    {
        secondaryBatchBuffer[ CmdPacketStart + i ] = 0;
    }
}
#endif //WA_LRI_COMMANDS_EXIST


//PQueue is needed for SLBOffset
void AddCmdsInSLBforScheduler20Parallel( uint slbOffset,
                                        __global IGIL_CommandQueue* pQueue,
                                        __global uint * secondaryBatchBuffer,
                                        __global char * dsh )
{
    EMULATION_ENTER_FUNCTION( );
#ifdef SCHEDULER_EMULATION
    uint3 StartPoint = { 0, 0, 0 };
    uint3 DimSize = { get_num_groups( 0 ), 1, 1 };
#else
    uint3 StartPoint = ( uint3 )( 0 );
    uint3 DimSize = ( uint3 )( get_num_groups( 0 ), 1, 1 );
#endif
    patchGpGpuWalker( slbOffset,
                      secondaryBatchBuffer,
                      0,
                      PARALLEL_SCHEDULER_COMPILATION_SIZE_20,
                      get_local_size(0),
                      DimSize,
                      StartPoint,
                      PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP20,
                      SIZEOF_3GRFS * PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP20 + pQueue->m_controls.m_SchedulerConstantBufferSize,
                      pQueue->m_controls.m_SchedulerDSHOffset );

    PatchMediaStateFlush( slbOffset, secondaryBatchBuffer, 0, SCHEDULER_MSF_INITIAL );
    PatchMediaStateFlush( slbOffset, secondaryBatchBuffer, 0, SCHEDULER_MSF_SECOND );

//When commands exists and scheduler does not require preemption off, noop the commands space
#if defined WA_LRI_COMMANDS_EXIST
#if defined WA_SCHEDULER_PREEMPTION
    if( pQueue->m_controls.m_EventTimestampAddress == 0u )
    {
        SetEnablePreemptionRegister( slbOffset, secondaryBatchBuffer );
        SetDisablePreemptionRegister( slbOffset, secondaryBatchBuffer );
    }
    else
    {
        NoopPreemptionCommand( slbOffset, IMM_LOAD_REGISTER_FOR_ENABLE_PREEMPTION_OFFSET, secondaryBatchBuffer );
        NoopPreemptionCommand( slbOffset, IMM_LOAD_REGISTER_FOR_DISABLE_PREEMPTION_OFFSET, secondaryBatchBuffer );
    }
#else
    //This is case, where LRI preemption is not required around scheduler WALKERs, but space for LRI commands exists, make sure they are nooped then
    NoopPreemptionCommand( SLBOffset, IMM_LOAD_REGISTER_FOR_ENABLE_PREEMPTION_OFFSET, secondaryBatchBuffer );
    NoopPreemptionCommand( SLBOffset, IMM_LOAD_REGISTER_FOR_DISABLE_PREEMPTION_OFFSET, secondaryBatchBuffer );
#endif //WA_SCHEDULER_PREEMPTION
#endif //WA_LRI_COMMANDS_EXIST
}

int generateLocalIDSParallel20(
    __global char* dsh,
    uint3 localSize,
    uint hwThreads,
    uint simdSize )
{
    uint it, currX, currY, currZ, FlattendID;

    uint Max = 1;
    if( simdSize == 32 )
    {
        Max = 2;
    }

    //Update full GRFs, each WI generate ID for one work item in x,y and z
    //in case we generate SIMD8 payload using 16 wi , idle half of them
    if( get_local_id( 0 ) < simdSize )
    {
        for( it = 0; it < hwThreads; it++ )
        {
            for( uint multip = 0; multip < Max; multip++ )
            {
                //We are in simd 8, each wi process generation for 1 wi
                FlattendID = get_local_id( 0 ) + it * simdSize + 16 * ( multip );

                currX = FlattendID % localSize.x;
                currY = ( FlattendID / localSize.x ) % localSize.y;
                currZ = ( FlattendID / ( localSize.x * localSize.y ) );//not needed % localSize.z;

                *( __global ushort * )( dsh + get_local_id( 0 ) * 2 + it * GRF_SIZE * 3 * Max + multip * GRF_SIZE )                                   = ( ushort )currX;
                *( __global ushort * )( dsh + get_local_id( 0 ) * 2 + it * GRF_SIZE * 3 * Max + GRF_SIZE * Max + multip * GRF_SIZE )                  = ( ushort )currY;
                *( __global ushort * )( dsh + get_local_id( 0 ) * 2 + it * GRF_SIZE * 3 * Max + GRF_SIZE * Max + GRF_SIZE * Max + multip * GRF_SIZE ) = ( ushort )currZ;
            }
        }
    }
    return 0;
}

//Function generate local ids.
//SIMD16 version
int generateLocalIDSsimd16(
    __global char* dsh,
    uint3 localSize,
    uint hwThreads)
{
    typedef union 
    {
        ushort16 vectors;
        ushort varray[ 16 ];
    }vectorUnion;
    
    __private vectorUnion LidX;
    __private vectorUnion LidY;
    __private vectorUnion LidZ;

    __private ushort currX = 0;
    __private ushort currY = 0;
    __private ushort currZ = 0;

    //Assuming full load of hw thread , remainder done separately
    for(uint it = 0; it < hwThreads; it++ )
    {
        //This will be unrolled by compiler
        for(uint x = 0; x < 16; x++ )
        {
            LidX.varray[ x ] = currX++;
            LidY.varray[ x ] = currY;
            LidZ.varray[ x ] = currZ;
            
            if( currX == localSize.x )
            {
                currX = 0;
                currY++;
            }

            if( currY == localSize.y )
            {
                currY = 0;
                currZ++;
            }
        }

        *( __global ushort16 * )( dsh + it * GRF_SIZE * 3 )                          = LidX.vectors;
        *( __global ushort16 * )( dsh + it * GRF_SIZE * 3 + GRF_SIZE )               = LidY.vectors;
        *( __global ushort16 * )( dsh + it * GRF_SIZE * 3 + GRF_SIZE + GRF_SIZE )    = LidZ.vectors;

    }

    return 0;
}

//Function generate local ids.
//SIMD8 version
int generateLocalIDSsimd8(
    __global char* dsh,
    uint3 localSize,
    uint hwThreads)
{
    typedef union 
    {
        ushort8 vectors;
        ushort varray[ 8 ];
    }vectorUnion;

    __private vectorUnion LidX;
    __private vectorUnion LidY;
    __private vectorUnion LidZ;

    __private ushort currX = 0;
    __private ushort currY = 0;
    __private ushort currZ = 0;

    //Assuming full load of hw thread , remainder done separately
    for(uint it = 0; it < hwThreads; it++ )
    {
        //This will be unrolled by compiler
        for(uint x = 0; x < 8; x++ )
        {
            LidX.varray[ x ] = currX++;
            LidY.varray[ x ] = currY;
            LidZ.varray[ x ] = currZ;

            if( currX == localSize.x )
            {
                currX = 0;
                currY++;
            }

            if( currY == localSize.y )
            {
                currY = 0;
                currZ++;
            }
        }

        *( __global ushort8 * )( dsh + it * GRF_SIZE * 3 )                          = LidX.vectors;
        *( __global ushort8 * )( dsh + it * GRF_SIZE * 3 + GRF_SIZE )               = LidY.vectors;
        *( __global ushort8 * )( dsh + it * GRF_SIZE * 3 + GRF_SIZE + GRF_SIZE )    = LidZ.vectors;
    }

    return 0;
}

//Function patches a curbe parametr , this version of function supports only these curbe tokens that may appear only once
int PatchDSH1Token( int currentIndex, uint tokenType, __global IGIL_KernelCurbeParams* pKernelCurbeParams, __global char* pDsh,
                    uint value )
{
    EMULATION_ENTER_FUNCTION( );

    uint PatchOffset;
#if SCHEDULER_DEBUG_MODE
    //If we are here it means that mask is ok and there are at least 3 curbe tokens that needs to be patched, do it right away
    if( pKernelCurbeParams[ CurrentIndex ].m_parameterType != TokenType )
    {
        return -1;
    }
#endif
    PatchOffset = pKernelCurbeParams[ currentIndex ].m_patchOffset;
    *( __global uint * )( &pDsh[ PatchOffset ] ) = value;

    currentIndex++;
    return currentIndex;
}

int PatchLocalMemEntities( int currentIndex, uint tokenType, __global IGIL_KernelCurbeParams* pKernelCurbeParams, __global char* pDsh,
                           __global IGIL_CommandHeader* pCommand )
{
    uint PatchOffset;
#if SCHEDULER_DEBUG_MODE
    //If we are here it means that mask is ok and there are at least 3 curbe tokens that needs to be patched, do it right away
    if( pKernelCurbeParams[ CurrentIndex ].m_parameterType != TokenType )
    {
        return -1;
    }
#endif
    //First patch is with 0
    PatchOffset  = pKernelCurbeParams[ currentIndex ].m_patchOffset;

    //SUM_OF_LOCAL_MEMORY_KERNEL_ARGS can be a 4 or 8 byte patch
    if( pKernelCurbeParams[currentIndex].m_parameterSize == sizeof( ulong ) )
    {
        *( __global ulong * )( &pDsh[PatchOffset] ) = 0;
    }
    else
    {
        *( __global uint * )( &pDsh[ PatchOffset ] ) = 0;
    }


    currentIndex++;
    uint Alignement;
    uint iter = 0;
    uint CurrentSum = 0;
    uint CurrentValue;
    //For each global captured there will be uint with index and ulong with address.
    uint GlobalPointersSize = ( pCommand->m_numGlobalCapturedBuffer * ( sizeof( ulong ) +  sizeof( uint ) ) ) / sizeof( uint );

    __global uint* pLocalMemSizes = &pCommand->m_data[ pCommand->m_numDependencies + pCommand->m_numScalarArguments + GlobalPointersSize ];
    //Check if there is second surface
    while( pKernelCurbeParams[ currentIndex ].m_parameterType == tokenType )
    {
        PatchOffset  = pKernelCurbeParams[ currentIndex ].m_patchOffset;

        //Value needs to be aligned to the value stored in sourceoffset
        Alignement = OCLRT_MAX( DWORD_SIZE_IN_BYTES, pKernelCurbeParams[ currentIndex ].m_sourceOffset );

        CurrentValue = pLocalMemSizes[ iter ];
        CurrentValue = OCLRT_ALIGN( CurrentValue, Alignement );
        CurrentSum   += CurrentValue;

        //SUM_OF_LOCAL_MEMORY_KERNEL_ARGS can be a 4 or 8 byte patch
        if( pKernelCurbeParams[currentIndex].m_parameterSize == sizeof( ulong ) )
        {
            *( __global ulong * )( &pDsh[PatchOffset] ) = ( ulong )CurrentSum;
        }
        else
        {
            *( __global uint * )( &pDsh[ PatchOffset ] ) = CurrentSum;
        }

        currentIndex++;
        iter++;
    }
    return currentIndex;
}

//Function patches a curbe parametr , this version of function supports only these curbe tokens that may appear only once
int PatchDSH1TokenParallel20( int currentIndex, uint tokenType, __global IGIL_KernelCurbeParams* pKernelCurbeParams, __global char* pDsh,
                              uint value )
{
    EMULATION_ENTER_FUNCTION( );

    uint PatchOffset;
    if( get_local_id( 0 ) == PARALLEL_SCHEDULER_COMPILATION_SIZE_20 )
    {
        PatchOffset  = pKernelCurbeParams[ currentIndex ].m_patchOffset;
        *( __global uint * ) ( &pDsh[ PatchOffset ] ) = value;
    }
    currentIndex++;
    return currentIndex;
}

//Function patches a curbe parametr, this version of function works on 3d curbe tokens
//It assumes that at least 3 tokens exists, then checks if 3 additional patches are needed
int PatchDSH6TokensParallel20( int currentIndex, uint tokenType, __global IGIL_KernelCurbeParams* pKernelCurbeParams, __global char* pDsh,
                               uint value1, uint value2, uint value3 )
{
    EMULATION_ENTER_FUNCTION( );

    uint PatchOffset, SourceOffset;
    uint WorkingOffset;
    uint ShiftSize;

    //Check if we patch 3 or 6 curbe tokens
    if( pKernelCurbeParams[ currentIndex + 3 ].m_parameterType == tokenType )
    {
        ShiftSize = 6;
    }
    else
    {
        ShiftSize = 3;
    }

    if( get_local_id( 0 ) < PARALLEL_SCHEDULER_COMPILATION_SIZE_20 + ShiftSize )
    {
        WorkingOffset = currentIndex + get_local_id( 0 ) - PARALLEL_SCHEDULER_COMPILATION_SIZE_20;
        PatchOffset   = pKernelCurbeParams[ WorkingOffset ].m_patchOffset;
        SourceOffset  = pKernelCurbeParams[ WorkingOffset ].m_sourceOffset;

        if( SourceOffset == 0 )
        {
            *( __global uint * )( &pDsh[ PatchOffset ] ) = value1;
        }
        else if( SourceOffset == 4 )
        {
            *( __global uint * )( &pDsh[ PatchOffset ] ) = value2;
        }
        else if( SourceOffset == 8 )
        {
            *( __global uint * )( &pDsh[ PatchOffset ] ) = value3;
        }
    }

    currentIndex += ShiftSize;
    return currentIndex;
}

int PatchLocalWorkSizes( int currentIndex, uint tokenType, __global IGIL_KernelCurbeParams* pKernelCurbeParams, __global char* pDsh,
                         uint enqLocalX, uint enqLocalY, uint enqLocalZ, uint cutLocalX, uint cutLocalY, uint cutLocalZ )
{
    EMULATION_ENTER_FUNCTION( );

    uint PatchOffset, SourceOffset;

    //Tokens are sorted by m_sourceOffset, it means that first 3 keys are always used to compute global_id and are always present
    for( uint it = 0; it < 3; it++ )
    {
        PatchOffset  = pKernelCurbeParams[ currentIndex ].m_patchOffset;
        SourceOffset = pKernelCurbeParams[ currentIndex ].m_sourceOffset;

        if( SourceOffset == 0 )
        {
            *( __global uint * )( &pDsh[ PatchOffset ] ) = enqLocalX;
        }
        else if( SourceOffset == 4 )
        {
            *( __global uint * )( &pDsh[ PatchOffset ] ) = enqLocalY;
        }
        else if( SourceOffset == 8 )
        {
            *( __global uint * )( &pDsh[ PatchOffset ] ) = enqLocalZ;
        }
        currentIndex++;
    }
    //If there are 3 more tokens, it means that get_local_size is used within a kernel, to deal with it patch with the second set of variables
    if( pKernelCurbeParams[ currentIndex ].m_parameterType == tokenType )
    {
        for( uint it = 0; it < 3; it++ )
        {
            PatchOffset  = pKernelCurbeParams[ currentIndex ].m_patchOffset;
            SourceOffset = pKernelCurbeParams[ currentIndex ].m_sourceOffset;

            if( SourceOffset == 0 )
            {
                *( __global uint * )( &pDsh[ PatchOffset ] ) = cutLocalX;
            }
            else if( SourceOffset == 4 )
            {
                *( __global uint * )( &pDsh[ PatchOffset ] ) = cutLocalY;
            }
            else if( SourceOffset == 8 )
            {
                *( __global uint * )( &pDsh[ PatchOffset ] ) = cutLocalZ;
            }
            currentIndex++;
        }
    }
    return currentIndex;
}

//Function patches a curbe parametr, this version of function works on 3d curbe tokens
//It assumes that at least 3 tokens exists, then checks if 3 additional patches are needed
int PatchLocalWorkSizesParallel( int currentIndex, uint tokenType, __global IGIL_KernelCurbeParams* pKernelCurbeParams, __global char* pDsh,
                                 uint enqLocalX, uint enqLocalY, uint enqLocalZ, uint cutLocalX, uint cutLocalY, uint cutLocalZ )
{
    EMULATION_ENTER_FUNCTION( );

    uint ShiftSize;

    //Check if we patch 3 or 6 curbe tokens
    if( pKernelCurbeParams[ currentIndex + 3 ].m_parameterType == tokenType )
    {
        ShiftSize = 6;
    }
    else
    {
        ShiftSize = 3;
    }

    //Use single threaded version
    if( get_local_id( 0 ) == PARALLEL_SCHEDULER_COMPILATION_SIZE_20 )
    {
        PatchLocalWorkSizes( currentIndex, SCHEDULER_DATA_PARAMETER_LOCAL_WORK_SIZE, pKernelCurbeParams, pDsh, enqLocalX, enqLocalY, enqLocalZ, cutLocalX, cutLocalY, cutLocalZ );
    }

    currentIndex += ShiftSize;
    return currentIndex;
}

//Function patches a curbe parametr, this version of function works on 3d curbe tokens
//It assumes that at least 3 tokens exists, then checks if 3 additional patches are needed
int PatchDSH6Tokens( int currentIndex, uint tokenType, __global IGIL_KernelCurbeParams* pKernelCurbeParams, __global char* pDsh,
                     uint value1, uint value2, uint value3 )
{
    EMULATION_ENTER_FUNCTION( );

    uint PatchOffset, SourceOffset;
#if SCHEDULER_DEBUG_MODE
    //If we are here it means that mask is ok and there are at least 3 curbe tokens that needs to be patched, do it right away
    if( pKernelCurbeParams[ CurrentIndex ].m_parameterType != TokenType )
    {
        return -1;
    }
#endif
    for( uint it = 0; it < 3; it++ )
    {
        PatchOffset  = pKernelCurbeParams[ currentIndex ].m_patchOffset;
        SourceOffset = pKernelCurbeParams[ currentIndex ].m_sourceOffset;

        if( SourceOffset == 0 )
        {
            *( __global uint * )( &pDsh[ PatchOffset ] ) = value1;
        }
        else if( SourceOffset == 4 )
        {
            *( __global uint * )( &pDsh[ PatchOffset ] ) = value2;
        }
        else if( SourceOffset == 8 )
        {
            *( __global uint * )( &pDsh[ PatchOffset ] ) = value3;
        }
        currentIndex++;
    }
    //Check if there are 3 more.
    if( pKernelCurbeParams[ currentIndex ].m_parameterType == tokenType )
    {
        for( uint it = 0; it < 3; it++ )
        {
            PatchOffset  = pKernelCurbeParams[ currentIndex ].m_patchOffset;
            SourceOffset = pKernelCurbeParams[ currentIndex ].m_sourceOffset;

            if( SourceOffset == 0 )
            {
                *( __global uint * )( &pDsh[ PatchOffset ] ) = value1;
            }
            else if( SourceOffset == 4 )
            {
                *( __global uint * )( &pDsh[ PatchOffset ] ) = value2;
            }
            else if( SourceOffset == 8 )
            {
                *( __global uint * )( &pDsh[ PatchOffset ] ) = value3;
            }
            currentIndex++;
        }
    }

    return currentIndex;
}
//Common code

inline __global char* GetPtrToCurbeData( uint offset, __global IGIL_KernelDataHeader * pKernelReflection )
{
    __global char * pRawKernelReflection = ( __global char * )pKernelReflection;
    return ( pRawKernelReflection + offset );
}

__global char* GetPtrToKernelReflectionOffset( uint offset, __global IGIL_KernelDataHeader * pKernelReflection )
{
    __global char * pRawKernelReflection = ( __global char * )pKernelReflection;
    return ( pRawKernelReflection + offset );
}

void InitWalkerDataParallel( __local IGIL_WalkerEnumeration* pWalkerEnumData,
                             uint workDim,
                             uint* pWalkerCount,
                             uint3 edgeArray,
                             uint3 globalDim,
                             uint3 globalSizes,
                             uint3 localSizes )
{
    EMULATION_ENTER_FUNCTION( );

    pWalkerEnumData->TotalDimSize.x = globalDim.x;
    pWalkerEnumData->TotalDimSize.y = globalDim.y;
    pWalkerEnumData->TotalDimSize.z = globalDim.z;
    
    pWalkerEnumData->WalkerArray[ 0 ].ActualLocalSize.x = localSizes.x;
    pWalkerEnumData->WalkerArray[ 0 ].WalkerStartPoint.x = 0;
    pWalkerEnumData->WalkerArray[ 0 ].WalkerDimSize.x = globalDim.x;

    pWalkerEnumData->WalkerArray[ 0 ].ActualLocalSize.y = localSizes.y;
    pWalkerEnumData->WalkerArray[ 0 ].WalkerStartPoint.y = 0;
    pWalkerEnumData->WalkerArray[ 0 ].WalkerDimSize.y = globalDim.y;

    pWalkerEnumData->WalkerArray[ 0 ].ActualLocalSize.z = localSizes.z;
    pWalkerEnumData->WalkerArray[ 0 ].WalkerStartPoint.z = 0;
    pWalkerEnumData->WalkerArray[ 0 ].WalkerDimSize.z = globalDim.z;

    uint WalkerCount = 1;

    if( edgeArray.x != 0 )
    {
        pWalkerEnumData->TotalDimSize.x++;

        pWalkerEnumData->WalkerArray[ 1 ].ActualLocalSize.x = edgeArray.x;
        pWalkerEnumData->WalkerArray[ 1 ].WalkerStartPoint.x = globalDim.x;
        pWalkerEnumData->WalkerArray[ 1 ].WalkerDimSize.x = pWalkerEnumData->TotalDimSize.x;

        pWalkerEnumData->WalkerArray[ 1 ].ActualLocalSize.y = localSizes.y;
        pWalkerEnumData->WalkerArray[ 1 ].WalkerStartPoint.y = 0;
        pWalkerEnumData->WalkerArray[ 1 ].WalkerDimSize.y = globalDim.y;

        pWalkerEnumData->WalkerArray[ 1 ].ActualLocalSize.z = localSizes.z;
        pWalkerEnumData->WalkerArray[ 1 ].WalkerStartPoint.z = 0;
        pWalkerEnumData->WalkerArray[ 1 ].WalkerDimSize.z = globalDim.z;

        WalkerCount++;
    }

    if( workDim > 1 )
    {
        if( edgeArray.y != 0 )
        {
            pWalkerEnumData->TotalDimSize.y++;

            pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.x = localSizes.x;
            pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.x = 0;
            pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.x = globalDim.x;

            pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.y = edgeArray.y;
            pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.y = globalDim.y;
            pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.y = pWalkerEnumData->TotalDimSize.y;

            pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.z = localSizes.z;
            pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.z = 0;
            pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.z = globalDim.z;

            WalkerCount++;
        }

        if( ( edgeArray.x != 0 ) & ( edgeArray.y != 0 ) )
        {
            pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.x = edgeArray.x;
            pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.x = globalDim.x;
            pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.x = pWalkerEnumData->TotalDimSize.x;

            pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.y = edgeArray.y;
            pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.y = globalDim.y;
            pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.y = pWalkerEnumData->TotalDimSize.y;

            pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.z = localSizes.z;
            pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.z = 0;
            pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.z = globalDim.z;

            WalkerCount++;
        }
        if( workDim > 2 )
        {
            if( edgeArray.z != 0 )
            {
                pWalkerEnumData->TotalDimSize.z++;

                pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.x = localSizes.x;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.x = 0;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.x = globalDim.x;

                pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.y = localSizes.y;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.y = 0;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.y = globalDim.y;

                pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.z = edgeArray.z;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.z = globalDim.z;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.z = pWalkerEnumData->TotalDimSize.z;

                WalkerCount++;
            }
            if( ( edgeArray.x != 0 ) & ( edgeArray.z != 0 ) )
            {
                pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.x = edgeArray.x;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.x = globalDim.x;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.x = pWalkerEnumData->TotalDimSize.x;

                pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.y = localSizes.y;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.y = 0;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.y = globalDim.y;

                pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.z = edgeArray.z;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.z = globalDim.z;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.z = pWalkerEnumData->TotalDimSize.z;

                WalkerCount++;
            }

            if( ( edgeArray.y != 0 ) & ( edgeArray.z != 0 ) )
            {
                pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.x = localSizes.x;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.x = 0;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.x = globalDim.x;

                pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.y = edgeArray.y;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.y = globalDim.y;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.y = pWalkerEnumData->TotalDimSize.y;

                pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.z = edgeArray.z;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.z = globalDim.z;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.z = pWalkerEnumData->TotalDimSize.z;

                WalkerCount++;
            }
            if( ( edgeArray.x != 0 ) & ( edgeArray.y != 0 ) & ( edgeArray.z != 0 ) )
            {
                pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.x = edgeArray.x;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.x = globalDim.x;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.x = pWalkerEnumData->TotalDimSize.x;

                pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.y = edgeArray.y;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.y = globalDim.y;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.y = pWalkerEnumData->TotalDimSize.y;

                pWalkerEnumData->WalkerArray[ WalkerCount ].ActualLocalSize.z = edgeArray.z;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerStartPoint.z = globalDim.z;
                pWalkerEnumData->WalkerArray[ WalkerCount ].WalkerDimSize.z = pWalkerEnumData->TotalDimSize.z;

                WalkerCount++;
            }
        }
    }

    *pWalkerCount = WalkerCount;
}

//Compute number of Walkers needed for this command packet, this function assumes that command packet is initialized
inline int GetWalkerCount( __global IGIL_CommandHeader* pCommand )
{
    int WalkerCount = 1;
    for( uint dim = 0; ( dim < pCommand->m_range.m_dispatchDimensions ); dim++ )
    {
        if( ( pCommand->m_range.m_globalWorkSize[ dim ] % pCommand->m_range.m_localWorkSize[ dim ] ) != 0 )
        {
            WalkerCount *= 2;
        }
    }
    return WalkerCount;
}

//This function intializes command packet, checks for null case, sets proper LWS sizes and return WalkerCount needed for this packet
inline void InitializeCommandPacket( __global IGIL_CommandHeader* pCommand )
{
    EMULATION_ENTER_FUNCTION( );

    //Check for NULL case
    if( pCommand->m_range.m_localWorkSize[ 0 ] == 0 )
    {
        //If null case detected use 16 x 1 x 1 lws
        if( pCommand->m_range.m_globalWorkSize[ 0 ] >= 16 )
        {
            pCommand->m_range.m_localWorkSize[ 0 ] = 16;
        }
        else
        {
            pCommand->m_range.m_localWorkSize[ 0 ] = pCommand->m_range.m_globalWorkSize[ 0 ];
        }

        pCommand->m_range.m_localWorkSize[ 1 ] = 1;
        pCommand->m_range.m_localWorkSize[ 2 ] = 1;
    }

}

//Patches the address for pipe  control
void PatchPipeControlProfilingAddres( __global uint* secondaryBatchBuffer, uint slBoffset, ulong address, uint pipeControlOffset )
{
    EMULATION_ENTER_FUNCTION( );

    //SlbOffset is expressed in bytes and for cmd it is needed to convert it to dwords
    uint PostSyncDwordOffset = ( slBoffset / DWORD_SIZE_IN_BYTES ) + pipeControlOffset + PIPE_CONTROL_POST_SYNC_DWORD;
    uint DwordOffset         = ( slBoffset / DWORD_SIZE_IN_BYTES ) + pipeControlOffset + PIPE_CONTROL_ADDRESS_FIELD_DWORD;
    //Patch P_C event timestamp address in SLB in 3rd and 4th dword
    secondaryBatchBuffer[ DwordOffset ] = 0;
    patchDword( &secondaryBatchBuffer[ DwordOffset ], PIPE_CONTROL_GRAPHICS_ADDRESS_START_BIT, PIPE_CONTROL_GRAPHICS_ADDRESS_END_BIT, ( uint )( address >> PIPE_CONTROL_GRAPHICS_ADDRESS_START_BIT ) );
    DwordOffset++;
    secondaryBatchBuffer[ DwordOffset ] = 0;
    patchDword( &secondaryBatchBuffer[ DwordOffset ], PIPE_CONTROL_GRAPHICS_ADDRESS_HIGH_START_BIT, PIPE_CONTROL_GRAPHICS_ADDRESS_HIGH_END_BIT, ( address >> 32 ) );

    //Patch Timestamp bit
    patchDword( &secondaryBatchBuffer[ PostSyncDwordOffset ], PIPE_CONTROL_POST_SYNC_START_BIT, PIPE_CONTROL_POST_SYNC_END_BIT, PIPE_CONTROL_GENERATE_TIME_STAMP );
}

void DisablePostSyncBitInPipeControl( __global uint* secondaryBatchBuffer, uint slBoffset, uint pipeControlOffset )
{
    //SlbOffset is expressed in bytes and for cmd it is needed to convert it to dwords
    uint PostSyncDwordOffset = ( slBoffset / DWORD_SIZE_IN_BYTES ) + pipeControlOffset + PIPE_CONTROL_POST_SYNC_DWORD;
    //Patch P_C event timestamp address in SLB in 3rd and 4th dword
    patchDword( &secondaryBatchBuffer[ PostSyncDwordOffset ], PIPE_CONTROL_POST_SYNC_START_BIT, PIPE_CONTROL_POST_SYNC_END_BIT, PIPE_CONTROL_NO_POSTSYNC_OPERATION );
}


int PatchDSH( __global IGIL_CommandQueue* pQueue,
              __global IGIL_KernelDataHeader * pKernelReflection,
              __global char* dsh,
              uint blockId,
              __global IGIL_CommandHeader* pCommandHeader,
              __global uint* secondaryBatchBuffer,
              uint dshOffset,
              uint intefaceDescriptorOffset,
              __local IGIL_WalkerEnumeration* pWalkerMain,
              uint walkerPos )
{
    EMULATION_ENTER_FUNCTION( );

    __global IGIL_KernelAddressData* pKernelAddressData = IGIL_GetKernelAddressData( pKernelReflection, blockId );
    __global IGIL_KernelData* pBlockData                = IGIL_GetKernelData( pKernelReflection, blockId );
    ulong PatchMask                                     = pBlockData->m_PatchTokensMask;
    uint CurrentIndex                                   = 0;

    __global char* pDsh                                 = ( __global char* )&dsh[ dshOffset ];
    __global IGIL_KernelCurbeParams* pKernelCurbeParams = ( __global IGIL_KernelCurbeParams* )&pBlockData->m_data;

    uint NumberOfDepencies                              = pCommandHeader->m_numDependencies;
    uint PatchOffset;
    __global char* pScalarData                          = ( __global char* )( &pCommandHeader->m_data[ NumberOfDepencies ] );
    __global char *pDshOnKRS                            = GetPtrToKernelReflectionOffset( pKernelAddressData->m_SamplerHeapOffset, pKernelReflection );

    uint SizeOfScalarsFromCurbe                         = 0;
    uint CurbeSize;

    uint TotalLocalSize;
    uint ThreadPayloadSize;
    uint NumberOfHWThreads;
    uint WorkDim;
    uint3 GlobalOffset;
    uint3 GlobalSizes;
    uint3 ActualLocalSize;

    GlobalOffset.x = ( uint )pCommandHeader->m_range.m_globalWorkOffset[ 0 ];
    GlobalOffset.y = ( uint )pCommandHeader->m_range.m_globalWorkOffset[ 1 ];
    GlobalOffset.z = ( uint )pCommandHeader->m_range.m_globalWorkOffset[ 2 ];

    GlobalSizes.x = ( uint )pCommandHeader->m_range.m_globalWorkSize[ 0 ];
    GlobalSizes.y = ( uint )pCommandHeader->m_range.m_globalWorkSize[ 1 ];
    GlobalSizes.z = ( uint )pCommandHeader->m_range.m_globalWorkSize[ 2 ];

    ActualLocalSize.x = pWalkerMain->WalkerArray[ walkerPos ].ActualLocalSize.x;
    ActualLocalSize.y = pWalkerMain->WalkerArray[ walkerPos ].ActualLocalSize.y;
    ActualLocalSize.z = pWalkerMain->WalkerArray[ walkerPos ].ActualLocalSize.z;

    WorkDim = pCommandHeader->m_range.m_dispatchDimensions;
    TotalLocalSize = ActualLocalSize.x * ActualLocalSize.y * ActualLocalSize.z;

    NumberOfHWThreads = TotalLocalSize / pBlockData->m_SIMDSize;
    if( TotalLocalSize % pBlockData->m_SIMDSize )
    {
        NumberOfHWThreads++;
    }

    ThreadPayloadSize = NumberOfHWThreads * 3 * GRF_SIZE;
    //Copy constant buffer to designated area on DSH.

    //pDshOnKRS seems to be in the global address space, not the private address space
    //Copy SamplerState and Constant Buffer at once
    IGILLOCAL_MEMCPY_GTOG( pDsh, pDshOnKRS, pBlockData->m_sizeOfConstantBuffer + pBlockData->m_SizeOfSamplerHeap );

    if( PatchMask & SCHEDULER_DATA_PARAMETER_KERNEL_ARGUMENT_MASK )
    {
        while( pKernelCurbeParams[ CurrentIndex ].m_parameterType == SCHEDULER_DATA_PARAMETER_KERNEL_ARGUMENT )
        {
            CurbeSize               = pKernelCurbeParams[ CurrentIndex ].m_parameterSize;
            SizeOfScalarsFromCurbe += CurbeSize;
            PatchOffset             = pKernelCurbeParams[ CurrentIndex ].m_patchOffset;

            //pScalarData is in the global address space, not the private address space
            IGILLOCAL_MEMCPY_GTOG( &pDsh[ PatchOffset ], pScalarData, CurbeSize );
            pScalarData            += CurbeSize;
            CurrentIndex++;
        }
#if SCHEDULER_DEBUG_MODE
        if( pCommandHeader->m_sizeOfScalarArguments != SizeOfScalarsFromCurbe )
        {
            pCommandHeader->m_commandState = SCHEDULER_CURBE_ARGUMENTS_SIZE_MISMATCH;
            return -1;
        }
#endif
    }
    
    if( PatchMask & SCHEDULER_DATA_PARAMETER_LOCAL_WORK_SIZE_MASK )
    {
        CurrentIndex     = PatchLocalWorkSizes( CurrentIndex,
                                                SCHEDULER_DATA_PARAMETER_LOCAL_WORK_SIZE,
                                                pKernelCurbeParams,
                                                pDsh,
                                                pWalkerMain->LocalWorkSize.x,
                                                pWalkerMain->LocalWorkSize.y,
                                                pWalkerMain->LocalWorkSize.z,
                                                ActualLocalSize.x,
                                                ActualLocalSize.y,
                                                ActualLocalSize.z );
#if SCHEDULER_DEBUG_MODE
        if( ( CurrentIndex == -1 ) || ( CurrentIndex >= pBlockData->m_numberOfCurbeParams ) )
        {
            pCommandHeader->m_commandState = SCHEDULER_CURBE_TOKEN_MISSED;
            return -1;
        }
#endif
    }
    
    if( PatchMask & SCHEDULER_DATA_PARAMETER_GLOBAL_WORK_SIZE_MASK )
    {
        CurrentIndex     = PatchDSH6Tokens( CurrentIndex,
                                            SCHEDULER_DATA_PARAMETER_GLOBAL_WORK_SIZE, 
                                            pKernelCurbeParams,
                                            pDsh,
                                            GlobalSizes.x,
                                            GlobalSizes.y,
                                            GlobalSizes.z );
#if SCHEDULER_DEBUG_MODE
        if( ( CurrentIndex == -1 ) || ( CurrentIndex >= pBlockData->m_numberOfCurbeParams ) )
        {
            pCommandHeader->m_commandState = SCHEDULER_CURBE_TOKEN_MISSED;
            return -1;
        }
#endif
    }
    if( PatchMask & SCHEDULER_DATA_PARAMETER_NUM_WORK_GROUPS_MASK )
    {
        CurrentIndex     = PatchDSH6Tokens( CurrentIndex,
                                            SCHEDULER_DATA_PARAMETER_NUM_WORK_GROUPS,
                                            pKernelCurbeParams,
                                            pDsh,
                                            pWalkerMain->TotalDimSize.x,
                                            pWalkerMain->TotalDimSize.y,
                                            pWalkerMain->TotalDimSize.z );
#if SCHEDULER_DEBUG_MODE
        if( ( CurrentIndex == -1 ) || ( CurrentIndex >= pBlockData->m_numberOfCurbeParams ) )
        {
            pCommandHeader->m_commandState = SCHEDULER_CURBE_TOKEN_MISSED;
            return -1;
        }
#endif
    }
    if( PatchMask & SCHEDULER_DATA_PARAMETER_WORK_DIMENSIONS_MASK )
    {
        CurrentIndex     = PatchDSH1Token( CurrentIndex,
                                           SCHEDULER_DATA_PARAMETER_WORK_DIMENSIONS,
                                           pKernelCurbeParams,
                                           pDsh,
                                           WorkDim );
#if SCHEDULER_DEBUG_MODE
        if( ( CurrentIndex == -1 ) || ( CurrentIndex >= pBlockData->m_numberOfCurbeParams ) )
        {
            pCommandHeader->m_commandState = SCHEDULER_CURBE_TOKEN_MISSED;
            return -1;
        }
#endif
    }

    if( PatchMask & SCHEDULER_DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES_MASK )
    {
        CurrentIndex     = PatchLocalMemEntities( CurrentIndex,
                                                  SCHEDULER_DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES,
                                                  pKernelCurbeParams,
                                                  pDsh,
                                                  pCommandHeader );
#if SCHEDULER_DEBUG_MODE
        if( ( CurrentIndex == -1 ) || ( CurrentIndex >= pBlockData->m_numberOfCurbeParams ) )
        {
            pCommandHeader->m_commandState = SCHEDULER_CURBE_TOKEN_MISSED;
            return -1;
        }
#endif
    }

    if( PatchMask & SCHEDULER_DATA_PARAMETER_GLOBAL_WORK_OFFSET_MASK )
    {
        CurrentIndex     = PatchDSH6Tokens( CurrentIndex,
                                            SCHEDULER_DATA_PARAMETER_GLOBAL_WORK_OFFSET,
                                            pKernelCurbeParams,
                                            pDsh,
                                            GlobalOffset.x,
                                            GlobalOffset.y,
                                            GlobalOffset.z );
#if SCHEDULER_DEBUG_MODE
        if( ( CurrentIndex == -1 ) || ( CurrentIndex >= pBlockData->m_numberOfCurbeParams ) )
        {
            pCommandHeader->m_commandState = SCHEDULER_CURBE_TOKEN_MISSED;
            return -1;
        }
#endif
    }

    if( PatchMask & SCHEDULER_DATA_PARAMETER_NUM_HARDWARE_THREADS_MASK )
    {
        CurrentIndex     = PatchDSH1Token( CurrentIndex,
                                           SCHEDULER_DATA_PARAMETER_NUM_HARDWARE_THREADS,
                                           pKernelCurbeParams,
                                           pDsh,
                                           NumberOfHWThreads );
#if SCHEDULER_DEBUG_MODE
        if( ( CurrentIndex == -1 ) || ( CurrentIndex >= pBlockData->m_numberOfCurbeParams ) )
        {
            pCommandHeader->m_commandState = SCHEDULER_CURBE_TOKEN_MISSED;
            return -1;
        }
#endif
    }

    if( PatchMask & SCHEDULER_DATA_PARAMETER_PARENT_EVENT_MASK )
    {
        CurrentIndex     = PatchDSH1Token( CurrentIndex,
                                           SCHEDULER_DATA_PARAMETER_PARENT_EVENT,
                                           pKernelCurbeParams,
                                           pDsh,
                                           pCommandHeader->m_event );
#if SCHEDULER_DEBUG_MODE
        if( ( CurrentIndex == -1 ) || ( CurrentIndex >= pBlockData->m_numberOfCurbeParams ) )
        {
            pCommandHeader->m_commandState = SCHEDULER_CURBE_TOKEN_MISSED;
            return -1;
        }
#endif
    }
    
    if( PatchMask & SCHEDULER_DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE_MASK )
    {
        CurrentIndex     = PatchDSH6Tokens( CurrentIndex,
                                            SCHEDULER_DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE,
                                            pKernelCurbeParams,
                                            pDsh,
                                            pWalkerMain->LocalWorkSize.x,
                                            pWalkerMain->LocalWorkSize.y,
                                            pWalkerMain->LocalWorkSize.z );
#if SCHEDULER_DEBUG_MODE
        if( ( CurrentIndex == -1 ) || ( CurrentIndex >= pBlockData->m_numberOfCurbeParams ) )
        {
            pCommandHeader->m_commandState = SCHEDULER_CURBE_TOKEN_MISSED;
            return -1;
        }
#endif
    }
    if( PatchMask & SCHEDULER_DATA_PARAMETER_GLOBAL_POINTER )
    {
        if( pCommandHeader->m_numGlobalCapturedBuffer > 0 )
        {
            //Handle global pointers patching in stateless mode, info about layout in declaration of IGIL_CommandHeader
            __global    uint*  pGlobalIndexes = ( __global uint* ) ( &pCommandHeader->m_data[ NumberOfDepencies + pCommandHeader->m_numScalarArguments ] );
            __global    uint*  pGlobalPtrs    = ( __global uint* ) ( &pCommandHeader->m_data[ NumberOfDepencies + pCommandHeader->m_numScalarArguments + pCommandHeader->m_numGlobalCapturedBuffer ] );
            uint        StartIndex            = CurrentIndex;

            //Argument in command header are not in correct sequence, that's why proper key needs to be located
            for( uint glIdx = 0 ; glIdx < pCommandHeader->m_numGlobalCapturedBuffer; glIdx++)
            {
                //Reset CurrentIndex as we need to start from the beginning.
                CurrentIndex  = StartIndex;
                while( pKernelCurbeParams[ CurrentIndex ].m_parameterType == COMPILER_DATA_PARAMETER_GLOBAL_SURFACE )
                {
                    //Patch only if exact match occurs
                    if( pKernelCurbeParams[ CurrentIndex ].m_sourceOffset == *pGlobalIndexes )
                    {
                        PatchOffset             = pKernelCurbeParams[ CurrentIndex ].m_patchOffset;
                        //64 bit patching
                        if( pKernelCurbeParams[ CurrentIndex ].m_parameterSize == 8 )
                        {
                            __global uint* pDst = (__global uint *) &pDsh[PatchOffset];
                            pDst[ 0 ] = pGlobalPtrs[ 0 ];
                            pDst[ 1 ] = pGlobalPtrs[ 1 ];
                            pGlobalPtrs++;
                        }
                        else
                        {
                            __global uint* pDst = ( __global uint* ) &pDsh[ PatchOffset ];
                            *pDst               = ( uint ) *pGlobalPtrs;
                        }
                    }
                    CurrentIndex++;
                }
                pGlobalPtrs++;
                pGlobalIndexes++;
            }
        }
    }

    //Now generate local IDS
    if( pBlockData->m_SIMDSize == 8 )
    {
        generateLocalIDSsimd8( &pDsh[ pBlockData->m_sizeOfConstantBuffer ], ActualLocalSize, NumberOfHWThreads );
    }
    else
    {
        generateLocalIDSsimd16( &pDsh[ pBlockData->m_sizeOfConstantBuffer ], ActualLocalSize, NumberOfHWThreads );
    }

    uint TotalSLMSize = pCommandHeader->m_totalLocalSize + pBlockData->m_InilineSLMSize;

    //Update Interface Descriptor Data with SLM size  / number of HW threads.
    CopyAndPatchIDData( dsh, blockId, NumberOfHWThreads, TotalSLMSize, intefaceDescriptorOffset, pQueue->m_controls.m_StartBlockID );

    //Add WalkerStartSize
    patchGpGpuWalker( pQueue->m_controls.m_SecondLevelBatchOffset, secondaryBatchBuffer, intefaceDescriptorOffset, pBlockData->m_SIMDSize,
                      TotalLocalSize, pWalkerMain->WalkerArray[ walkerPos ].WalkerDimSize, pWalkerMain->WalkerArray[ walkerPos ].WalkerStartPoint,
                      NumberOfHWThreads, pBlockData->m_sizeOfConstantBuffer + ThreadPayloadSize, dshOffset );

    PatchMediaStateFlush( pQueue->m_controls.m_SecondLevelBatchOffset, secondaryBatchBuffer, intefaceDescriptorOffset, SCHEDULER_MSF_INITIAL );
    PatchMediaStateFlush( pQueue->m_controls.m_SecondLevelBatchOffset, secondaryBatchBuffer, intefaceDescriptorOffset, SCHEDULER_MSF_SECOND );

    return 0;
}

//Returns: isSRGB(ChannelOrder) ? ChannelOrder : 0;
inline uint GetSRGBChannelOrder( uint channelOrder )
{
    const uint AsSrgb = channelOrder - CL_sRGB;
    const uint NumSrgbFormats = CL_sBGRA - CL_sRGB;
    if( AsSrgb < NumSrgbFormats )
      return channelOrder;
    else
      return 0;
}

void PatchDSHParallelWithDynamicDSH20( uint slbOffsetBase,
                                      uint dshOffsetBase,
                                      uint intefaceDescriptorOffsetBase,
                                      __global IGIL_KernelDataHeader * pKernelReflection,
                                      __global char* dsh,
                                      uint blockId,
                                      __global IGIL_CommandHeader* pCommandHeader,
                                      __global uint* secondaryBatchBuffer,
                                      __global IGIL_CommandQueue* pQueue,
                                      __global IGIL_EventPool* eventsPool,
                                      __global char* ssh,
                                      uint btOffset,
                                      __local IGIL_WalkerEnumeration* pWalkerEnum,
                                      __local uint* objectIds
#ifdef ENABLE_DEBUG_BUFFER                                              
                                      , __global DebugDataBuffer* DebugQueue
#endif
                                      )
{
    EMULATION_ENTER_FUNCTION( );

    __global IGIL_KernelAddressData* pKernelAddressData = IGIL_GetKernelAddressData( pKernelReflection, blockId );
    __global IGIL_KernelData* pBlockData                = IGIL_GetKernelData( pKernelReflection, blockId );
    ulong PatchMask                                     = pBlockData->m_PatchTokensMask;
    uint CurrentIndex                                   = 0;

    __global IGIL_KernelCurbeParams* pKernelCurbeParams = ( __global IGIL_KernelCurbeParams* )&pBlockData->m_data;

    uint NumberOfDepencies                              = pCommandHeader->m_numDependencies;
    uint PatchOffset;

    uint CurbeSize;

    //Get pointer to the Sampler State
    __global char *pDshOnKRS                            = GetPtrToKernelReflectionOffset( pKernelAddressData->m_SamplerHeapOffset, pKernelReflection );

    uint WalkerCount                                    = GetWalkerCount( pCommandHeader );
    __global char *pKernelReflectionChar                = ( __global char * ) pKernelReflection;
    __global IGIL_KernelCurbeParams* pSSHdata           = ( __global  IGIL_KernelCurbeParams* )&pKernelReflectionChar[ pKernelAddressData->m_SSHTokensOffset ];

    //WALKER variables that will be propagated to SLB
    uint3 LocalSizes;
    uint3 GlobalSizes;
    uint3 GlobalOffset;
    uint3 EdgeArray;
    uint3 XYZDim;

    //X is always there
    GlobalOffset.x  = ( uint )pCommandHeader->m_range.m_globalWorkOffset[ 0 ];
    GlobalSizes.x   = ( uint )pCommandHeader->m_range.m_globalWorkSize[ 0 ];
    LocalSizes.x    = ( uint )pCommandHeader->m_range.m_localWorkSize[ 0 ];
    EdgeArray.x     = GlobalSizes.x % LocalSizes.x;
    uint WorkDim    = pCommandHeader->m_range.m_dispatchDimensions;
    XYZDim.x        = GlobalSizes.x / LocalSizes.x;

    if( WorkDim > 1 )
    {
        GlobalOffset.y  = ( uint )pCommandHeader->m_range.m_globalWorkOffset[ 1 ];
        GlobalSizes.y   = ( uint )pCommandHeader->m_range.m_globalWorkSize[ 1 ];
        LocalSizes.y    = ( uint )pCommandHeader->m_range.m_localWorkSize[ 1 ];
        EdgeArray.y     = GlobalSizes.y % LocalSizes.y;
        XYZDim.y        = GlobalSizes.y / LocalSizes.y;
        
        if( WorkDim > 2 )
        {
            GlobalOffset.z  = ( uint )pCommandHeader->m_range.m_globalWorkOffset[ 2 ];
            GlobalSizes.z   = ( uint )pCommandHeader->m_range.m_globalWorkSize[ 2 ];
            LocalSizes.z    = ( uint )pCommandHeader->m_range.m_localWorkSize[ 2 ];
            XYZDim.z        = GlobalSizes.z / LocalSizes.z;
            EdgeArray.z     = GlobalSizes.z % LocalSizes.z;
        }
        else
        {
            GlobalOffset.z  = 0;
            GlobalSizes.z   = 1;
            LocalSizes.z    = 1;
            EdgeArray.z     = 0;
            XYZDim.z        = 1;
        }
    }
    else
    {
        GlobalOffset.y  = 0;
        GlobalOffset.z  = 0;
        GlobalSizes.y   = 1;
        GlobalSizes.z   = 1;
        LocalSizes.y    = 1;
        LocalSizes.z    = 1;
        EdgeArray.z     = 0;
        EdgeArray.y     = 0;
        XYZDim.z        = 1;
        XYZDim.y        = 1;
    }

    if( get_local_id( 0 ) == 0 )
    {
        InitWalkerDataParallel( pWalkerEnum, WorkDim, &WalkerCount, EdgeArray, XYZDim, GlobalSizes, LocalSizes );
    }
   
    uint SLBOffset                  = slbOffsetBase;
    uint DshOffset                  = dshOffsetBase;
    uint IntefaceDescriptorOffset   = intefaceDescriptorOffsetBase;
    __global uint* pArgumentIds     = NULL;
    __global uint* pObjectIds       = NULL;
    __global char* pLocalIdsOnDSH   = NULL;

    uint SamplerHeapSize = pBlockData->m_SizeOfSamplerHeap;

    //Object ID is in fact surface state offset for parent in case of surfaces using SSH, copy SSH from parent to child.
    //Copy binding table state of this kernel to allocated place on ssh
    GLOBAL_MEMCPY( &ssh[ btOffset ], &ssh[ pKernelAddressData->m_BTSoffset ] , pKernelAddressData->m_BTSize );

    for( uint WalkerID = 0; WalkerID < WalkerCount; WalkerID++ )
    {
        //Update the offsets
        if( WalkerID > 0 )
        {
            SLBOffset += SECOND_LEVEL_BUFFER_SPACE_FOR_EACH_ENQUEUE;
            SLBOffset %= MAX_SLB_OFFSET;
            IntefaceDescriptorOffset++;
            DshOffset += MAX_DSH_SIZE_PER_ENQUEUE;
        }

        __global char* pDsh = ( __global char* )&dsh[ DshOffset ];
        pLocalIdsOnDSH      = &pDsh[ pBlockData->m_sizeOfConstantBuffer + SamplerHeapSize ];

        //Copy Sampler State  and constant buffer on all threads
        GLOBAL_MEMCPY( pDsh, pDshOnKRS, pBlockData->m_sizeOfConstantBuffer + SamplerHeapSize );

        barrier( CLK_GLOBAL_MEM_FENCE );

        //Update BorderColorPointer on all threads
        if( pBlockData->m_numberOfSamplerStates )
        {
            uint SamplerId = get_local_id( 0 );
            __global uint* pSamplerState;
            while( SamplerId < pBlockData->m_numberOfSamplerStates )
            {
                pSamplerState = ( __global uint* )&dsh[ DshOffset + pBlockData->m_SamplerStateArrayOffsetOnDSH + SamplerId * OCLRT_SIZEOF_SAMPLER_STATE ];
                uint PatchValue = DshOffset >> 5;
                patchDword( &pSamplerState[ SAMPLER_STATE_DESCRIPTOR_BORDER_COLOR_POINTER_DWORD ], 5, 31, PatchValue );
                SamplerId += PARALLEL_SCHEDULER_COMPILATION_SIZE_20 * PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP20;
            }
        }

        //Setup SSH if needed, do it only for first Walker as all Walkers will re-use the same binding table layout.
        if( ( pCommandHeader->m_numGlobalArguments > 0 ) & ( WalkerID == 0 ) )
        {
            //Global arguments are after scalars, global pointers slm sizes and events
            uint offset                 = pCommandHeader->m_numDependencies + pCommandHeader->m_numScalarArguments + pCommandHeader->m_numOfLocalPtrSizes + ( pCommandHeader->m_numGlobalCapturedBuffer * ( sizeof( ulong ) +  sizeof( uint ) ) / sizeof( uint ) );

            pArgumentIds = &pCommandHeader->m_data[ offset ];
            //Object IDS are located after Argument IDs
            pObjectIds   = &pCommandHeader->m_data[ offset + pCommandHeader->m_numGlobalArguments ];

            //Setup local memory for fast access for Curbe patching
            uint ArgId = get_local_id( 0 );

            //Only third group Updates ObjectIDS, this will be synchronized with condition below
            if( ( ArgId >> HW_GROUP_ID_SHIFT( PARALLEL_SCHEDULER_COMPILATION_SIZE_20 ) ) == 2 )
            { 
                ArgId = ArgId - ( PARALLEL_SCHEDULER_COMPILATION_SIZE_20 << 1 );
                while( ArgId < pCommandHeader->m_numGlobalArguments )
                {
                    objectIds[ pArgumentIds[ ArgId ] ] = pObjectIds[ ArgId ];
                    ArgId += PARALLEL_SCHEDULER_COMPILATION_SIZE_20;
                }
            }
#ifdef SCHEDULER_EMULATION
            //Synchronization needed for Emulation, ObjectIDS needs to be set by whole HW group, on GPU there is implicit synchronization in HW group
            barrier( CLK_GLOBAL_MEM_FENCE );
#endif
            if( get_local_id( 0 ) == PARALLEL_SCHEDULER_COMPILATION_SIZE_20 * 2 )
            {
                __global uint* pBindingTable = ( __global uint* ) &ssh[ btOffset ];

                
                //To properly set up binding table point to parents surface state heap
                for( uint ArgumentID = 0 ; ArgumentID < pCommandHeader->m_numGlobalArguments; ArgumentID++ )
                {
                    uint ArgId      = pArgumentIds[ ArgumentID ];
                    
                    //Locate proper Arg ID
                    //Get ssh offset, lookup table already provided
                    if( objectIds[ ArgId ] < MAX_SSH_PER_KERNEL_SIZE )
                    {
                        if( pSSHdata[ ArgId ].m_sourceOffset == ArgId )
                        {
                            pBindingTable[ pSSHdata[ ArgId ].m_patchOffset ] = objectIds[ ArgId ];
                        }
                        else
                        {
                            pQueue->m_controls.m_ErrorCode += 10;
                            uint CurrentArg = 0;
                            while( CurrentArg < pKernelAddressData->m_BTSize / 4 )
                            {
                                if( pSSHdata[ CurrentArg ].m_sourceOffset == ArgId )
                                {
                                    pBindingTable[ pSSHdata[ CurrentArg ].m_patchOffset ] = objectIds[ ArgId ];
                                    break;
                                }
                                CurrentArg++;
                            }
                        }
                    }
                }
            }
        }

        if( ( PatchMask & SCHEDULER_DATA_PARAMETER_SAMPLER_MASK ) )
        {
            if( get_local_id( 0 ) == 2 * PARALLEL_SCHEDULER_COMPILATION_SIZE_20 )
            {
                for( uint ArgumentID = 0; ArgumentID < pCommandHeader->m_numGlobalArguments; ArgumentID++ )
                {
                    uint ArgId      = pArgumentIds[ ArgumentID ];
                    if( ( objectIds[ ArgId ] >= MAX_SSH_PER_KERNEL_SIZE ) )
                    {
                        uint SamplerCount = 0;
                        //Get pointer to Parent's samplers ( arguments ) data stored on KRS
                        __global IGIL_SamplerParams* pSamplerParamsOnKRS    = ( __global IGIL_SamplerParams* )GetPtrToKernelReflectionOffset( pKernelAddressData->m_SamplerParamsOffset, pKernelReflection );

                        //Iterate through all samplers passed from parent and copy state to proper SSA offset
                        while( pKernelReflection->m_ParentSamplerCount > SamplerCount )
                        {
                            //Get offset in parent's SSA from ObjectID, offset to beginning of SSA is included ( before SSA is BorderColorPointer ) so this is relative to parent's DSH heap
                            PatchOffset = objectIds[ ArgId ] - MAX_SSH_PER_KERNEL_SIZE;

                            if( pSamplerParamsOnKRS->m_ArgID == ArgId )
                            {
                                IGILLOCAL_MEMCPY_GTOG( &pDsh[ pSamplerParamsOnKRS->m_SamplerStateOffset ], &dsh[ pQueue->m_controls.m_ParentDSHOffset + PatchOffset ], OCLRT_SIZEOF_SAMPLER_STATE );
                                break;
                            }
                            pSamplerParamsOnKRS = pSamplerParamsOnKRS + 1;
                            SamplerCount        = SamplerCount + 1;
                        }
                    }
                }
            }
        }
        __global    char*   pScalarData                     = ( __global char* ) ( &pCommandHeader->m_data[ NumberOfDepencies ] );

        CurrentIndex = 0;
        uint TotalLocalSize = pWalkerEnum->WalkerArray[ WalkerID ].ActualLocalSize.x *
                              pWalkerEnum->WalkerArray[ WalkerID ].ActualLocalSize.y *
                              pWalkerEnum->WalkerArray[ WalkerID ].ActualLocalSize.z;

        uint NumberOfHWThreads = TotalLocalSize / pBlockData->m_SIMDSize;

        if( TotalLocalSize % pBlockData->m_SIMDSize != 0 )
        {
            NumberOfHWThreads++;
        }

        uint ThreadPayloadSize = NumberOfHWThreads * pBlockData->m_PayloadSize;

        //Move pointer to Constant Buffer Offset
        pDsh =  ( __global char* )&dsh[ DshOffset + SamplerHeapSize ];

        if( ( get_local_id( 0 ) >= PARALLEL_SCHEDULER_COMPILATION_SIZE_20 ) & ( get_local_id( 0 ) < PARALLEL_SCHEDULER_COMPILATION_SIZE_20 + 6 ) )
        {
            if( PatchMask & SCHEDULER_DATA_PARAMETER_KERNEL_ARGUMENT_MASK )
            {
                while( pKernelCurbeParams[ CurrentIndex ].m_parameterType == SCHEDULER_DATA_PARAMETER_KERNEL_ARGUMENT )
                {
                    CurbeSize               = pKernelCurbeParams[ CurrentIndex ].m_parameterSize;
                    PatchOffset             = pKernelCurbeParams[ CurrentIndex ].m_patchOffset;
                    IGILLOCAL_MEMCPY_GTOG( &pDsh[ PatchOffset ], pScalarData, CurbeSize );
                    pScalarData             += CurbeSize;
                    CurrentIndex++;
                }
            }
            if( PatchMask & SCHEDULER_DATA_PARAMETER_LOCAL_WORK_SIZE_MASK )
            {
                CurrentIndex     = PatchLocalWorkSizesParallel( CurrentIndex,
                                                                SCHEDULER_DATA_PARAMETER_LOCAL_WORK_SIZE,
                                                                pKernelCurbeParams,
                                                                pDsh,
                                                                LocalSizes.x,
                                                                LocalSizes.y,
                                                                LocalSizes.z,
                                                                pWalkerEnum->WalkerArray[ WalkerID ].ActualLocalSize.x,
                                                                pWalkerEnum->WalkerArray[ WalkerID ].ActualLocalSize.y,
                                                                pWalkerEnum->WalkerArray[ WalkerID ].ActualLocalSize.z );
            }
            if( PatchMask & SCHEDULER_DATA_PARAMETER_GLOBAL_WORK_SIZE_MASK )
            {
                CurrentIndex     = PatchDSH6TokensParallel20( CurrentIndex,
                                                              SCHEDULER_DATA_PARAMETER_GLOBAL_WORK_SIZE,
                                                              pKernelCurbeParams,
                                                              pDsh,
                                                              GlobalSizes.x,
                                                              GlobalSizes.y,
                                                              GlobalSizes.z );
            }
            if( PatchMask & SCHEDULER_DATA_PARAMETER_NUM_WORK_GROUPS_MASK )
            {
                CurrentIndex     = PatchDSH6TokensParallel20( CurrentIndex,
                                                              SCHEDULER_DATA_PARAMETER_NUM_WORK_GROUPS,
                                                              pKernelCurbeParams,
                                                              pDsh,
                                                              pWalkerEnum->TotalDimSize.x,
                                                              pWalkerEnum->TotalDimSize.y,
                                                              pWalkerEnum->TotalDimSize.z );
            }
            if( PatchMask & SCHEDULER_DATA_PARAMETER_WORK_DIMENSIONS_MASK )
            {
                CurrentIndex     = PatchDSH1TokenParallel20( CurrentIndex,
                                                             SCHEDULER_DATA_PARAMETER_WORK_DIMENSIONS,
                                                             pKernelCurbeParams,
                                                             pDsh,
                                                             WorkDim );
            }
            if( PatchMask & SCHEDULER_DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES_MASK )
            {
                CurrentIndex     = PatchLocalMemEntities( CurrentIndex,
                                                          SCHEDULER_DATA_PARAMETER_SUM_OF_LOCAL_MEMORY_OBJECT_ARGUMENT_SIZES,
                                                          pKernelCurbeParams,
                                                          pDsh,
                                                          pCommandHeader );
            }
            if( PatchMask & SCHEDULER_DATA_PARAMETER_GLOBAL_WORK_OFFSET_MASK )
            {
                CurrentIndex     = PatchDSH6TokensParallel20( CurrentIndex,
                                                              SCHEDULER_DATA_PARAMETER_GLOBAL_WORK_OFFSET,
                                                              pKernelCurbeParams,
                                                              pDsh,
                                                              GlobalOffset.x,
                                                              GlobalOffset.y,
                                                              GlobalOffset.z );
            }
            if( PatchMask & SCHEDULER_DATA_PARAMETER_NUM_HARDWARE_THREADS_MASK )
            {
                CurrentIndex     = PatchDSH1TokenParallel20( CurrentIndex,
                                                             SCHEDULER_DATA_PARAMETER_NUM_HARDWARE_THREADS,
                                                             pKernelCurbeParams,
                                                             pDsh,
                                                             NumberOfHWThreads );
            }
            if( PatchMask & SCHEDULER_DATA_PARAMETER_PARENT_EVENT_MASK )
            {
                CurrentIndex     = PatchDSH1TokenParallel20( CurrentIndex,
                                                             SCHEDULER_DATA_PARAMETER_PARENT_EVENT,
                                                             pKernelCurbeParams,
                                                             pDsh,
                                                             pCommandHeader->m_event );
            }
            if( PatchMask & SCHEDULER_DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE_MASK )
            {
                CurrentIndex     = PatchDSH6TokensParallel20( CurrentIndex,
                                                              SCHEDULER_DATA_PARAMETER_ENQUEUED_LOCAL_WORK_SIZE,
                                                              pKernelCurbeParams,
                                                              pDsh,
                                                              LocalSizes.x,
                                                              LocalSizes.y,
                                                              LocalSizes.z );
            }
            if( PatchMask & SCHEDULER_DATA_PARAMETER_GLOBAL_POINTER )
            {
                if( pCommandHeader->m_numGlobalCapturedBuffer > 0 )
                {
                    //Handle global pointers patching in stateless mode, info about layout in declaration of IGIL_CommandHeader
                    __global    uint*  pGlobalIndexes = ( __global uint* ) ( &pCommandHeader->m_data[ NumberOfDepencies + pCommandHeader->m_numScalarArguments ] );
                    __global    uint*  pGlobalPtrs    = ( __global uint* ) ( &pCommandHeader->m_data[ NumberOfDepencies + pCommandHeader->m_numScalarArguments + pCommandHeader->m_numGlobalCapturedBuffer ] );
                    uint        StartIndex            = CurrentIndex;

                    //Argument in command header are not in correct sequence, that's why proper key needs to be located
                    for( uint glIdx = 0 ; glIdx < pCommandHeader->m_numGlobalCapturedBuffer; glIdx++)
                    {
                        //Reset CurrentIndex as we need to start from the beginning.
                        CurrentIndex  = StartIndex;
                        while( pKernelCurbeParams[ CurrentIndex ].m_parameterType == COMPILER_DATA_PARAMETER_GLOBAL_SURFACE )
                        {
                            //Patch only if exact match occurs
                            if( pKernelCurbeParams[ CurrentIndex ].m_sourceOffset == *pGlobalIndexes )
                            {
                                PatchOffset             = pKernelCurbeParams[ CurrentIndex ].m_patchOffset;
                                //64 bit patching
                                if( pKernelCurbeParams[ CurrentIndex ].m_parameterSize == 8 )
                                {
                                    __global uint* pDst = (__global uint *) &pDsh[PatchOffset];
                                    pDst[0] = pGlobalPtrs[0];
                                    pDst[1] = pGlobalPtrs[1];
                                    pGlobalPtrs++;
                                }
                                else
                                {
                                    __global uint* pDst = ( __global uint* ) &pDsh[ PatchOffset ];
                                    *pDst               = ( uint ) *pGlobalPtrs;
                                }
                            }
                            CurrentIndex++;
                        }
                        pGlobalPtrs++;
                        pGlobalIndexes++;
                    }
                }
                while( pKernelCurbeParams[ CurrentIndex ].m_parameterType == COMPILER_DATA_PARAMETER_GLOBAL_SURFACE )
                {
                    CurrentIndex++;
                }
            }

            //Patch images curbe entries
            if( ( PatchMask & SCHEDULER_DATA_PARAMETER_IMAGE_CURBE_ENTRIES ) | ( PatchMask & SCHEDULER_DATA_PARAMETER_SAMPLER_MASK ) )
            {             
                if( ( pArgumentIds != NULL ) & ( pObjectIds != NULL ) )
                {
                    //pKernelReflectionChar is a global address pointer
                    __global IGIL_ImageParamters      *pImageParams           = ( __global IGIL_ImageParamters * ) &pKernelReflectionChar[ pKernelReflection->m_ParentImageDataOffset ];
                    __global IGIL_ParentSamplerParams *pParentSamplerParams   = ( __global IGIL_ParentSamplerParams* ) &pKernelReflectionChar[ pKernelReflection->m_ParentSamplerParamsOffset ];
                    //First obtain argument ID
                    uint WorkID = get_local_id( 0 ) - PARALLEL_SCHEDULER_COMPILATION_SIZE_20;
                    while( WorkID + CurrentIndex < pBlockData->m_numberOfCurbeTokens )
                    {
                        uint ArgId              = pKernelCurbeParams[ CurrentIndex + WorkID ].m_sourceOffset;
                        uint ObjectID           = objectIds[ ArgId ];
                        uint CurrentImage       = 0;
                        uint CurrentSampler     = 0;    

                        uint PatchValue         = 0;
                        uint TokenType          = pKernelCurbeParams[ CurrentIndex + WorkID ].m_parameterType;
                        uint PatchOffset        = pKernelCurbeParams[ CurrentIndex + WorkID ].m_patchOffset;
                        uint PatchValueInvalid  = 0;

                        //If Images
                        if( ObjectID < OCLRT_IMAGE_MAX_OBJECT_ID )
                        {
                            //Locate proper parent Image
                            while( ( pImageParams[ CurrentImage ].m_ObjectID != ObjectID ) & ( CurrentImage < pKernelReflection->m_ParentKernelImageCount ) )
                            {
                                CurrentImage++;
                            }
                            //Proper image is located under CurrentImage patch the token

                            if( TokenType == SCHEDULER_DATA_PARAMETER_IMAGE_WIDTH )
                            {
                                PatchValue  = pImageParams[ CurrentImage ].m_Width;
                            }
                            else if( TokenType == SCHEDULER_DATA_PARAMETER_IMAGE_HEIGHT )
                            {
                                PatchValue  = pImageParams[ CurrentImage ].m_Height;
                            }
                            else if( TokenType == SCHEDULER_DATA_PARAMETER_IMAGE_DEPTH )
                            {
                                PatchValue  = pImageParams[ CurrentImage ].m_Depth;
                            }
                            else if( TokenType == SCHEDULER_DATA_PARAMETER_IMAGE_CHANNEL_DATA_TYPE )
                            {
                                PatchValue  = pImageParams[ CurrentImage ].m_ChannelDataType;
                            }
                            else if( TokenType == SCHEDULER_DATA_PARAMETER_IMAGE_CHANNEL_ORDER )
                            {
                                PatchValue  = pImageParams[ CurrentImage ].m_ChannelOrder;
                            }
                            else if( TokenType == SCHEDULER_DATA_PARAMETER_IMAGE_SRGB_CHANNEL_ORDER )
                            {
                                PatchValue  = GetSRGBChannelOrder( pImageParams[ CurrentImage ].m_ChannelOrder );
                            }
                            else if( TokenType == SCHEDULER_DATA_PARAMETER_IMAGE_ARRAY_SIZE )
                            {
                                PatchValue  = pImageParams[ CurrentImage ].m_ArraySize;
                            }
                            else if( TokenType == SCHEDULER_DATA_PARAMETER_IMAGE_NUM_SAMPLES )
                            {
                                PatchValue  = pImageParams[ CurrentImage ].m_NumSamples;
                            }
                            else if( TokenType == SCHEDULER_DATA_PARAMETER_IMAGE_NUM_MIP_LEVELS )
                            {
                                PatchValue  = pImageParams[ CurrentImage ].m_NumMipLevels;
                            }
                            else if( TokenType == SCHEDULER_DATA_PARAMETER_IMAGE_OBJECT_ID )
                            {
                                PatchValue  = ObjectID;
                            }
                            else
                            {
                                PatchValueInvalid = 1;
                            }
                        }
                        //If Sampler
                        else if( ObjectID >= OCLRT_SAMPLER_MIN_OBJECT_ID )
                        {
                            //Mark PatchValue invalid if SamplerParams will not be found
                            PatchValueInvalid = 1;
                            //Locate proper parent Image
                            while( CurrentSampler < pKernelReflection->m_ParentSamplerCount )
                            {
                                if( pParentSamplerParams[ CurrentSampler ].m_ObjectID == ObjectID )
                                {
                                    PatchValueInvalid = 0;
                                    if( TokenType == DATA_PARAMETER_SAMPLER_ADDRESS_MODE )
                                    { 
                                        PatchValue = pParentSamplerParams[ CurrentSampler ].m_AddressingMode;
                                    }
                                    else if( TokenType == DATA_PARAMETER_SAMPLER_NORMALIZED_COORDS )
                                    { 
                                        PatchValue = pParentSamplerParams[ CurrentSampler ].NormalizedCoords;
                                    }
                                    else if( TokenType == DATA_PARAMETER_SAMPLER_COORDINATE_SNAP_WA_REQUIRED )
                                    {
                                        PatchValue = pParentSamplerParams[ CurrentSampler ].CoordinateSnapRequired;
                                    }
                                    else if( TokenType == SCHEDULER_DATA_PARAMETER_SAMPLER_OBJECT_ID )
                                    {
                                        PatchValue = ObjectID;
                                    }
                                    else
                                    {
                                        PatchValueInvalid = 1;
                                    }
                                    CurrentSampler = pKernelReflection->m_ParentSamplerCount;
                                }
                                CurrentSampler++;
                            }
                        }
                        else
                        {
                            PatchValueInvalid = 1;
                        }
                        
                        if( PatchValueInvalid == 0 )
                        {
                            *( __global uint * ) ( &pDsh[ PatchOffset ] ) = PatchValue;
                        }
                        CurrentIndex += 6;
                    }

                }
                else
                {
                    pQueue->m_controls.m_ErrorCode += 7;
                }
            }
        }
#ifdef SCHEDULER_EMULATION
        barrier( CLK_GLOBAL_MEM_FENCE );
#endif

        if( get_local_id( 0 ) == 0 )
        {
#if defined WA_LRI_COMMANDS_EXIST
            bool ShouldDisablePreemption = false;
#endif
            //Profiling support
            if( pQueue->m_controls.m_IsProfilingEnabled != 0 )
            {
                bool DisableTimeStampStart = true;
                bool DisableTimeStampEnd   = true;
                if( ( ( uint )pCommandHeader->m_event != IGIL_EVENT_INVALID_HANDLE ) & ( ( WalkerID == 0 ) | ( WalkerID == WalkerCount - 1 ) ) )
                {
                    //Event is propagated to childs as "parent event", to avoid overwriting the same start value, only generate timestamp write
                    //For the first command for this event, this means we look for event with no children ( so compare to 1 ).
                    clk_event_t EventID = __builtin_astype( ( void* ) ( ( ulong ) pCommandHeader->m_event ), clk_event_t );
                    __global IGIL_DeviceEvent *events = TEMP_IGIL_GetDeviceEvents( eventsPool );

                    if( events[ ( uint )(size_t)__builtin_astype( EventID, void* ) ].m_numChildren == 1 )
                    {
#if defined WA_LRI_COMMANDS_EXIST && defined WA_PROFILING_PREEMPTION
                        //This is a case, where profiling of block kernels occurs - presence of event in EM workload
                        //In such case, disable preemption around all WALKERs for that block kernel and event
                        ShouldDisablePreemption = true;
#endif
                        if( WalkerID == 0 )
                        {
                            //Emit pipecontrol with timestamp write
                            ulong Address = ( ulong )&( events[ ( uint )(size_t)__builtin_astype( EventID, void* ) ].m_profilingCmdStart );
                            //Timestamp start
                            PatchPipeControlProfilingAddres( secondaryBatchBuffer,
                                                             SLBOffset,
                                                             Address,
                                                             PIPE_CONTROL_FOR_TIMESTAMP_START_OFFSET );
                            DisableTimeStampStart = false;
                        }
                        if( WalkerID == WalkerCount - 1 )
                        {
                            ulong Address = ( ulong )&( events[ ( uint )(size_t)__builtin_astype( EventID, void* ) ].m_profilingCmdEnd );
                            //Timestamp end
                            PatchPipeControlProfilingAddres( secondaryBatchBuffer,
                                                             SLBOffset,
                                                             Address,
                                                             PIPE_CONTROL_FOR_TIMESTAMP_END_OFFSET );
                            DisableTimeStampEnd = false;
                        }
                    }
                }
                if( DisableTimeStampStart )
                {
                    DisablePostSyncBitInPipeControl( secondaryBatchBuffer,
                                                     SLBOffset,
                                                     PIPE_CONTROL_FOR_TIMESTAMP_START_OFFSET );
                }
                if( DisableTimeStampEnd )
                {
                    DisablePostSyncBitInPipeControl( secondaryBatchBuffer,
                                                     SLBOffset,
                                                     PIPE_CONTROL_FOR_TIMESTAMP_END_OFFSET );
                }
            }
            else
            {
                //Optimized path, in case block can be run concurently noop pipe control after such block.
                uint DwordOffset = SLBOffset / DWORD_SIZE_IN_BYTES;

                if( pBlockData->m_CanRunConcurently != 0 )
                {
                    NOOPCSStallPipeControl( secondaryBatchBuffer, DwordOffset, PIPE_CONTROL_FOR_TIMESTAMP_END_OFFSET );
                }
                else
                {
                    PutCSStallPipeControl( secondaryBatchBuffer, DwordOffset, PIPE_CONTROL_FOR_TIMESTAMP_END_OFFSET );
                }
            }

#if defined WA_LRI_COMMANDS_EXIST
            bool NoopPreemptionDisabling = true;
            bool NoopPreemptionEnabling = true;

#if defined WA_KERNEL_PREEMPTION
            //This is case, where block kernel should have disabled preemption because of its sampler usage around all WALKERs of that block kernel
            //Preemption should be disabled when EM event profiling is used OR kernel data indicate such behavior
            ShouldDisablePreemption |= ( pBlockData->m_DisablePreemption != 0 );
#endif

#if defined WA_PROFILING_PREEMPTION
            //m_EventTimestampAddress != NULL means profiling of the whole workload is enabled (preemption around whole chained BB is disabled)
            //So disabling preemption should be permitted only when workload profiling is off, in other cases noop all LRI commands
            //For m_EventTimestampAddress != NULL preemption is enabled before BB_END
            ShouldDisablePreemption &= ( pQueue->m_controls.m_EventTimestampAddress == 0 );
#endif

            if( ShouldDisablePreemption != false )
            {
                if( WalkerID == 0 )
                {
                    SetDisablePreemptionRegister( SLBOffset, secondaryBatchBuffer );
                    NoopPreemptionDisabling = false;
                }

                if( WalkerID == WalkerCount - 1 )
                {
                    SetEnablePreemptionRegister( SLBOffset, secondaryBatchBuffer );
                    NoopPreemptionEnabling = false;
                }
            }

            if( NoopPreemptionDisabling )
            {
                NoopPreemptionCommand( SLBOffset, IMM_LOAD_REGISTER_FOR_DISABLE_PREEMPTION_OFFSET, secondaryBatchBuffer );
            }

            if( NoopPreemptionEnabling )
            {
                NoopPreemptionCommand( SLBOffset, IMM_LOAD_REGISTER_FOR_ENABLE_PREEMPTION_OFFSET, secondaryBatchBuffer );
            }
#endif //WA_LRI_COMMANDS_EXIST
        }

        //Witems from 0 to 16 are responsible for local ids generation.
        if( ( get_local_id( 0 ) < 16 ) & ( pBlockData->m_NeedLocalIDS != 0 ) )
        {
            //Now generate local IDS
            generateLocalIDSParallel20( pLocalIdsOnDSH, pWalkerEnum->WalkerArray[ WalkerID ].ActualLocalSize, NumberOfHWThreads, pBlockData->m_SIMDSize );
        }
        //3rd HW thread will take care of patching media curbe load and GPPGU_WALKER command
        if( get_local_id( 0 ) == PARALLEL_SCHEDULER_COMPILATION_SIZE_20 * 2 )
        {

            uint TotalSLMSize = pCommandHeader->m_totalLocalSize + pBlockData->m_InilineSLMSize;
            //Update Interface Descriptor Data with SLM size  / number of HW threads.
            CopyAndPatchIDData20(dsh, blockId, NumberOfHWThreads, TotalSLMSize, IntefaceDescriptorOffset,
                                 pQueue->m_controls.m_StartBlockID, btOffset, DshOffset,
                                 pBlockData->m_numberOfSamplerStates
#ifdef ENABLE_DEBUG_BUFFER
                                 ,
                                 DebugQueue
#endif
            );

            patchGpGpuWalker( SLBOffset, secondaryBatchBuffer, IntefaceDescriptorOffset, pBlockData->m_SIMDSize,
                              TotalLocalSize, pWalkerEnum->WalkerArray[ WalkerID ].WalkerDimSize, pWalkerEnum->WalkerArray[ WalkerID ].WalkerStartPoint,
                              NumberOfHWThreads, pBlockData->m_sizeOfConstantBuffer + ThreadPayloadSize, SamplerHeapSize + DshOffset );

            PatchMediaStateFlush( SLBOffset, secondaryBatchBuffer, IntefaceDescriptorOffset, SCHEDULER_MSF_INITIAL );
            PatchMediaStateFlush( SLBOffset, secondaryBatchBuffer, IntefaceDescriptorOffset, SCHEDULER_MSF_SECOND );
        }
    }
}

uint CheckEventStatus( __global IGIL_CommandHeader* pCommand,
                       __global IGIL_EventPool* eventsPool )
{
    if( pCommand->m_numDependencies == 0 )
    {
        return 0;
    }
    else
    {
         __global IGIL_DeviceEvent* pDeviceEvent;
         //Events are stored at the begining of command packet dynamic payload
         for( uint i = 0; i < pCommand->m_numDependencies; i++ )
         {
             pDeviceEvent = TEMP_IGIL_GetDeviceEvent( eventsPool, pCommand->m_data[ i ] );
             if( pDeviceEvent->m_state != CL_COMPLETE )
             {
                 return 1;
             }
         }
    }
    return 0;
}

void DecreaseEventDependenciesParallel( __global IGIL_CommandHeader* pCommand,
                                        __global IGIL_EventPool* eventsPool )
{
     __global IGIL_DeviceEvent* pDeviceEvent;

     //Events are stored at the begining of command packet dynamic payload
     for( uint i = 0; i < pCommand->m_numDependencies; i++ )
     {
         pDeviceEvent = TEMP_IGIL_GetDeviceEvent( eventsPool, pCommand->m_data[ i ] );
         int OldDependants = atomic_dec( &pDeviceEvent->m_numDependents );

         if( ( pDeviceEvent->m_refCount <= 0 ) &
             ( ( OldDependants - 1 ) <= 0 ) &
             ( pDeviceEvent->m_numChildren <= 0 ) )
             {
                 TEMP_IGIL_FreeEvent( __builtin_astype( ( void* )( ( ulong )pCommand->m_data[ i ] ), clk_event_t ), eventsPool );
             }
    }
}

//Update status of the event and all events that are depending on this event
void UpdateEventsTreeStatusParallel( clk_event_t eventId, __global IGIL_EventPool* eventsPool, bool isProfilingEnabled )
{
    __global IGIL_DeviceEvent *events = TEMP_IGIL_GetDeviceEvents( eventsPool );
    __global IGIL_DeviceEvent *pEvent;
    do
    {
        pEvent = &events[ (uint) (size_t)__builtin_astype( eventId, void* ) ];

        int OldNumChild = atomic_dec( &pEvent->m_numChildren );
        if( ( OldNumChild - 1 ) <= 0 )
        {
            pEvent->m_state = CL_COMPLETE;

            if( ( pEvent->m_refCount <= 0 ) &
                ( pEvent->m_numDependents <= 0 ) &
                ( pEvent->m_numChildren <= 0 ) )
            {
                TEMP_IGIL_FreeEvent( eventId, eventsPool );
            }
            //This event transitions to CL_COMPLETE state, update it profiling informations.
            if( isProfilingEnabled != 0 )
            {
                //CL COMPLETE time is before this scheduler starts
                pEvent->m_profilingCmdComplete = eventsPool->m_CLcompleteTimestamp;

                //Check if this event has profiling pointer, if so update profiling data, all times should be there atm
                if( pEvent->m_pProfiling != 0 )
                {
                    __global ulong* retValues = ( __global ulong * )pEvent->m_pProfiling;

                    ulong StartTime                = pEvent->m_profilingCmdStart;
                    ulong EndTime                  = pEvent->m_profilingCmdEnd;
                    ulong CompleteTime             = pEvent->m_profilingCmdComplete;
                    ulong CLEndTransitionTime      = 0;
                    ulong CLCompleteTransitionTime = 0;
                    //Check if timer didn't reset by hitting max value
                    if( CompleteTime > StartTime )
                    {
                        CLEndTransitionTime      = EndTime - StartTime;
                        CLCompleteTransitionTime = CompleteTime - StartTime;
                    }
                    //If we hit this else it means that GPU timer reset to 0, compute proper delta
                    else
                    {
                        if( EndTime < StartTime ) 
                        {
                            CLEndTransitionTime = PROFILING_MAX_TIMER_VALUE - StartTime + EndTime;
                        }
                        else
                        {
                            CLEndTransitionTime = EndTime - StartTime;
                        }
                        CLCompleteTransitionTime = PROFILING_MAX_TIMER_VALUE - StartTime + CompleteTime;
                    }
                    //First value is END - START timestamp
                    retValues[ 0 ] = ( ulong )( ( float )CLEndTransitionTime * __intel__getProfilingTimerResolution() );
                    //Second value is COMPLETE - START timestamp
                    retValues[ 1 ] = ( ulong )( ( float )CLCompleteTransitionTime * __intel__getProfilingTimerResolution() );
                }
            }
            //Signal parent because we completed
            eventId = __builtin_astype( ( void* )( ( ulong )pEvent->m_parentEvent ), clk_event_t );
        }
    }
    while ( ( ( uint )(size_t)__builtin_astype( eventId, void* ) != IGIL_EVENT_INVALID_HANDLE ) & ( pEvent->m_numChildren <= 0 ) );
}

void GlobalBarrier( __global volatile uint* syncSurface )
{
    //Make sure each WKG item hit the barrier.
    barrier( CLK_GLOBAL_MEM_FENCE );

    //Now first thread of each wkg writes to designated place on SyncSurface
    if ( get_local_id( 0 ) == 0 )
    {
        syncSurface[ get_group_id( 0 ) ] = 1;
    }
    //Higher wkg ids tend to not have work to do in all cases, therefore I choose last wkg to wait for the others, as it is most likely it will hit this code sooner.
    if( get_group_id( 0 ) == ( get_num_groups( 0 ) - 1 ) )
    {
        //24 -48 case
        uint Value;
        do
        {
            Value = 1;
            for( uint i = get_local_id( 0 ); i < get_num_groups( 0 ); i += get_local_size( 0 ) )
            {
                Value = Value & syncSurface[ i ];
            }

        }
        while( Value == 0 );
        barrier( CLK_GLOBAL_MEM_FENCE );

        for( uint i = get_local_id( 0 ); i < get_num_groups( 0 ); i += get_local_size( 0 ) )
        {
            syncSurface[ i ] = 0;
        }
    }

    if( get_local_id( 0 ) == 0 )
    {
        while( syncSurface[ get_group_id( 0 ) ] != 0 );
    }
    barrier( CLK_GLOBAL_MEM_FENCE );
}

void GlobalBarrierUpdateQueue( __global volatile uint* syncSurface, __global IGIL_CommandQueue* pQueue )
{
    //Make sure each WKG item hit the barrier.
    barrier( CLK_GLOBAL_MEM_FENCE | CLK_LOCAL_MEM_FENCE);
    //Now first thread of each wkg writes to designated place on SyncSurface
    if ( get_local_id(0) == 0 )
    {
        syncSurface[get_group_id(0)] = 1;
    }
    //Higher wkg ids tend to not have work to do in all cases, therefore I choose last wkg to wait for the others, as it is most likely it will hit this code sooner.
    if( get_group_id(0) == ( get_num_groups( 0 ) - 1 ) )
    {
        uint Value;
        do
        {
            Value = 1;
            for( uint i = get_local_id( 0 ); i < get_num_groups( 0 ); i += get_local_size( 0 ) )
            {
                Value = Value & syncSurface[ i ];
            }
        }
        while( Value == 0 );
        barrier( CLK_GLOBAL_MEM_FENCE );
        pQueue->m_controls.m_IDTAfterFirstPhase = pQueue->m_controls.m_CurrentIDToffset;

        barrier( CLK_GLOBAL_MEM_FENCE );

        for( uint i = get_local_id( 0 ); i < get_num_groups( 0 ); i += get_local_size( 0 ) )
        {
            syncSurface[ i ] = 0;
        }
    }

    if( get_local_id(0) == 0 )
    {
        while( syncSurface[ get_group_id(0) ] != 0 );
    }
    barrier( CLK_GLOBAL_MEM_FENCE );
}


#ifdef SCHEDULER_EMULATION
__local int IDTOffset;
__local int DSHOffset;
__local int SLBOffset;
__local int StackOffset;
__local int QStorageOffset;
__local int MarkerOffset;
__local int BTSoffset;
__local IGIL_WalkerEnumeration WalkerEnum;
__local uint ObjectIDS[ MAX_GLOBAL_ARGS ];
#endif

#define WA_INT_DESC_MAX 62
__kernel __attribute__((intel_reqd_sub_group_size(PARALLEL_SCHEDULER_COMPILATION_SIZE_20)))
void SchedulerParallel20(
    __global IGIL_CommandQueue* pQueue,
    __global uint* commandsStack,
    __global IGIL_EventPool*    eventsPool,
    __global uint* secondaryBatchBuffer,        //SLB that will be used to put commands in.
    __global char* dsh,                         //Pointer to the start of Dynamic State Heap
    __global IGIL_KernelDataHeader* kernelData, //This is kernel reflection surface
    __global volatile uint* queueStorageBuffer,
    __global char* ssh,                         //Pointer to Surface state heap with BT and SS
    __global DebugDataBuffer* debugQueue )
{
    EMULATION_ENTER_FUNCTION( );
#ifdef WA_DISABLE_SCHEDULERS
    return;
#endif

#ifdef DEBUG
    //Early return enabled when m_SchedulerEarlyReturn is > 0,
    if( pQueue->m_controls.m_SchedulerEarlyReturn > 0 )
    {
        if( pQueue->m_controls.m_SchedulerEarlyReturn == 1 )
        {
            return;
        }

        if( get_global_id( 0 ) == 0 )
        {
            pQueue->m_controls.m_SchedulerEarlyReturnCounter++;
        }
        GlobalBarrier( queueStorageBuffer );

        if( pQueue->m_controls.m_SchedulerEarlyReturnCounter == pQueue->m_controls.m_SchedulerEarlyReturn )
        {
            if( ( ( get_group_id( 0 ) == 1) == ( get_num_groups( 0 ) > 1 )  ) & ( get_local_id( 0 ) == 0 ) )
            {
#ifdef ENABLE_DEBUG_BUFFER
                //Set START time of current (last) scheduler
                if( ( pQueue->m_controls.m_IsProfilingEnabled != 0 ) & ( DebugQueue != 0 ) & ( DebugQueue->m_flags == DDB_SCHEDULER_PROFILING ) )
                {
                    *( ( __global ulong * ) ( &DebugQueue->m_data[ atomic_add( &DebugQueue->m_offset, 2 ) ] ) ) = EventsPool->m_CLcompleteTimestamp;
                }
#endif
                pQueue->m_controls.Temporary[ 2 ]++;

                //SlbOffset is expressed in bytes and for cmd it is needed to convert it to dwords
                __private uint DwordOffset = ( pQueue->m_controls.m_SecondLevelBatchOffset % MAX_SLB_OFFSET ) / DWORD_SIZE_IN_BYTES;
                //BB_START 1st DWORD
                secondaryBatchBuffer[ DwordOffset ] = OCLRT_BATCH_BUFFER_BEGIN_CMD_DWORD0;
                DwordOffset++;
                //BB_START 2nd DWORD - Address, 3rd DWORD Address high
                secondaryBatchBuffer[ DwordOffset++ ] = (uint)(pQueue->m_controls.m_CleanupSectionAddress & 0xFFFFFFFF);
                secondaryBatchBuffer[ DwordOffset ] = (uint)((pQueue->m_controls.m_CleanupSectionAddress >> 32) & 0xFFFFFFFF);
            }
            return;
        }
    }
#endif

    //First check if there are any new command packets on queue_t
    __global IGIL_CommandHeader* pCommand = 0;
    uint GroupID                          = get_group_id( 0 );

#ifndef SCHEDULER_EMULATION
    __local int IDTOffset;
    __local int DSHOffset;
    __local int SLBOffset;
    __local int StackOffset;
    __local int QStorageOffset;
    __local int MarkerOffset;
    __local int BTSoffset;
    __local IGIL_WalkerEnumeration WalkerEnum;
    __local uint ObjectIDS[ MAX_GLOBAL_ARGS ];

#endif

    if( pQueue->m_controls.m_LastScheduleEventNumber > 0 )
    {
        //Check if there are any events that needs updating, each wkg uses all hw threads in wkg to update events
        if( GroupID * PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP20 < pQueue->m_controls.m_LastScheduleEventNumber )
        {
            clk_event_t EventID;
            if( get_local_id( 0 ) % PARALLEL_SCHEDULER_COMPILATION_SIZE_20 == 0 )
            {
                uint ID = ( GroupID * PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP20 ) + ( get_local_id( 0 ) / PARALLEL_SCHEDULER_COMPILATION_SIZE_20 );
                while( ID < pQueue->m_controls.m_LastScheduleEventNumber )
                {
                    EventID = __builtin_astype( ( void* )( ( ulong )pQueue->m_controls.m_EventDependencies[ ID ] ), clk_event_t );
                    UpdateEventsTreeStatusParallel( EventID, eventsPool, ( pQueue->m_controls.m_IsProfilingEnabled != 0 ) );
                    ID      += get_num_groups( 0 ) * PARALLEL_SCHEDULER_HWTHREADS_IN_HW_GROUP20;
                }
            }
        }
        GlobalBarrier( queueStorageBuffer );
    }
    //Queue parsing section
    uint NumberOfEnqueues = pQueue->m_controls.m_TotalNumberOfQueues - pQueue->m_controls.m_PreviousNumberOfQueues;
    if( NumberOfEnqueues > 0 )
    {
        uint InitialOffset              = pQueue->m_controls.m_PreviousHead;
        bool PacketScheduled            = true;
        uint offset                     = 0;
        
        for( uint CurrentPacket = GroupID; CurrentPacket < NumberOfEnqueues; CurrentPacket += get_num_groups( 0 ) )
        {
            if( CurrentPacket == GroupID )
            {
                offset                  = TEMP_IGIL_GetNthCommandHeaderOffset( __builtin_astype( pQueue, queue_t ), InitialOffset, CurrentPacket );
            }
            else
            {
                offset                  = TEMP_IGIL_GetNthCommandHeaderOffset( __builtin_astype( pQueue, queue_t ), offset, get_num_groups( 0 ) );
            }
            pCommand                    = TEMP_IGIL_GetCommandHeader( __builtin_astype( pQueue, queue_t ), offset );

            //Initialize command packet with proper lws
            if( get_local_id( 0 ) == 0 )
            {
                InitializeCommandPacket( pCommand );
            }
         
            //Can I run this command ?
            if( CheckEventStatus( pCommand, eventsPool ) == 0 )
            {
                //Is it marker command ?
                if( pCommand->m_kernelId != IGIL_KERNEL_ID_ENQUEUE_MARKER )
                {
                    //Is there enough IDT space for me ?
                    if( get_local_id( 0 ) == 0 )
                    {
                        int WalkerNeeded = GetWalkerCount( pCommand );
                        //Optimization - check if IDT has free space for me 
                        if( pQueue->m_controls.m_CurrentIDToffset + WalkerNeeded  <= WA_INT_DESC_MAX )
                        {
                            uint Temp = atomic_add( &pQueue->m_controls.m_CurrentIDToffset, WalkerNeeded );
                            if( Temp + WalkerNeeded <= WA_INT_DESC_MAX )
                            {
                                IDTOffset = Temp;
                                DSHOffset = atomic_add( &pQueue->m_controls.m_CurrentDSHoffset, ( MAX_DSH_SIZE_PER_ENQUEUE * WalkerNeeded ) );
                                SLBOffset = ( ( atomic_add( &pQueue->m_controls.m_SecondLevelBatchOffset, ( SECOND_LEVEL_BUFFER_SPACE_FOR_EACH_ENQUEUE * WalkerNeeded ) ) ) % MAX_SLB_OFFSET );
                                BTSoffset = atomic_add( &pQueue->m_controls.m_CurrentSSHoffset, pQueue->m_controls.m_BTmaxSize );
                            }
                            else
                            {
                                IDTOffset = -1;
                            }
                        }
                        else
                        {
                            IDTOffset = -1;
                        }
                    }
                    //Now barrier and check if we can go with scheduling
                    barrier( CLK_LOCAL_MEM_FENCE );

                    if( IDTOffset != -1 )
                    {
                        //This packet is all set, schedule it and we are done with it.
                        //Patch DSH has media curbe load and patch gpgpu walker inside
                        PatchDSHParallelWithDynamicDSH20( SLBOffset,
                                                          DSHOffset,
                                                          IDTOffset,
                                                          kernelData,
                                                          dsh,
                                                          pCommand->m_kernelId,
                                                          pCommand,
                                                          secondaryBatchBuffer,
                                                          pQueue,
                                                          eventsPool,
                                                          ssh,
                                                          BTSoffset,
                                                          &WalkerEnum,
                                                          ObjectIDS
#ifdef ENABLE_DEBUG_BUFFER                                              
                                                          , DebugQueue 
#endif
                                                          );
                        PacketScheduled = true;
                    }
                    else
                    {
                        PacketScheduled = false;
                    }
                }
                else //For marker we need to update returned event status
                {
                    //Check if there is space to track event
                    if( get_local_id( 0 ) == 0 )
                    {
                        uint Temp = atomic_inc( &pQueue->m_controls.m_EnqueueMarkerScheduled );
                        if( Temp < MAX_NUMBER_OF_ENQUEUE_MARKER )
                        {
                            MarkerOffset = Temp;
                        }
                        else
                        {
                            MarkerOffset = -1;
                        }
                    }
                    barrier( CLK_LOCAL_MEM_FENCE );
                    if( MarkerOffset != -1 )
                    {
                        PacketScheduled = true;
                    }
                    else
                    {
                        PacketScheduled = false;
                    }
                }
                //Update event dependencies if any, if there are event waiting for status change, put them on the list.
                if( PacketScheduled == true )
                {
                    if( get_local_id( 0 ) == 0 )
                    {
                        if( ( uint )pCommand->m_event != IGIL_EVENT_INVALID_HANDLE )
                        {
                           pQueue->m_controls.m_EventDependencies[ atomic_inc( &pQueue->m_controls.m_CurrentScheduleEventNumber ) ] = pCommand->m_event;
                        }
                        //Remove event dependencies setting.
                        if( pCommand->m_numDependencies > 0 )
                        {
                            DecreaseEventDependenciesParallel( pCommand, eventsPool );
                        }
                    }
                }
            }
            //Can't schedule it right now, move to storage.
            else 
            {
                if( pQueue->m_controls.m_IsSimulation )
                {
                    barrier( CLK_LOCAL_MEM_FENCE );
                }
                PacketScheduled = false;
            }

            //Allocation failure, move command to stack storage and update stack pointers
            if( PacketScheduled == false )
            {
                if( get_local_id( 0 ) == 0 )
                {
                    StackOffset                  = atomic_dec( &pQueue->m_controls.m_StackTop ) - 1;
                    QStorageOffset               = atomic_sub( &pQueue->m_controls.m_QstorageTop, pCommand->m_commandSize ) - pCommand->m_commandSize;
                    commandsStack[ StackOffset ] = QStorageOffset;
                }
                barrier( CLK_LOCAL_MEM_FENCE );
                __global char* ptrQueue = ( __global char* )pQueue;
                GLOBAL_MEMCPY( ( __global void* )&queueStorageBuffer[ QStorageOffset / 4 ], (__global void * )&ptrQueue[ offset ] , pCommand->m_commandSize );
            }
            else if( pQueue->m_controls.m_IsSimulation )
            {
                barrier( CLK_LOCAL_MEM_FENCE );
            }
        }

        //In case there were new enqueues on queue_t, all work items must hit the global barrier before they can start taking items from the stack.
        GlobalBarrierUpdateQueue( queueStorageBuffer, pQueue );
    }

    //Check stack only when there are free IDTS
    if( ( pQueue->m_controls.m_IDTAfterFirstPhase < WA_INT_DESC_MAX ) &
        ( pQueue->m_controls.m_PreviousStackTop != pQueue->m_controls.m_StackSize ) )
    {
        //Start stack browsing
        uint MyID            = get_group_id( 0 );
        //Start browsing from the begining of the previous stack top
        uint CurrentOffset   = pQueue->m_controls.m_PreviousStackTop + MyID;
        uint CommandOffset   = 0;

        while( CurrentOffset < pQueue->m_controls.m_StackSize )
        {
            CommandOffset = commandsStack[ CurrentOffset ];

            if( CommandOffset != 0 )
            {
                pCommand = GetCommandHeaderFromStorage( ( __global uint* )queueStorageBuffer, CommandOffset );

                //Can I run this command ?
                if( CheckEventStatus( pCommand, eventsPool ) == 0 )
                {
                    //Is it marker command ?
                    if( pCommand->m_kernelId != IGIL_KERNEL_ID_ENQUEUE_MARKER )
                    {
                        //Is there enough IDT space for me ?
                        if( get_local_id( 0 ) == 0 )
                        {
                            int WalkerNeeded = GetWalkerCount( pCommand );
                            //Optimization - check if IDT has free space for me 
                            if( pQueue->m_controls.m_CurrentIDToffset + WalkerNeeded  <= WA_INT_DESC_MAX )
                            {
                                uint Temp = atomic_add( &pQueue->m_controls.m_CurrentIDToffset, WalkerNeeded );
                                if( Temp + WalkerNeeded <= WA_INT_DESC_MAX )
                                {
                                    IDTOffset = Temp;
                                    DSHOffset = atomic_add( &pQueue->m_controls.m_CurrentDSHoffset, ( MAX_DSH_SIZE_PER_ENQUEUE * WalkerNeeded ) );
                                    SLBOffset = ( ( atomic_add( &pQueue->m_controls.m_SecondLevelBatchOffset, ( SECOND_LEVEL_BUFFER_SPACE_FOR_EACH_ENQUEUE * WalkerNeeded ) ) ) % MAX_SLB_OFFSET );
                                    BTSoffset = atomic_add( &pQueue->m_controls.m_CurrentSSHoffset, pQueue->m_controls.m_BTmaxSize );
                                }
                                else
                                {
                                    IDTOffset = -1;
                                }
                            }
                            else
                            {
                                 IDTOffset = -1;
                            }
                        }
                        //Now barrier and check if we can go with scheduling
                        barrier( CLK_LOCAL_MEM_FENCE );
                        if( IDTOffset != -1 )
                        {
                            //This packet is all set, schedule it and we are done with it.
                            //Patch DSH has media curbe load and patch gpgpu walker inside
                            PatchDSHParallelWithDynamicDSH20( SLBOffset,
                                                              DSHOffset,
                                                              IDTOffset,
                                                              kernelData,
                                                              dsh,
                                                              pCommand->m_kernelId,
                                                              pCommand,
                                                              secondaryBatchBuffer,
                                                              pQueue,
                                                              eventsPool,
                                                              ssh,
                                                              BTSoffset,
                                                              &WalkerEnum,
                                                              ObjectIDS
#ifdef ENABLE_DEBUG_BUFFER                                              
                                                              , DebugQueue
#endif
                                                              );
                            pCommand->m_commandState = CAN_BE_RECLAIMED;
                            //Reset stack offset
                            commandsStack[ CurrentOffset ] = 0;

                            //Update event status
                            if( get_local_id( 0 ) == 0 )
                            {
                                //Add events dependant on this command to list of events neeeded to be updated.
                                if( ( uint )pCommand->m_event != IGIL_EVENT_INVALID_HANDLE )
                                {
                                    pQueue->m_controls.m_EventDependencies[ atomic_inc( &pQueue->m_controls.m_CurrentScheduleEventNumber ) ] = pCommand->m_event;
                                }

                                //Remove event dependencies setting.
                                if( pCommand->m_numDependencies > 0 )
                                {
                                    DecreaseEventDependenciesParallel( pCommand, eventsPool );
                                }
                            }
                        }
                    }
                    else // For marker we need to update returned event status
                    {
                        barrier( CLK_GLOBAL_MEM_FENCE );
                        //Check if there is space to track event
                        if( get_local_id( 0 ) == 0 )
                        {
                            uint Temp = atomic_inc( &pQueue->m_controls.m_EnqueueMarkerScheduled );
                            if( Temp < MAX_NUMBER_OF_ENQUEUE_MARKER )
                            {
                                pCommand->m_commandState = CAN_BE_RECLAIMED;
                                commandsStack[ CurrentOffset ] = 0;
                                //Add events dependant on this command to list of events neeeded to be updated.
                                if( ( uint )pCommand->m_event != IGIL_EVENT_INVALID_HANDLE )
                                {
                                    pQueue->m_controls.m_EventDependencies[ atomic_inc( &pQueue->m_controls.m_CurrentScheduleEventNumber ) ] = pCommand->m_event;
                                }

                                //Remove event dependencies setting.
                                if( pCommand->m_numDependencies > 0 )
                                {
                                    DecreaseEventDependenciesParallel( pCommand, eventsPool );
                                }
                            }
                        }
                    }
                }
            }
            CurrentOffset += get_num_groups( 0 );
            if( pQueue->m_controls.m_IsSimulation )
            {
                barrier( CLK_LOCAL_MEM_FENCE );
            }
        }
    }

    //Finish execution and check end conditons
    //Execute this global barrier only when needed, i.e. stack browsing was executed or new item were added on the stack
    if( ( pQueue->m_controls.m_PreviousStackTop != pQueue->m_controls.m_StackSize ) |
        ( pQueue->m_controls.m_StackTop != pQueue->m_controls.m_PreviousStackTop ) )
    {
        GlobalBarrier( queueStorageBuffer );
    }

    //Cleanup & resource reclamation section
    //We are after global sync section, we can do anything to globals right now.
    if( ( get_local_id( 0 ) == 0 ) & ( get_group_id( 0 ) == 0 ) )
    {
        {
            pQueue->m_controls.m_CurrentDSHoffset           = pQueue->m_controls.m_DynamicHeapStart;
            pQueue->m_controls.m_IDTAfterFirstPhase         = 1;
            pQueue->m_controls.m_PreviousNumberOfQueues     = pQueue->m_controls.m_TotalNumberOfQueues;
            pQueue->m_controls.m_LastScheduleEventNumber    = pQueue->m_controls.m_CurrentScheduleEventNumber;
            pQueue->m_controls.m_CurrentScheduleEventNumber = 0;
        }
    }

    //Schedule scheduler
    if( ( (get_group_id( 0 ) == 1) == (get_num_groups( 0 ) > 1)  ) & ( get_local_id(0) == 0 ) )    
    {
        pQueue->m_controls.m_SecondLevelBatchOffset = ( pQueue->m_controls.m_SecondLevelBatchOffset % MAX_SLB_OFFSET );

        //If we scheduled any blocks, put scheduler right after
        if( ( pQueue->m_controls.m_CurrentIDToffset > 1 ) | ( pQueue->m_controls.m_EnqueueMarkerScheduled > 0 ) )
        {
            AddCmdsInSLBforScheduler20Parallel( pQueue->m_controls.m_SecondLevelBatchOffset,
                                                pQueue,
                                                secondaryBatchBuffer,
                                                dsh );
            
            //If we have profiling enabled, we need CL_COMPLETE time, which is before next scheduler starts
            if( pQueue->m_controls.m_IsProfilingEnabled != 0 )
            {
                ulong Address  = ( ulong ) &( eventsPool->m_CLcompleteTimestamp );
                //Emit pipecontrol with timestamp write
                PatchPipeControlProfilingAddres( secondaryBatchBuffer,
                                                 pQueue->m_controls.m_SecondLevelBatchOffset,
                                                 Address,
                                                 PIPE_CONTROL_FOR_TIMESTAMP_START_OFFSET );
                //Bit after scheduler may be set by some other command, reset it to 0
                DisablePostSyncBitInPipeControl( secondaryBatchBuffer,
                                                 pQueue->m_controls.m_SecondLevelBatchOffset,
                                                 PIPE_CONTROL_FOR_TIMESTAMP_END_OFFSET_TO_PATCH );
#ifdef ENABLE_DEBUG_BUFFER
                if( ( DebugQueue != 0 ) & ( DebugQueue->m_flags == DDB_SCHEDULER_PROFILING ) )
                {
                    //Store Current scheduler START time
                    *((__global ulong * ) (&DebugQueue->m_data[ atomic_add( &DebugQueue->m_offset, 2 ) ] )) = EventsPool->m_CLcompleteTimestamp;
                    
                    //Set address to store next scheduler's END time 
                    PatchPipeControlProfilingAddres( secondaryBatchBuffer,
                                                        pQueue->m_controls.m_SecondLevelBatchOffset,
                                                        ( ulong )(&DebugQueue->m_data[ atomic_add( &DebugQueue->m_offset, 2 ) ]),
                                                        PIPE_CONTROL_FOR_TIMESTAMP_END_OFFSET );
                }
#endif
            }

            //Program pipe controls around scheduler to make sure it is not executed concurently to blocks
            else
            {
                //Locate previous pipe control
                int PreviousOffset = pQueue->m_controls.m_SecondLevelBatchOffset - SECOND_LEVEL_BUFFER_SPACE_FOR_EACH_ENQUEUE;
                //If offset is negative it means we are first command after chaining
                if( PreviousOffset < 0 )
                {
                    PreviousOffset += MAX_SLB_OFFSET;
                }
                //Tighten previous pipecontrol
                uint DwordOffset = PreviousOffset / DWORD_SIZE_IN_BYTES;

                PutCSStallPipeControl( secondaryBatchBuffer, DwordOffset, PIPE_CONTROL_FOR_TIMESTAMP_END_OFFSET );

                //Now put pipe control after scheduler
                DwordOffset = pQueue->m_controls.m_SecondLevelBatchOffset / DWORD_SIZE_IN_BYTES;

                PutCSStallPipeControl( secondaryBatchBuffer, DwordOffset, PIPE_CONTROL_FOR_TIMESTAMP_END_OFFSET );
            }

            pQueue->m_controls.m_SecondLevelBatchOffset  += SECOND_LEVEL_BUFFER_SPACE_FOR_EACH_ENQUEUE;
            pQueue->m_controls.m_CurrentIDToffset        = 1;
            pQueue->m_controls.m_EnqueueMarkerScheduled  = 0;
            pQueue->m_controls.Temporary[1]++;
            pQueue->m_controls.m_CurrentSSHoffset        = pQueue->m_controls.m_BTbaseOffset;
        }
        //Nothing to schedule, return to the host
        else
        {
#ifdef ENABLE_DEBUG_BUFFER
            //Set START time of current (last) scheduler
            if( ( pQueue->m_controls.m_IsProfilingEnabled != 0 ) & ( DebugQueue != 0 ) & ( DebugQueue->m_flags == DDB_SCHEDULER_PROFILING ) )
            {
                *((__global ulong * ) (&DebugQueue->m_data[ atomic_add( &DebugQueue->m_offset, 2 ) ] )) = EventsPool->m_CLcompleteTimestamp;
            }
#endif
            pQueue->m_controls.Temporary[2]++;
            pQueue->m_controls.m_SLBENDoffsetInBytes = ( int ) pQueue->m_controls.m_SecondLevelBatchOffset;
            //SlbOffset is expressed in bytes and for cmd it is needed to convert it to dwords
            __private uint DwordOffset = pQueue->m_controls.m_SecondLevelBatchOffset / DWORD_SIZE_IN_BYTES;
            //BB_START 1st DWORD
            secondaryBatchBuffer[ DwordOffset ] = OCLRT_BATCH_BUFFER_BEGIN_CMD_DWORD0;
            DwordOffset++;
            //BB_START 2nd DWORD - Address, 3rd DWORD Address high
            secondaryBatchBuffer[ DwordOffset++ ] = (uint)( pQueue->m_controls.m_CleanupSectionAddress & 0xFFFFFFFF );
            secondaryBatchBuffer[ DwordOffset++ ] = (uint)( ( pQueue->m_controls.m_CleanupSectionAddress >> 32 ) & 0xFFFFFFFF );
        }
    }

    //Parallel stack compaction
    if( ( ( get_group_id( 0 ) == 2 ) == ( get_num_groups( 0 ) > 2 )  ) & ( get_local_id( 0 ) == 0 ) )
    {
        uint Current = pQueue->m_controls.m_StackTop + get_local_id( 0 );
        uint StackSize = pQueue->m_controls.m_StackSize;
        uint Found = 0;

        while( ( Current < StackSize ) && ( Found == 0 ) )
        {
            __global uint * pCmdStackBlock = (__global uint *)( commandsStack + Current );
            //We have found an element
            if( *pCmdStackBlock != 0 )
            {
                Found = 1;
            }
            else 
            {
                Current += get_local_size( 0 );
            }
        }

        if ( Found == 1 )
        {
            atomic_min( &pQueue->m_controls.m_StackTop, Current );
            atomic_min( &pQueue->m_controls.m_PreviousStackTop, Current );
        }
    }

    //Qstorage compaction
    if( ( ( get_group_id( 0 ) == 3 ) == ( get_num_groups( 0 ) > 3 )  ) & ( get_local_id( 0 ) == 0 ) )
    {
        uint ReclaimFurhter = 1;
        while( ( pQueue->m_controls.m_QstorageTop < pQueue->m_controls.m_QstorageSize ) & ( ReclaimFurhter == 1 ) )
        {
            pCommand = GetCommandHeaderFromStorage( ( __global uint* ) queueStorageBuffer, pQueue->m_controls.m_QstorageTop );
            if( pCommand->m_commandState == CAN_BE_RECLAIMED )
            {
                pQueue->m_controls.m_QstorageTop += pCommand->m_commandSize;
            }
            else
            {
                ReclaimFurhter = 0;
            }
        }

        pQueue->m_controls.m_PreviousStorageTop      = pQueue->m_controls.m_QstorageTop;

#ifndef DISABLE_RESOURCE_RECLAMATION
        //Reclaim space on queue_t, do this only if there is enough space
        //1 KB is used for global barrier, make sure this space will never be used.
        if( pQueue->m_controls.m_QstorageTop - 1024 > pQueue->m_size )
        {
            //In this case we can take full queue_t next time we enter scheduler, so reclaim full space on queue_t
            pQueue->m_head = IGIL_DEVICE_QUEUE_HEAD_INIT;
        }
#endif
        pQueue->m_controls.m_PreviousHead           = pQueue->m_head;
    }
}
