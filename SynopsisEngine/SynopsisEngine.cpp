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

vsCode vsDetectObjects(vsHandle yoloHandle, const unsigned char* imgData, int width, int height, int channels, Detection** outDetections, int* outCount)
{
	if (!yoloHandle || !imgData || width <= 0 || height <= 0 || channels <= 0 || !outDetections || !outCount)
		return VS_ERROR_INVALID_HANDLE;

	std::lock_guard<std::mutex> lock(g_mutex);
	auto it = g_instances.find(yoloHandle);
	if (it == g_instances.end())
		return VS_ERROR_INVALID_HANDLE;

	YoloRunner* runner = it->second.get();

	cv::Mat img;
	if (channels == 3) {
		img = cv::Mat(height, width, CV_8UC3, const_cast<unsigned char*>(imgData)).clone();
	}
	else if (channels == 4) {
		img = cv::Mat(height, width, CV_8UC4, const_cast<unsigned char*>(imgData)).clone();
	}
	else if (channels == 1) {
		img = cv::Mat(height, width, CV_8UC1, const_cast<unsigned char*>(imgData)).clone();
	}
	else {
		return VS_ERROR_INVALID_HANDLE;
	}

	std::vector<Detection> detections = runner->runDetect(img);

	*outCount = static_cast<int>(detections.size());
	if (*outCount > 0) {
		*outDetections = new Detection[*outCount];
		for (int i = 0; i < *outCount; ++i) {
			(*outDetections)[i] = detections[i];
		}
	}
	else {
		*outDetections = nullptr;
	}

	return VS_SUCCESS;
}