#include "pch.h"
#include "SynopsisEngine.h"

vsCode VSENGINE_API vsInitializeEngine(vsHandle* outHandle, const TCHAR* app_path)
{
	// Placeholder implementation
	*outHandle = reinterpret_cast<vsHandle>(new int(42)); // Dummy handle
	return VS_SUCCESS;
}

vsCode VSENGINE_API vsShutdownEngine(vsHandle handle)
{
	// Placeholder implementation
	delete reinterpret_cast<int*>(handle);
	return VS_SUCCESS;
}

vsCode VSENGINE_API vsOpenVideo(vsHandle handle, int source_id, const TCHAR* url)
{
	// Placeholder implementation
	return VS_SUCCESS;
}

vsCode VSENGINE_API vsCloseVideo(vsHandle handle, int source_id)
{
	// Placeholder implementation
	return VS_SUCCESS;
}

vsCode VSENGINE_API vsInitYoloModel(vsHandle* outYolo, YoloTask task, const TCHAR* modelPath, const TCHAR* confPath)
{
	// Placeholder implementation
	*outYolo = reinterpret_cast<vsHandle>(new int(84)); // Dummy YOLO handle
	return VS_SUCCESS;
}

vsCode VSENGINE_API vsReleaseYoloModel(vsHandle yoloHandle)
{
	// Placeholder implementation
	delete reinterpret_cast<int*>(yoloHandle);
	return VS_SUCCESS;
}