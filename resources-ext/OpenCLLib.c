//Written by Ben Craine, 2023

//std lib headers
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

//openCL headers
#include <CL/cl_platform.h>
#include <CL/cl.h>

//dana headers
#include "dana_api_1.7/dana_lib_defs.h"
#include "dana_api_1.7/dana_types.h"
#include "dana_api_1.7/nli_util.h"
#include "dana_api_1.7/vmi_util.h"

#define MAX_DEVICES 1

static CoreAPI *api;

static GlobalTypeLink* intArrayGT = NULL;
static GlobalTypeLink *charArrayGT = NULL;
static GlobalTypeLink* stringArrayGT = NULL;
static GlobalTypeLink* stringItemGT = NULL;
static GlobalTypeLink* intMatrixGT = NULL;

cl_platform_id* globalClPlatform;
cl_device_id* devices;
cl_program* programs;

u_int getNumberOfDevices() {
    u_int numOfDevices = 0;
    for (int i = 0; i < MAX_DEVICES; i++) {
        cl_device_id* ittr = devices+i;
        if (*ittr != NULL) {
            numOfDevices++;
        }
    }
    return numOfDevices;
}

INSTRUCTION_DEF init(void) {
    printf("in init\n");
    cl_int CL_err = CL_SUCCESS;
    cl_uint numPlatforms = 0;

    devices = (cl_device_id*) malloc(sizeof(cl_device_id)*MAX_DEVICES);
    globalClPlatform = (cl_platform_id*) malloc(sizeof(cl_platform_id));
    
    CL_err = clGetPlatformIDs( 1, globalClPlatform, &numPlatforms );
    if (CL_err != CL_SUCCESS) {
        printf("platform id broke in init\n");
    }
    
    CL_err = clGetDeviceIDs(*globalClPlatform, CL_DEVICE_TYPE_GPU, MAX_DEVICES, devices, NULL);

    return RETURN_OK;
}

INSTRUCTION_DEF getComputeDeviceIDs(VFrame* cframe) {
    printf("in getComputeDevicesIDs\n");
    cl_int CL_err = CL_SUCCESS;

    if (devices == NULL) {
        printf("no devices found");
        //return empty array
        DanaEl* newArray = api->makeArray(stringArrayGT, 0, NULL);
        api->returnEl(cframe, newArray);
        //exit
        return RETURN_OK;
    }

    u_int numOfDevices = getNumberOfDevices();

    printf("devices not null\n");
    cl_device_id* cpy = devices;

    printf("num of devices: %d\n", numOfDevices);

    if (numOfDevices > 0) {
        DanaEl* newArray = api->makeArray(intArrayGT, numOfDevices, NULL);
        cpy = devices;
        for (size_t i = 0; i < numOfDevices; i++) {
            api->setArrayCellInt(newArray, i, (size_t) *cpy);
            cpy++;
        }
        api->returnEl(cframe, newArray);
    }
    else {
        printf("no devices found\n");
        DanaEl* newArray = api->makeArray(intArrayGT, numOfDevices, NULL);
        api->returnEl(cframe, newArray);
    }
    return RETURN_OK;
}

INSTRUCTION_DEF getComputeDevices(VFrame* cframe) {
    printf("in getComputeDevices\n");
    cl_int CL_err = CL_SUCCESS;
    if (devices == NULL) {
        printf("no devices found");
        //return empty array
        DanaEl* newArray = api->makeArray(stringArrayGT, 0, NULL);
        api->returnEl(cframe, newArray);
        //exit
        return RETURN_OK;
    }

    u_int numOfDevices = getNumberOfDevices();

    printf("devices not null\n");
    cl_device_id* cpy = devices;

    printf("num of devices: %d\n", numOfDevices);

    if (numOfDevices > 0) {
        DanaEl* newArray = api->makeArray(stringArrayGT, numOfDevices, NULL);

        cpy = devices;
        for (size_t i = 0; i < numOfDevices; i++) {
            char* deviceNameBuf = (char*) malloc(sizeof(char)*200);
            CL_err = clGetDeviceInfo(*cpy, CL_DEVICE_NAME, sizeof(char)*200, deviceNameBuf, NULL);

            size_t strLen = strlen(deviceNameBuf);

            DanaEl* nextStringInArray = api->makeData(stringItemGT);

            unsigned char* cnt = NULL;
            DanaEl* itemArray = api->makeArray(charArrayGT, strLen, &cnt);

            memcpy(cnt, deviceNameBuf, strLen);

            api->setDataFieldEl(nextStringInArray, 0, itemArray);
            api->setArrayCellEl(newArray, i, nextStringInArray);

            cpy++;
        }
        api->returnEl(cframe, newArray);
    }
    else {
        printf("no devices found\n");
        DanaEl* newArray = api->makeArray(stringArrayGT, 0, NULL);
        api->returnEl(cframe, newArray);
    }
    printf("returning from get devices\n");
    return RETURN_OK;
}

INSTRUCTION_DEF createContext(VFrame* cframe) {
    cl_int CL_Err = CL_SUCCESS;
    printf("in context creation\n");
    const cl_context_properties props[] = {CL_CONTEXT_PLATFORM, *globalClPlatform, 0};
    cl_context context = clCreateContext(props, 1, devices, NULL, NULL, &CL_Err);
    if (CL_Err != CL_SUCCESS) {
        printf("cl not likey\n");
        printf("code: %d\n", CL_Err);
    }
    printf("from context: %lu\n", (u_int64_t) context);
    api->returnInt(cframe, (u_int64_t) context);

    return RETURN_OK;
}

INSTRUCTION_DEF createAsynchQueue(FrameData* cframe) {
    printf("in create asynch\n");
    cl_int CL_err = CL_SUCCESS;
    u_int64_t rawParam = api->getParamInt(cframe, 0);
    cl_context context = (cl_context) rawParam; 
    const cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    cl_command_queue newQ = clCreateCommandQueueWithProperties(context, *devices, props, &CL_err);
    if(CL_err != CL_SUCCESS) {
        printf("err in asynch creation\n");
    }
    printf("from q creation: %lu\n", (uint64_t) newQ);
    api->returnInt(cframe, (uint64_t) newQ);
    return RETURN_OK;
}

INSTRUCTION_DEF createSynchQueue(FrameData* cframe) {
    printf("in create synch\n");
    cl_int CL_err = CL_SUCCESS;
    u_int64_t rawParam = api->getParamInt(cframe, 0);
    cl_context context = (cl_context) rawParam; 
    const cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    cl_command_queue newQ = clCreateCommandQueueWithProperties(context, *devices, props, &CL_err);
    if(CL_err != CL_SUCCESS) {
        printf("err in asynch creation\n");
    }
    printf("from q creation: %lu\n", (uint64_t) newQ);
    api->returnInt(cframe, (uint64_t) newQ);
    return RETURN_OK;
}

INSTRUCTION_DEF createArray(FrameData* cframe) {
    printf("in create array\n");
    cl_int CL_err = CL_SUCCESS;
    u_int64_t rawParam = api->getParamInt(cframe, 0);
    cl_context context = (cl_context) rawParam; 

    rawParam = api->getParamInt(cframe, 1);
    size_t bytes = (size_t) rawParam;

    cl_mem newArray = clCreateBuffer(context, CL_MEM_READ_WRITE, bytes, NULL, &CL_err);

    if (CL_err != CL_SUCCESS) {
        printf("issue in array creation\n");
    }

    printf("newArray: %p\n", newArray);

    api->returnInt(cframe, (uint64_t) newArray);

    return RETURN_OK;
}

INSTRUCTION_DEF writeIntArray(FrameData* cframe) {
    uint64_t rawParam = api->getParamInt(cframe, 0);
    cl_command_queue queue = (cl_command_queue) rawParam;
    rawParam = api->getParamInt(cframe, 1);
    cl_mem memObj = (cl_mem) rawParam;

    DanaEl* hostArray = api->getParamEl(cframe, 2);
    size_t hostArrayLen = api->getArrayLength(hostArray);

    uint64_t* rawHostArray = (uint64_t*) malloc(sizeof(uint64_t)*hostArrayLen);
    uint64_t* rawHostArrayCpy = rawHostArray;
    for (int i = 0; i < hostArrayLen; i++) {
        *rawHostArrayCpy = api->getArrayCellInt(hostArray, i);
        rawHostArrayCpy++;
    }

    cl_int CL_err = clEnqueueWriteBuffer(queue, memObj, CL_TRUE, 0, hostArrayLen*8, rawHostArray, NULL, NULL, NULL);
    if (CL_err != CL_SUCCESS) {
        printf("error in write buffer");
    }
    return RETURN_OK;    
}

INSTRUCTION_DEF readIntArray(FrameData* cframe) {
    uint64_t rawParam = api->getParamInt(cframe, 0);
    cl_command_queue queue = (cl_command_queue) rawParam;
    rawParam = api->getParamInt(cframe, 1);
    cl_mem memObj = (cl_mem) rawParam;

    DanaEl* hostArray = api->getParamEl(cframe, 2);
    size_t hostArrayLen = api->getArrayLength(hostArray);

    uint64_t* rawHostArray = (uint64_t*) api->getArrayContent(hostArray);

    cl_int CL_err = clEnqueueReadBuffer(queue, memObj, CL_TRUE, 0, hostArrayLen*8, rawHostArray, NULL, NULL, NULL);
    if (CL_err != CL_SUCCESS) {
        printf("error in read buffer");
    }

    for (int i = 0; i < hostArrayLen; i++) {
        api->setArrayCellInt(hostArray, i, *rawHostArray);
        rawHostArray++;
    }

    api->returnEl(cframe, hostArray);

    return RETURN_OK;    
}

INSTRUCTION_DEF createMatrix(FrameData* cframe) {
    printf("in create matrix\n");
    cl_int CL_err = CL_SUCCESS;
    u_int64_t rawParam = api->getParamInt(cframe, 0);
    cl_context context = (cl_context) rawParam; 

    rawParam = api->getParamInt(cframe, 1);
    size_t x_bytes = (size_t) rawParam;

    rawParam = api->getParamInt(cframe, 2);
    size_t y_bytes = (size_t) rawParam;

    cl_image_desc desc = {CL_MEM_OBJECT_IMAGE2D, x_bytes, y_bytes, 0, 1, 0, 0, 0, 0, NULL};
    cl_image_format form = {CL_R, CL_UNSIGNED_INT32};

    cl_mem newMatrix = clCreateImage(context, CL_MEM_READ_WRITE, &form, &desc, NULL, &CL_err);

    if (CL_err != CL_SUCCESS) {
        printf("issue in matrix creation\n");
    }

    printf("newMatrix: %p\n", newMatrix);

    api->returnInt(cframe, (uint64_t) newMatrix);

    return RETURN_OK;
}

INSTRUCTION_DEF writeIntMatrix(FrameData* cframe) {
    uint64_t rawParam = api->getParamInt(cframe, 0);
    cl_command_queue queue = (cl_command_queue) rawParam;
    rawParam = api->getParamInt(cframe, 1);
    cl_mem memObj = (cl_mem) rawParam;

    DanaEl* hostMatrix = api->getParamEl(cframe, 2);
    size_t dim = 2; //only supporting 2d matricies
    size_t* dims = api->getArrayDimensions(hostMatrix, &dim);
    printf("lens: %lu\n", *dims);
    printf("lens: %lu\n", *(dims+1));

    uint32_t* rawHostMatrix = (uint32_t*) malloc(sizeof(uint32_t)*dims[0]*dims[1]);
    uint32_t* rawHostMatrixCpy = rawHostMatrix;
    for (int i = 0; i < dims[0]; i++) {
        for (int j = 0; j < dims[1]; j++) {
            *rawHostMatrixCpy = api->getArrayCellInt(hostMatrix, (i*dims[1])+j);
            printf("writing to gpu: %u\n", *rawHostMatrixCpy);
            rawHostMatrixCpy++;
        }
    }

    size_t origin[] = {0, 0, 0};
    size_t region[] = {dims[0], dims[1], 1};
    cl_int CL_err = clEnqueueWriteImage(queue, memObj, CL_TRUE, origin, region, 0, 0, rawHostMatrix, 0, NULL, NULL);

    if (CL_err != CL_SUCCESS) {
        printf("error in write buffer: %d\n", CL_err);
    }
    else {
        printf("write matrix success\n");
    }
    return RETURN_OK;    
}

INSTRUCTION_DEF readIntMatrix(FrameData* cframe) {
    uint64_t rawParam = api->getParamInt(cframe, 0);
    cl_command_queue queue = (cl_command_queue) rawParam;
    rawParam = api->getParamInt(cframe, 1);
    cl_mem memObj = (cl_mem) rawParam;

    DanaEl* hostMatrix = api->getParamEl(cframe, 2);
    size_t dim = 2;
    size_t* hostMatrixLens = api->getArrayDimensions(hostMatrix, &dim);
    printf("lens: %lu\n", *hostMatrixLens);
    printf("lens: %lu\n", *(hostMatrixLens+1));

    uint32_t* rawHostMatrix = (uint32_t*) malloc(sizeof(uint32_t)*hostMatrixLens[0]*hostMatrixLens[1]);

    size_t origin[] = {0, 0, 0};
    size_t region[] = {hostMatrixLens[0], hostMatrixLens[1], 1};

    int CL_Err = clEnqueueReadImage(queue, memObj, CL_TRUE, origin, region, 0, 0, rawHostMatrix, 0, NULL, NULL);
    if (CL_Err != CL_SUCCESS) {
        printf("error in read matrix: %d\n", CL_Err);
    }

    for (int i = 0; i < hostMatrixLens[0]; i++) {
        for (int j = 0; j < hostMatrixLens[1]; j++) {
            printf("read from gpu: %u\n", *rawHostMatrix);
            api->setArrayCellInt(hostMatrix, (i*hostMatrixLens[1])+j, *rawHostMatrix);
            rawHostMatrix++;
        }
    }

    api->returnEl(cframe, hostMatrix);

    return RETURN_OK;    
}

INSTRUCTION_DEF destroyMemoryArea(FrameData* cframe) {
    printf("in destroy mem area\n");
    cl_int CL_err = CL_SUCCESS;
    u_int64_t rawParam = api->getParamInt(cframe, 0);
    cl_mem memObj = (cl_mem) rawParam; 
    printf("memObj: %p\n", memObj);
    //the next line seg faults if memObj has already been released...
    //averting this has been attemted in dana, but I'll leave this note
    //here just in case
    CL_err = clReleaseMemObject(memObj);
    if (CL_err != CL_SUCCESS) {
        printf("error in freeing memory area");
    }
    return RETURN_OK;
}

INSTRUCTION_DEF createProgram(FrameData* cframe) {
    printf("in createProgram\n");
    cl_program prog;
    cl_int CL_err = CL_SUCCESS;

    uint64_t rawParam = api->getParamInt(cframe, 0);
    cl_context context = (cl_context) rawParam;

    char** programStrings = (char**) malloc(sizeof(char*));
    char* programSource = x_getParam_char_array(api, cframe, 1);
    *programStrings = programSource;

    prog = clCreateProgramWithSource(context, 1, (const char**) programStrings, NULL, &CL_err);
    if (CL_err != CL_SUCCESS) {
        printf("err when creating program: %d\n", CL_err);
    }

    CL_err = CL_SUCCESS;
    cl_device_id* deviceList = (cl_device_id*) malloc(sizeof(cl_device_id));
    *deviceList = *devices;
    CL_err = clBuildProgram(prog, 1, deviceList, NULL, NULL, NULL);
    if (CL_err != CL_SUCCESS) {
        size_t len;
        char buf[2048];
        clGetProgramBuildInfo(prog, *deviceList, CL_PROGRAM_BUILD_LOG, sizeof(buf), buf, &len);
        printf("%s\n",buf);
    }

    api->returnInt(cframe, (uint64_t) prog);

    return RETURN_OK;
}

INSTRUCTION_DEF prepareKernel(FrameData* cframe) {
    uint64_t rawParam = api->getParamInt(cframe, 0);
    cl_program program = (cl_program) rawParam;
    rawParam = api->getParamInt(cframe, 2);
    size_t paramCount = (size_t) rawParam;

    DanaEl* paramArray = api->getParamEl(cframe, 1);
    cl_mem* rawParamArray = (cl_mem*) malloc(sizeof(cl_mem)*paramCount);
    cl_mem* rawParamArrayCpy = rawParamArray;
    for (int i = 0; i < paramCount; i++) {
        *rawParamArrayCpy = (cl_mem) api->getArrayCellInt(paramArray, i);
        rawParamArrayCpy++;
    }

    char* progName = x_getParam_char_array(api, cframe, 3);

    cl_int CL_err = CL_SUCCESS;
    cl_kernel kernel = clCreateKernel(program, progName, &CL_err);

    if (CL_err != CL_SUCCESS) {
        printf("issue with kernel creation: %d\n", CL_err);
    }

    rawParamArrayCpy = rawParamArray;
    for (int i = 0; i < paramCount; i++) {
        printf("memObj: %p\n", *rawParamArrayCpy);
        rawParamArrayCpy++;
    }

    CL_err = CL_SUCCESS;
    rawParamArrayCpy = rawParamArray;
    for (int i = 0; i < paramCount; i++) {
        CL_err = clSetKernelArg(kernel, i, sizeof(uint64_t), rawParamArrayCpy);
        if (CL_err != CL_SUCCESS) {
            printf("issue with kernel args: %d\n", CL_err);
        }
        rawParamArrayCpy++;
    }
    CL_err = clSetKernelArg(kernel, paramCount, sizeof(size_t), &paramCount);

    api->returnInt(cframe, (uint64_t) kernel);

    return RETURN_OK;

}

INSTRUCTION_DEF runKernel(FrameData* cframe) {
    uint64_t rawParam = api->getParamInt(cframe, 0);
    cl_kernel kernel = (cl_kernel) rawParam;
    rawParam = api->getParamInt(cframe, 1);
    cl_command_queue queue = (cl_command_queue) rawParam;

    uint64_t globalWorkers = 50000;

    cl_int CL_err = CL_SUCCESS;
    CL_err = clEnqueueNDRangeKernel(queue, kernel, 1, NULL, &globalWorkers, NULL, 0, NULL, NULL);
    if (CL_err != CL_SUCCESS) {
        printf("error execing kernel");
    }

    return RETURN_OK;
}

INSTRUCTION_DEF findPlatforms(void) {
    cl_int CL_err = CL_SUCCESS;
    cl_uint numPlatforms = 0;
    
    CL_err = clGetPlatformIDs( 0, NULL, &numPlatforms );
    
    if (CL_err == CL_SUCCESS) {
        printf("%u platform(s) found\n", numPlatforms);
    }
    else {
        printf("clGetPlatformIDs(%i)\n", CL_err);
    }

    return RETURN_OK;
}

Interface* load(CoreAPI* capi) {
    api = capi;

    setInterfaceFunction("findPlatforms", findPlatforms);
    setInterfaceFunction("getComputeDeviceIDs", getComputeDeviceIDs);
    setInterfaceFunction("getComputeDevices", getComputeDevices);
    setInterfaceFunction("init", init);
    setInterfaceFunction("createContext", createContext);
    setInterfaceFunction("createAsynchQueue", createAsynchQueue);
    setInterfaceFunction("createSynchQueue", createSynchQueue);
    setInterfaceFunction("createArray", createArray);
    setInterfaceFunction("writeIntArray", writeIntArray);
    setInterfaceFunction("readIntArray", readIntArray);
    setInterfaceFunction("createMatrix", createMatrix);
    setInterfaceFunction("writeIntMatrix", writeIntMatrix);
    setInterfaceFunction("readIntMatrix", readIntMatrix);
    setInterfaceFunction("destroyMemoryArea", destroyMemoryArea);
    setInterfaceFunction("createProgram", createProgram);
    setInterfaceFunction("prepareKernel", prepareKernel);
    setInterfaceFunction("runKernel", runKernel);

    charArrayGT = api->resolveGlobalTypeMapping(getTypeDefinition("char[]"));
    stringArrayGT = api->resolveGlobalTypeMapping(getTypeDefinition("String[]"));
    stringItemGT = api->resolveGlobalTypeMapping(getTypeDefinition("String"));
    intArrayGT = api->resolveGlobalTypeMapping(getTypeDefinition("int[]"));
    intMatrixGT = api->resolveGlobalTypeMapping(getTypeDefinition("int[][]"));

    return getPublicInterface();
}

void unload() {
    api->decrementGTRefCount(charArrayGT);
    api->decrementGTRefCount(stringArrayGT);
    api->decrementGTRefCount(stringItemGT);
    api->decrementGTRefCount(intArrayGT);
    api->decrementGTRefCount(intMatrixGT);
}
