#ifndef __SYNOPSIS_ENGINE_H__
#define __SYNOPSIS_ENGINE_H__
#include <tchar.h>
#include "yolo_define.h"

#define VSENGINE_API __declspec(dllexport)

typedef enum error_code {
	VS_SUCCESS = 0,
	VS_ERROR_INITIALIZATION_FAILED = 1,
	VS_ERROR_INVALID_HANDLE = 2,
	VS_ERROR_UNKNOWN = 99
}vsCode;

#ifdef __cplusplus
extern "C" {
#endif

typedef void* vsHandle;

// vsCode VSENGINE_API vsInitializeEngine(vsHandle* outHandle, const TCHAR* app_path);
// vsCode VSENGINE_API vsShutdownEngine(vsHandle handle);

// vsCode VSENGINE_API vsOpenVideo(vsHandle handle, int source_id, const TCHAR* url);
// vsCode VSENGINE_API vsCloseVideo(vsHandle handle, int source_id);

vsCode VSENGINE_API vsInitYoloModel(vsHandle* outYolo, const TCHAR* appPath, YoloTask task = YT_DETECT);
vsCode VSENGINE_API vsReleaseYoloModel(vsHandle yoloHandle);
vsCode VSENGINE_API vsDetectObjects(vsHandle yoloHandle, const unsigned char* imgData, int width, int height, int channels, Detection** outDetections, int* outCount);


#ifdef __cplusplus
}
#endif

#endif//__SYNOPSIS_ENGINE_H__