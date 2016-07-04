//
// Created by svalov on 7/2/16.
//

#ifndef MOARSTACK_LIBINTERFACE_H
#define MOARSTACK_LIBINTERFACE_H

#include "stdint.h"

#define MOAR_API_CURRENT_VERSION 1 //should be incremented on every not back compatible change of crosslayer api
#define MOAR_LIBRARY_INFO_OK 0
#define MOAR_LIBRARY_INFO_FAILED 1

typedef enum{
    MoarLayer_Interface,
    MoarLayer_Channel,
    MoarLayer_Routing,
    MoarLayer_Router,
    MoarLayer_Presentation,
    MoarLayer_Service,
    MoarLayer_LayersCount,
} MoarLayerType_T;

typedef struct{
    char* Author;
    char* LibraryName;
    char* ShortDescription;
    uint16_t VersionMajor;
    uint16_t VersionMinor;
    uint16_t VersionBuild;
    uint16_t TargetMoarApiVersion; // version of build moar shared library
    MoarLayerType_T LayerType;
} MoarLibInfo_T;


typedef int (*moarLibraryInfo_F)(MoarLibInfo_T* libInfo);

#ifdef __cplusplus
extern "C" {
#endif

extern int MoarLibraryInfo(MoarLibInfo_T* libInfo);

#ifdef __cplusplus
};
#endif

#endif //MOARSTACK_LIBINTERFACE_H
