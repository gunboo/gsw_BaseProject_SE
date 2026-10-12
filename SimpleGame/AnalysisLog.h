#pragma once

#include <fstream>
#include <string>
#include <vector>

class Renderer;
class SceneGraph;

// Update callbacks may run more than once between display callbacks.
struct FrameSample
{
	double intervalMs = -1.0;
	double updateCpuMs = 0.0;
	double renderCpuMs = 0.0;
	double swapCpuMs = 0.0;
	double updateWallMs = 0.0;
	double maxUpdateWallMs = 0.0;
	double simulationMs = 0.0;
	unsigned int updates = 0;
	unsigned int clampedUpdates = 0;
};

// Single GLUT thread only. Files are buffered and flushed at summary boundaries.
class AnalysisLog
{
public:
	static AnalysisLog& Get();
	static double NowMs();
	~AnalysisLog();
	void Start();
	void Stop(const std::string& reason);
	bool IsGpuTimingEnabled() const;
	void Event(const std::string& kind, const std::string& detail);
	void RecordFrame(const FrameSample& frame, const Renderer& renderer,
		const SceneGraph& graph, const char* scene, int state);

private:
	AnalysisLog();
	bool OpenDetail();
	void WriteSummary();
	void Flush();

	bool m_Enabled = false;
	bool m_Detail = true;
	bool m_GpuTiming = true;
	std::string m_Directory;
	std::string m_Scene;
	int m_State = -1;
	double m_StartMs = 0.0;
	double m_WindowStartMs = 0.0;
	double m_LastSampleMs = 0.0;
	double m_LastFlushMs = 0.0;
	double m_PreviousLogMs = 0.0;
	unsigned int m_Part = 0;
	unsigned long long m_FrameCount = 0;
	unsigned long long m_SceneFirstFrame = 0;
	unsigned long long m_DetailBytes = 0;
	unsigned long long m_EventBytes = 0;
	unsigned long long m_WindowFrames = 0;
	unsigned long long m_WindowDraws = 0;
	unsigned long long m_WindowUploadBytes = 0;
	unsigned long long m_WindowClamps = 0;
	unsigned long long m_WindowGpuSamples = 0;
	double m_WindowUpdateMs = 0.0;
	double m_WindowRenderMs = 0.0;
	double m_WindowSwapMs = 0.0;
	double m_WindowGpuMs = 0.0;
	double m_WindowLogMs = 0.0;
	unsigned int m_WindowMaxDraws = 0;
	std::vector<double> m_Intervals;
	std::ofstream m_Events;
	std::ofstream m_Frames;
	std::ofstream m_Summary;
};
