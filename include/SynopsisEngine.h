#ifndef __SYNOPSIS_ENGINE_H__
#define __SYNOPSIS_ENGINE_H__
#include <tchar.h>

#define VSENGINE_API __declspec(dllexport)

typedef enum error_code {
	VS_SUCCESS = 0,
	VS_ERROR_INITIALIZATION_FAILED = 1,
	VS_ERROR_INVALID_HANDLE = 2,
	VS_ERROR_UNKNOWN = 99
}vsCode;

typedef enum YoloTask {
	YOLO_TASK_DETECT = 0,
	YOLO_TASK_CLASSIFY,
	YOLO_TASK_SEGMENT,
	YOLO_TASK_POSE,
	YOLO_TASK_OBB,
	YOLO_TASK_MAX
}yoloTask;

#ifdef __cplusplus
extern "C" {
#endif

typedef void* vsHandle;

vsCode VSENGINE_API vsInitializeEngine(vsHandle* outHandle, const TCHAR* app_path);
vsCode VSENGINE_API vsShutdownEngine(vsHandle handle);

vsCode VSENGINE_API vsOpenVideo(vsHandle handle, int source_id, const TCHAR* url);
vsCode VSENGINE_API vsCloseVideo(vsHandle handle, int source_id);

vsCode VSENGINE_API vsInitYoloModel(vsHandle* outYolo, YoloTask task, const TCHAR* modelPath, const TCHAR* confPath);
vsCode VSENGINE_API vsReleaseYoloModel(vsHandle yoloHandle);

#ifdef __cplusplus
}
#endif

#endif//__SYNOPSIS_ENGINE_H__