//Written by Ben Craine, 2023

//std lib headers
#include <math.h>
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

#define FLOAT 0
#define UINT 1

#define MAX_PLATFORMS 100
#define MAX_DEVICES 100

static CoreAPI *api;

static GlobalTypeLink* intArrayGT = NULL;
static GlobalTypeLink* charArrayGT = NULL;
static GlobalTypeLink* stringArrayGT = NULL;
static GlobalTypeLink* stringItemGT = NULL;
static GlobalTypeLink* intMatrixGT = NULL;
static GlobalTypeLink* decArrayGT = NULL;

uint8_t alreadyInitFlag = 0;

cl_platform_id* globalClPlatforms;
cl_uint numofPlatforms;
cl_uint* numOfDevicesPerPlatform;
cl_device_id** devices;
cl_program* programs;

typedef struct _device_context_map {
    cl_device_id device;
    cl_context context;
    struct _device_context_map* next;
} DC_MAP;

typedef struct _context_list_item {
    cl_context context;
    cl_platform_id platform;
    cl_device_id* devices;
    uint8_t numOfDevices;
    struct _context_list_item* next;
} CONTEXT_LI;

typedef struct _one_per_dana_comp {
    CONTEXT_LI* contexts;
    DC_MAP* map;
    DC_MAP* mapEnd;
} DANA_COMP;

void addNewContext(DANA_COMP* danaComp, CONTEXT_LI* newContext) {
    if (danaComp->contexts == NULL) {
        danaComp->contexts = newContext;
        return;
    }

    CONTEXT_LI* probe = danaComp->contexts;
    for (probe; probe->next != NULL; probe = probe->next) {
    }
    probe->next = newContext;
}

int getNumOfContexts(DANA_COMP* danaComp) {
    CONTEXT_LI* probe = danaComp->contexts;
    if (danaComp->contexts == NULL) {
        return 0;
    }
    int count = 0;
    for (probe; probe->next != NULL; probe = probe->next) {
        count++;
    }
    count++;
    return count;
}

CONTEXT_LI* getContextByIndex(DANA_COMP* danaComp, int n) {
    CONTEXT_LI* probe = danaComp->contexts;
    if (danaComp->contexts == NULL) {
        return NULL;
    }
    int count = 0;
    for (probe; probe->next != NULL; probe = probe->next) {
        if (count == n) {
            return probe;
        }
        count++;
    }
    if (count == n) {
        return probe;
    }
    return NULL;
}

/*
// returns the context that a physical device belongs to
// returns NULL if no context is found
// this is linear but should be fine as most linked lists are not going to exceed 10 items
*/
cl_context getMapping(DANA_COMP* danaComp, cl_device_id id) {
    DC_MAP* probe = danaComp->map;
    for (probe; probe->next != NULL; probe = probe->next) {
        if (probe->device == id) {
            
            return probe->context;
        }
    }
    if (probe->device == id) {
        return probe->context;
    }

    return NULL;
}

/*
 * 1) allocate memory to store platform IDs
 * 2) retrieve platform IDs
 * 3) for each platform, allocate a pointer to an array that will store the
 *      device IDs for that platform, and an array of the same legth that stores
 *      the number of devices on that platform
 * 4) for each platform, get and store the device IDs
 * 5) set the global number of platforms
 */
INSTRUCTION_DEF init(VFrame* cframe) {
    cl_int CL_err = CL_SUCCESS;
    cl_uint numPlatforms = 0;

    if (alreadyInitFlag) {
        DANA_COMP* dana_component_id = (DANA_COMP*) malloc(sizeof(DANA_COMP));
        dana_component_id->contexts = NULL;
        dana_component_id->map = NULL;
        dana_component_id->mapEnd = NULL;
        api->returnInt(cframe, (uint64_t) dana_component_id);
        return RETURN_OK;
    }

    //1
    globalClPlatforms = (cl_platform_id*) malloc(sizeof(cl_platform_id)*MAX_PLATFORMS);
    
    //2
    CL_err = clGetPlatformIDs( MAX_PLATFORMS, globalClPlatforms, &numPlatforms );
    if (CL_err != CL_SUCCESS) {
        printf("platform id broke in init\n");
    }

    
    //3
    devices = (cl_device_id**) malloc(sizeof(cl_device_id*)*numPlatforms);
    numOfDevicesPerPlatform = (cl_uint*) malloc(sizeof(cl_uint)*numPlatforms);
    
    //4
    cl_uint returnNumOfDevices = 0;
    for (int i = 0; i < numPlatforms; i++) {
        *(devices+i) = (cl_device_id*) malloc(sizeof(cl_device_id)*MAX_DEVICES);
        CL_err = clGetDeviceIDs(*(globalClPlatforms+i), CL_DEVICE_TYPE_ALL, MAX_DEVICES, *(devices+i), &returnNumOfDevices);
        if (CL_err != CL_SUCCESS) {
            printf("error in clGetDeviceIDs: %d\n", CL_err);
        }
        *(numOfDevicesPerPlatform+i) = returnNumOfDevices;
    }

    //5
    numofPlatforms = numPlatforms;

    alreadyInitFlag = 1;

    DANA_COMP* dana_component_id = (DANA_COMP*) malloc(sizeof(DANA_COMP));
    dana_component_id->contexts = NULL;
    dana_component_id->map = NULL;
    dana_component_id->mapEnd = NULL;
    api->returnInt(cframe, (uint64_t) dana_component_id);

    return RETURN_OK;
}

INSTRUCTION_DEF getComputeDeviceIDs(VFrame* cframe) {
    cl_int CL_err = CL_SUCCESS;

    if (devices == NULL) {
        printf("no devices found");
        //return empty array
        DanaEl* newArray = api->makeArray(stringArrayGT, 0, NULL);
        api->returnEl(cframe, newArray);
        //exit
        return RETURN_OK;
    }

    //go thru each platform
    int arrSize = 0;
    for (int i = 0; i < numofPlatforms; i++) {
        arrSize += *(numOfDevicesPerPlatform+i);
    }

    //grab each device C handle
    cl_device_id ids[arrSize];
    int seen = 0;
    for (int i = 0; i < numofPlatforms; i++) {
        for (int j = 0; j < *(numOfDevicesPerPlatform+i); j++) {
            ids[seen+j] = *(*(devices+i)+j);
        }
        seen += *(numOfDevicesPerPlatform+i);
    }

    //arrange in a dana array
    DanaEl* returnArray = api->makeArray(intArrayGT, arrSize, NULL);

    for (int i = 0; i < arrSize; i++) {
        api->setArrayCellInt(returnArray, i, (uint64_t) ids[i]);
    }

    //return
    api->returnEl(cframe, returnArray);

    return RETURN_OK;
}

INSTRUCTION_DEF getComputeDevices(VFrame* cframe) {
    cl_int CL_err = CL_SUCCESS;
    if (devices == NULL) {
        printf("no devices found");
        //return empty array
        DanaEl* newArray = api->makeArray(stringArrayGT, 0, NULL);
        api->returnEl(cframe, newArray);
        //exit
        return RETURN_OK;
    }

    //go thru each platform
    int arrSize = 0;
    for (int i = 0; i < numofPlatforms; i++) {
        arrSize += *(numOfDevicesPerPlatform+i);
    }

    //grab each device ID and query opencl for the device name
    char* deviceNames[arrSize];
    int seen = 0;
    for (int i = 0; i < numofPlatforms; i++) {
        for (int j = 0; j < *(numOfDevicesPerPlatform+i); j++) {
            char* buf = (char*) malloc(sizeof(char)*500);
            deviceNames[seen+j] = buf;
            size_t bufReturnSize;
            CL_err = clGetDeviceInfo(*(*(devices+i)+j), CL_DEVICE_NAME, sizeof(char)*500, buf, &bufReturnSize);
            *(deviceNames[seen+j]+bufReturnSize) = '\0';
        }
        seen += *(numOfDevicesPerPlatform+i);
    }

    //arrange device names in a dana array
    DanaEl* returnArray = api->makeArray(stringArrayGT, arrSize, NULL);

    for (int i = 0; i < arrSize; i++) {
        DanaEl* string = api->makeData(stringItemGT);

        size_t sublen = strlen(deviceNames[i]);

        unsigned char* cnt = NULL;
        DanaEl* charArr = api->makeArray(charArrayGT, sublen, &cnt);

        memcpy(cnt, deviceNames[i], sublen);

        api->setDataFieldEl(string, 0, charArr);

        api->setArrayCellEl(returnArray, i, string);
    }

    //return
    api->returnEl(cframe, returnArray);

    return RETURN_OK;
}

/* A context in opencl is a set of physical deviecs, command queues pointing to
 * those devices, kernels and memory objects
 * All devices in a context must belong to the same platform
 * We aim to abstract away this contraint in Dana
 */
INSTRUCTION_DEF createContext(FrameData* cframe) {
    cl_int CL_Err = CL_SUCCESS;

    //input: array of device IDs
    DanaEl* deviceArray = api->getParamEl(cframe, 0);
    cl_device_id deviceHandles[api->getArrayLength(deviceArray)];

    DANA_COMP* danaComp = (DANA_COMP*) api->getParamInt(cframe, 1);

    for (int i = 0; i < api->getArrayLength(deviceArray); i++) {
        deviceHandles[i] = (cl_device_id) api->getArrayCellInt(deviceArray, i);
    }

    /*
        1) for each platform get all the devices that are a part of that platform
        and a part of the devices given as an input to this function
        2) store that intersection in array
        3) create the contex
        4) for each device that is now linked to a context, add the device/context
        mapping to the global list
        TODO: make sure the device ID only goes into the mapping once!
    */
    for (int i = 0; i < numofPlatforms; i++) {
        cl_platform_id platform = *(globalClPlatforms+i);
        cl_device_id deviceHandlesForThisPlat[MAX_DEVICES];
        int deviceForPlatCount = 0;

        //1
        for (int j = 0; j < *(numOfDevicesPerPlatform+i); j++) {
            for (int k = 0; k < api->getArrayLength(deviceArray); k++) {
                if (deviceHandles[k] == *(*(devices+i)+j)) {
                    deviceHandlesForThisPlat[deviceForPlatCount] = deviceHandles[k];
                    deviceForPlatCount++;
                }
            }
        }
        
        if (deviceForPlatCount != 0) {
            CONTEXT_LI* newContextItem = (CONTEXT_LI*) malloc(sizeof(CONTEXT_LI));
            newContextItem->next = NULL;
            newContextItem->platform = platform;

            //2
            //strip the array down to its mimimum size
            cl_device_id deviceHandlesForThisPlatCut[deviceForPlatCount];
            newContextItem->devices = (cl_device_id*) malloc(sizeof(cl_device_id)*deviceForPlatCount);
            newContextItem->numOfDevices = deviceForPlatCount;
            for(int j = 0; j < deviceForPlatCount; j++) {
                deviceHandlesForThisPlatCut[j] = deviceHandlesForThisPlat[j];
                newContextItem->devices[j] = deviceHandlesForThisPlatCut[j];
            }


            //3
            const cl_context_properties props[] = {CL_CONTEXT_PLATFORM, newContextItem->platform, 0};
            newContextItem->context = clCreateContext(props, newContextItem->numOfDevices, newContextItem->devices, NULL, NULL, &CL_Err);
            if (CL_Err != CL_SUCCESS) {
                printf("cl not likey\n");
                printf("code: %d\n", CL_Err);
            }

            //4
            for (int j = 0; j < newContextItem->numOfDevices; j++) {
                DC_MAP* newMapping = (DC_MAP*) malloc(sizeof(DC_MAP));
                newMapping->device = deviceHandlesForThisPlatCut[j];
                newMapping->context = newContextItem->context;
                newMapping->next = NULL;
                if (danaComp->map == NULL) {
                    danaComp->map = newMapping;
                    danaComp->mapEnd = newMapping;
                }
                else {
                    danaComp->mapEnd->next = newMapping;
                    danaComp->mapEnd = newMapping;
                }
            }
            addNewContext(danaComp, newContextItem);
        }
    }

    return RETURN_OK;
}


INSTRUCTION_DEF createAsynchQueue(FrameData* cframe) {
    cl_int CL_err = CL_SUCCESS;
    u_int64_t rawParam = api->getParamInt(cframe, 0);
    cl_device_id device = (cl_device_id) rawParam; 

    DANA_COMP* danaComp = (DANA_COMP*) api->getParamInt(cframe, 1);
    cl_context context = getMapping(danaComp, device);

    const cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, 0};
    cl_command_queue newQ = clCreateCommandQueueWithProperties(context, device, NULL, &CL_err);
    if(CL_err != CL_SUCCESS) {
        printf("err in asynch creation: %d\n", CL_err);
    }

    api->returnInt(cframe, (uint64_t) newQ);
    return RETURN_OK;
}

INSTRUCTION_DEF createSynchQueue(FrameData* cframe) {
    cl_int CL_err = CL_SUCCESS;
    u_int64_t rawParam = api->getParamInt(cframe, 0);

    cl_device_id device = (cl_device_id) rawParam; 

    DANA_COMP* danaComp = (DANA_COMP*) api->getParamInt(cframe, 1);
    cl_context context = getMapping(danaComp, device);

    const cl_queue_properties props[] = {CL_QUEUE_PROPERTIES, CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE, 0};
    cl_command_queue newQ = clCreateCommandQueueWithProperties(context, device, props, &CL_err);
    if(CL_err != CL_SUCCESS) {
        printf("err in asynch creation: %d\n", CL_err);
    }

    api->returnInt(cframe, (uint64_t) newQ);
    return RETURN_OK;
}

INSTRUCTION_DEF createArray(FrameData* cframe) {
    cl_int CL_err = CL_SUCCESS;
    u_int64_t rawParam = api->getParamInt(cframe, 0);
    cl_device_id device = (cl_device_id) rawParam;

    DANA_COMP* danaComp = (DANA_COMP*) api->getParamInt(cframe, 3);
    cl_context context = getMapping(danaComp, device);

    rawParam = api->getParamInt(cframe, 1);
    size_t length = (size_t) rawParam;

    rawParam = api->getParamInt(cframe, 2);
    size_t type = (uint64_t) rawParam;

    size_t size;
    if (type == FLOAT) {
        size = sizeof(float)*length;
    }
    else if (type == UINT) {
        size = sizeof(uint64_t)*length;
    }
    else {
        api->returnInt(cframe, (uint64_t) 0);
        return RETURN_OK;
    }

    cl_mem newArray = clCreateBuffer(context, CL_MEM_READ_WRITE, size, NULL, &CL_err);

    if (CL_err != CL_SUCCESS) {
        printf("issue in array creation\n");
    }

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

    cl_int CL_err = clEnqueueWriteBuffer(queue, memObj, CL_TRUE, 0, hostArrayLen*sizeof(uint64_t), rawHostArray, NULL, NULL, NULL);
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

    cl_int CL_err = clEnqueueReadBuffer(queue, memObj, CL_TRUE, 0, hostArrayLen*sizeof(uint64_t), rawHostArray, NULL, NULL, NULL);
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

INSTRUCTION_DEF writeFloatArray(FrameData* cframe) {
    uint64_t rawParam = api->getParamInt(cframe, 0);
    cl_command_queue queue = (cl_command_queue) rawParam;
    rawParam = api->getParamInt(cframe, 1);
    cl_mem memObj = (cl_mem) rawParam;

    DanaEl* hostArray = api->getParamEl(cframe, 2);
    size_t hostArrayLen = api->getArrayLength(hostArray);

    float* rawHostArray = (float*) malloc(sizeof(float)*hostArrayLen);
    float* rawHostArrayCpy = rawHostArray;
    for (int i = 0; i < hostArrayLen; i++) {
        *rawHostArrayCpy = api->getArrayCellDec(hostArray, i);
        rawHostArrayCpy++;
    }

    cl_int CL_err = clEnqueueWriteBuffer(queue, memObj, CL_TRUE, 0, hostArrayLen*sizeof(float), rawHostArray, NULL, NULL, NULL);
    if (CL_err != CL_SUCCESS) {
        printf("error in write buffer");
    }
    return RETURN_OK;    
}

INSTRUCTION_DEF readFloatArray(FrameData* cframe) {
    uint64_t rawParam = api->getParamInt(cframe, 0);
    cl_command_queue queue = (cl_command_queue) rawParam;
    rawParam = api->getParamInt(cframe, 1);
    cl_mem memObj = (cl_mem) rawParam;

    size_t hostArrayLen = api->getParamInt(cframe, 2);

    float* fromDevice = (float*) malloc(sizeof(float)*hostArrayLen);

    cl_int CL_err = clEnqueueReadBuffer(queue, memObj, CL_TRUE, 0, hostArrayLen*sizeof(float), fromDevice, NULL, NULL, NULL);
    if (CL_err != CL_SUCCESS) {
        printf("error in read buffer");
    }


    DanaEl* danaArr = api->makeArray(decArrayGT, hostArrayLen, NULL);
    for (int i = 0; i < hostArrayLen; i++) {
        api->setArrayCellDec(danaArr, i, *fromDevice);
        fromDevice++;
    }

    api->returnEl(cframe, danaArr);

    return RETURN_OK;    
}

INSTRUCTION_DEF createMatrix(FrameData* cframe) {
    cl_int CL_err = CL_SUCCESS;
    u_int64_t rawParam = api->getParamInt(cframe, 0);
    cl_device_id device = (cl_device_id) rawParam;

    DANA_COMP* danaComp = (DANA_COMP*) api->getParamInt(cframe, 4);
    cl_context context = getMapping(danaComp, device);

    rawParam = api->getParamInt(cframe, 1);
    size_t rows = (size_t) rawParam;

    rawParam = api->getParamInt(cframe, 2);
    size_t cols = (size_t) rawParam;

    rawParam = api->getParamInt(cframe, 3);
    uint64_t type = (uint64_t) rawParam;

    cl_image_desc desc = {CL_MEM_OBJECT_IMAGE2D, cols, rows, 0, 1, 0, 0, 0, 0, NULL};
    cl_image_format form;
    if (type == FLOAT) {
        form = (cl_image_format) {CL_R, CL_FLOAT};
    }
    else if (type == UINT) {
        form = (cl_image_format) {CL_R, CL_UNSIGNED_INT32};
    }
    else {
        api->returnInt(cframe, (uint64_t) 0);
        return RETURN_OK;
    }

    cl_mem newMatrix = clCreateImage(context, CL_MEM_READ_WRITE, &form, &desc, NULL, &CL_err);

    if (CL_err != CL_SUCCESS) {
        printf("issue in matrix creation\n");
    }

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

    uint32_t* rawHostMatrix = (uint32_t*) malloc(sizeof(uint32_t)*dims[0]*dims[1]);
    uint32_t* rawHostMatrixCpy = rawHostMatrix;
    for (int i = 0; i < dims[0]; i++) {
        for (int j = 0; j < dims[1]; j++) {
            *rawHostMatrixCpy = api->getArrayCellInt(hostMatrix, (i*dims[1])+j);
            rawHostMatrixCpy++;
        }
    }

    size_t origin[] = {0, 0, 0};
    size_t region[] = {dims[1], dims[0], 1};
    cl_int CL_err = clEnqueueWriteImage(queue, memObj, CL_TRUE, origin, region, 0, 0, rawHostMatrix, 0, NULL, NULL);

    if (CL_err != CL_SUCCESS) {
        printf("error in write buffer: %d\n", CL_err);
    }
    else {
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

    uint32_t* rawHostMatrix = (uint32_t*) malloc(sizeof(uint32_t)*hostMatrixLens[0]*hostMatrixLens[1]);

    size_t origin[] = {0, 0, 0};
    size_t region[] = {hostMatrixLens[1], hostMatrixLens[0], 1};

    int CL_Err = clEnqueueReadImage(queue, memObj, CL_TRUE, origin, region, 0, 0, rawHostMatrix, 0, NULL, NULL);
    if (CL_Err != CL_SUCCESS) {
        printf("error in read matrix: %d\n", CL_Err);
    }

    for (int i = 0; i < hostMatrixLens[0]; i++) {
        for (int j = 0; j < hostMatrixLens[1]; j++) {
            api->setArrayCellInt(hostMatrix, (i*hostMatrixLens[1])+j, *rawHostMatrix);
            rawHostMatrix++;
        }
    }

    api->returnEl(cframe, hostMatrix);

    return RETURN_OK;    
}

INSTRUCTION_DEF writeFloatMatrix(FrameData* cframe) {
    uint64_t rawParam = api->getParamInt(cframe, 0);
    cl_command_queue queue = (cl_command_queue) rawParam;
    rawParam = api->getParamInt(cframe, 1);
    cl_mem memObj = (cl_mem) rawParam;

    DanaEl* hostMatrix = api->getParamEl(cframe, 2);
    size_t dim = 2; //only supporting 2d matricies
    size_t* dims = api->getArrayDimensions(hostMatrix, &dim);

    float_t* rawHostMatrix = (float_t*) malloc(sizeof(float_t)*dims[0]*dims[1]);
    float_t* rawHostMatrixCpy = rawHostMatrix;
    for (int i = 0; i < dims[0]; i++) {
        for (int j = 0; j < dims[1]; j++) {
            *rawHostMatrixCpy = api->getArrayCellDec(hostMatrix, (i*dims[1])+j);
            rawHostMatrixCpy++;
        }
    }

    size_t origin[] = {0, 0, 0};
    size_t region[] = {dims[1], dims[0], 1};
    cl_int CL_err = clEnqueueWriteImage(queue, memObj, CL_TRUE, origin, region, 0, 0, rawHostMatrix, 0, NULL, NULL);

    if (CL_err != CL_SUCCESS) {
        printf("error in write buffer: %d\n", CL_err);
    }
    else {
    }
    return RETURN_OK;    
}

INSTRUCTION_DEF readFloatMatrix(FrameData* cframe) {
    uint64_t rawParam = api->getParamInt(cframe, 0);
    cl_command_queue queue = (cl_command_queue) rawParam;
    rawParam = api->getParamInt(cframe, 1);
    cl_mem memObj = (cl_mem) rawParam;

    DanaEl* hostMatrix = api->getParamEl(cframe, 2);
    size_t dim = 2;
    size_t* hostMatrixLens = api->getArrayDimensions(hostMatrix, &dim);

    float_t* rawHostMatrix = (float_t*) malloc(sizeof(float_t)*hostMatrixLens[0]*hostMatrixLens[1]);

    size_t origin[] = {0, 0, 0};
    size_t region[] = {hostMatrixLens[1], hostMatrixLens[0], 1};

    int CL_Err = clEnqueueReadImage(queue, memObj, CL_TRUE, origin, region, 0, 0, rawHostMatrix, 0, NULL, NULL);
    if (CL_Err != CL_SUCCESS) {
        printf("error in read matrix: %d\n", CL_Err);
    }

    for (int i = 0; i < hostMatrixLens[0]; i++) {
        for (int j = 0; j < hostMatrixLens[1]; j++) {
            api->setArrayCellDec(hostMatrix, (i*hostMatrixLens[1])+j, *rawHostMatrix);
            rawHostMatrix++;
        }
    }

    api->returnEl(cframe, hostMatrix);

    return RETURN_OK;    
}

INSTRUCTION_DEF destroyMemoryArea(FrameData* cframe) {
    cl_int CL_err = CL_SUCCESS;
    u_int64_t rawParam = api->getParamInt(cframe, 0);
    cl_mem memObj = (cl_mem) rawParam; 
    //the next line seg faults if memObj has already been released...
    //averting this has been attemted in dana, but I'll leave this note
    //here just in case
    CL_err = clReleaseMemObject(memObj);
    if (CL_err != CL_SUCCESS) {
        printf("error in freeing memory area");
    }
    return RETURN_OK;
}

/*
    * Input: .cl program source code
    * For each platform/context attempt to build the program
    * If fails, print the compile errors
    * Return: Array of built program IDs including 0s for those that failed
    * TODO: could probably rework this to give more info for what platforms failed
    * to build back to dana
*/
INSTRUCTION_DEF createProgram(FrameData* cframe) {
    cl_program* progs = (cl_program*) malloc(sizeof(cl_program)*numofPlatforms);
    cl_int CL_err = CL_SUCCESS;

    char** programStrings = (char**) malloc(sizeof(char*));
    char* programSource = x_getParam_char_array(api, cframe, 0);
    *programStrings = programSource;

    DANA_COMP* danaComp = (DANA_COMP*) api->getParamInt(cframe, 1);

    for (int i = 0; i < getNumOfContexts(danaComp); i++) {
        CONTEXT_LI* contextItem = getContextByIndex(danaComp, i);
        progs[i] = clCreateProgramWithSource(contextItem->context, 1, (const char**) programStrings, NULL, &CL_err);
        if (CL_err != CL_SUCCESS) {
        }

        CL_err = CL_SUCCESS;
        CL_err = clBuildProgram(progs[i], contextItem->numOfDevices, contextItem->devices, NULL, NULL, NULL);
        if (CL_err != CL_SUCCESS) {
            size_t len;
            char buf[2048];
            printf("CL_err = %d\n", CL_err);
            clGetProgramBuildInfo(progs[i], **(devices+i), CL_PROGRAM_BUILD_LOG, sizeof(buf), buf, &len);
            printf("%s\n",buf);
        }
    }

    DanaEl* returnArr = api->makeArray(intArrayGT, getNumOfContexts(danaComp), NULL);
    for (int i = 0; i < getNumOfContexts(danaComp); i++) {
        api->setArrayCellInt(returnArr, i, (uint64_t) progs[i]);
    }

    api->returnEl(cframe, returnArr);


    return RETURN_OK;
}

/*
    * Input: program ID, number of parameters for the program, the opencl
    * memory objects that make up the parameters, the program name
    *
    * call opencl to create kernel
    * if successful call opencl to set kernel parameters
    *
    * if both succesful return kernel ID
    * if not return 0
*/
INSTRUCTION_DEF prepareKernel(FrameData* cframe) {
    uint64_t rawParam = api->getParamInt(cframe, 0);
    cl_program program = (cl_program) rawParam;
    rawParam = api->getParamInt(cframe, 2);
    size_t paramCount = (size_t) rawParam;

    DanaEl* paramArray = api->getParamEl(cframe, 1);
    uint64_t* rawParamArray = (uint64_t*) malloc(sizeof(uint64_t)*paramCount);
    uint64_t* rawParamArrayCpy = rawParamArray;
    for (int i = 0; i < paramCount; i++) {
        *rawParamArrayCpy = (uint64_t) api->getArrayCellInt(paramArray, i);
        rawParamArrayCpy++;
    }

    char* progName = x_getParam_char_array(api, cframe, 3);

    cl_int CL_err = CL_SUCCESS;
    cl_kernel kernel = clCreateKernel(program, progName, &CL_err);

    if (CL_err != CL_SUCCESS) {
        api->returnInt(cframe, (uint64_t) 0);
        printf("issue with kernel creation: %d\n", CL_err);
        return RETURN_OK;
    }

    rawParamArrayCpy = rawParamArray;
    for (int i = 0; i < paramCount; i++) {
        rawParamArrayCpy++;
    }

    CL_err = CL_SUCCESS;
    rawParamArrayCpy = rawParamArray;
    for (int i = 0; i < paramCount; i++) {
        CL_err = clSetKernelArg(kernel, i, sizeof(uint64_t), rawParamArrayCpy);
        if (CL_err != CL_SUCCESS) {
            printf("issue with kernel args: %d\n", CL_err);
            api->returnInt(cframe, (uint64_t) 0);
            return RETURN_OK;
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

    //create an amount of kernel threads that is
    //equivilent to the size and shape of the output
    //vector/matrix of the kernel parameters
    DanaEl* rawOutputDimentions = api->getParamEl(cframe, 2);

    size_t rawArrLen = api->getArrayLength(rawOutputDimentions);
    
    uint64_t* globalWorkers = (uint64_t*) malloc(sizeof(uint64_t)*rawArrLen);
    for(int i = 0; i < rawArrLen; i++) {
        *(globalWorkers+i) = api->getArrayCellInt(rawOutputDimentions, i);
    }

    cl_event* kernel_event = (cl_event*) malloc(sizeof(cl_event));

    cl_int CL_err = CL_SUCCESS;
    CL_err = clEnqueueNDRangeKernel(queue, kernel, rawArrLen, NULL, globalWorkers, NULL, 0, NULL, kernel_event);
    if (CL_err != CL_SUCCESS) {
        printf("error execing kernel: %d\n", CL_err);
    }

    clWaitForEvents(1, kernel_event);

    //clean up
    clReleaseEvent(*kernel_event);
    free(globalWorkers);


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
    setInterfaceFunction("writeFloatArray", writeFloatArray);
    setInterfaceFunction("readFloatArray", readFloatArray);
    setInterfaceFunction("createMatrix", createMatrix);
    setInterfaceFunction("writeIntMatrix", writeIntMatrix);
    setInterfaceFunction("readIntMatrix", readIntMatrix);
    setInterfaceFunction("writeFloatMatrix", writeFloatMatrix);
    setInterfaceFunction("readFloatMatrix", readFloatMatrix);
    setInterfaceFunction("destroyMemoryArea", destroyMemoryArea);
    setInterfaceFunction("createProgram", createProgram);
    setInterfaceFunction("prepareKernel", prepareKernel);
    setInterfaceFunction("runKernel", runKernel);

    charArrayGT = api->resolveGlobalTypeMapping(getTypeDefinition("char[]"));
    stringArrayGT = api->resolveGlobalTypeMapping(getTypeDefinition("String[]"));
    stringItemGT = api->resolveGlobalTypeMapping(getTypeDefinition("String"));
    intArrayGT = api->resolveGlobalTypeMapping(getTypeDefinition("int[]"));
    intMatrixGT = api->resolveGlobalTypeMapping(getTypeDefinition("int[][]"));
    decArrayGT = api->resolveGlobalTypeMapping(getTypeDefinition("dec[]"));

    return getPublicInterface();
}

void unload() {
    api->decrementGTRefCount(charArrayGT);
    api->decrementGTRefCount(stringArrayGT);
    api->decrementGTRefCount(stringItemGT);
    api->decrementGTRefCount(intArrayGT);
    api->decrementGTRefCount(intMatrixGT);
    api->decrementGTRefCount(decArrayGT);
}
