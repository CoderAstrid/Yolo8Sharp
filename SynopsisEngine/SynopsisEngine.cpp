#include "pch.h"
#include "SynopsisEngine.h"

#include "third_party\yolo_runner.h"
#include <map>
#include <memory>
#include <mutex>

static std::mutex g_mutex;
static std::map<vsHandle, std::unique_ptr<YoloRunner>> g_instances;

/*
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
*/

vsCode vsInitYoloModel(vsHandle* outYolo, const TCHAR* appPath, YoloTask task)
{
	if (!outYolo || !appPath)
		return VS_ERROR_INVALID_HANDLE;

	

	auto runner = std::make_unique<YoloRunner>();

	if (!runner->Init(task, appPath))
		return VS_ERROR_INITIALIZATION_FAILED;

	vsHandle handle = reinterpret_cast<vsHandle>(runner.get());

	std::lock_guard<std::mutex> lock(g_mutex);
	g_instances[handle] = std::move(runner);
	*outYolo = handle;

	return VS_SUCCESS;
}

vsCode VSENGINE_API vsReleaseYoloModel(vsHandle yoloHandle)
{
	std::lock_guard<std::mutex> lock(g_mutex);
	auto it = g_instances.find(yoloHandle);
	if (it == g_instances.end())
		return VS_ERROR_INVALID_HANDLE;

	g_instances.erase(it);
	return VS_SUCCESS;
}