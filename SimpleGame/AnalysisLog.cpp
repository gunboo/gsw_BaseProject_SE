#include "stdafx.h"
#include "AnalysisLog.h"
#include "Renderer.h"
#include "SceneGraph.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Psapi.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <locale>
#include <numeric>
#include <sstream>

#pragma comment(lib, "Psapi.lib")

namespace
{
	const double SUMMARY_INTERVAL_MS = 1000.0;
	const unsigned long long DETAIL_PART_BYTES = 32ull * 1024 * 1024;
	const unsigned int MAX_DETAIL_PARTS = 8;
	const unsigned long long MAX_EVENT_BYTES = 8ull * 1024 * 1024;
	const double SLOW_FRAME_MS = 1000.0 / 30.0;

	std::string Setting(const char* name)
	{
		char* value = NULL;
		size_t length = 0;
		_dupenv_s(&value, &length, name);
		const std::string result = value != NULL ? value : "";
		free(value);

		return result;
	}

	std::string Csv(const std::string& value)
	{
		std::string result = "\"";

		for (char character : value)
		{
			if (character == '"')
			{
				result += '"';
			}

			result += character;
		}

		return result + '"';
	}

	void CountGraph(const Actor& actor, size_t depth, size_t& total, size_t& maxDepth)
	{
		++total;
		maxDepth = (std::max)(maxDepth, depth);

		for (size_t i = 0; i < actor.GetChildCount(); ++i)
		{
			CountGraph(*actor.GetChild(i), depth + 1, total, maxDepth);
		}
	}

	double Percentile(const std::vector<double>& sorted, double fraction)
	{
		if (sorted.empty())
		{
			return -1.0;
		}

		const size_t index = static_cast<size_t>(ceil(fraction * sorted.size())) - 1;

		return sorted[(std::min)(index, sorted.size() - 1)];
	}
}

AnalysisLog& AnalysisLog::Get()
{
	static AnalysisLog instance;

	return instance;
}

double AnalysisLog::NowMs()
{
	return std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

AnalysisLog::AnalysisLog()
{
	m_Intervals.reserve(4096);
}

AnalysisLog::~AnalysisLog()
{
	Stop("process_exit");
}

void AnalysisLog::Start()
{
	if (m_Enabled || Setting("GAME_ANALYSIS") == "0")
	{
		return;
	}

	m_Detail = Setting("GAME_ANALYSIS_DETAIL") != "0";
	m_GpuTiming = Setting("GAME_ANALYSIS_GPU") != "0";
	CreateDirectoryA("./Data/Logs", NULL);
	SYSTEMTIME utc;
	GetSystemTime(&utc);
	char name[128];
	sprintf_s(name, "./Data/Logs/%04u%02u%02u_%02u%02u%02u_%03u_%lu",
		utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond,
		utc.wMilliseconds, GetCurrentProcessId());
	m_Directory = name;

	if (!CreateDirectoryA(m_Directory.c_str(), NULL))
	{
		std::cerr << "Analysis logs disabled: cannot create " << m_Directory << "\n";
		return;
	}

	m_Events.open(m_Directory + "/events.csv", std::ios::binary);
	m_Summary.open(m_Directory + "/summary.csv", std::ios::binary);
	m_Events.imbue(std::locale::classic());
	m_Summary.imbue(std::locale::classic());

	if (!m_Events || !m_Summary)
	{
		std::cerr << "Analysis logs disabled: file open failed.\n";
		return;
	}

	m_StartMs = NowMs();
	m_WindowStartMs = m_StartMs;
	m_LastSampleMs = m_StartMs;
	m_Enabled = true;
	m_Events << "schema,elapsed_ms,kind,detail\n";
	m_Summary << "schema,start_ms,end_ms,scene,state,frames,interval_samples,fps,frame_mean_ms,frame_p50_ms,"
		"frame_p95_ms,frame_p99_ms,frame_max_ms,over_33_33ms,update_mean_ms,render_mean_ms,swap_mean_ms,"
		"draw_mean,draw_max,upload_mean_bytes,clamped_updates,gpu_samples,gpu_mean_ms,logger_mean_ms\n";

	if (m_Detail && !OpenDetail())
	{
		m_Detail = false;
		Event("detail.stopped", "file_open_failed; summaries_continue");
	}

	std::ofstream metadata(m_Directory + "/session.txt", std::ios::binary);
	metadata << "schema=1\nclock=steady_clock\nfolder_timestamp=UTC\nbuild=" << __DATE__ << ' ' << __TIME__
		<< "\npointer_bits=" << sizeof(void*) * 8 << "\nlabel=" << Csv(Setting("GAME_ANALYSIS_LABEL"))
		<< "\nrevision=" << Csv(Setting("GAME_ANALYSIS_REVISION")) << "\ndetail=" << m_Detail
		<< "\ngpu_timer=" << m_GpuTiming << "\n";
#ifdef _DEBUG
	metadata << "configuration=Debug\n";
#else
	metadata << "configuration=Release\n";
#endif
	metadata.flush();

	if (!metadata)
	{
		Event("metadata.failed", "session_file_write_failed");
	}

	Event("session.start", "detail=" + std::to_string(m_Detail) + ";gpu_timer=" + std::to_string(m_GpuTiming));
	Flush();
	std::cout << "[Analysis] " << m_Directory << "\n";
}

bool AnalysisLog::IsGpuTimingEnabled() const
{
	return m_Enabled && m_GpuTiming;
}

bool AnalysisLog::OpenDetail()
{
	m_Frames.close();
	m_Frames.clear();
	char filename[48];
	sprintf_s(filename, "/frames_%03u.csv", m_Part);
	m_Frames.open(m_Directory + filename, std::ios::binary);
	m_Frames.imbue(std::locale::classic());
	m_DetailBytes = 0;
	m_Frames << "schema,frame,elapsed_ms,scene,state,width,height,frame_interval_ms,update_calls,update_wall_sum_ms,"
		"update_wall_max_ms,simulation_sum_ms,clamped_updates,update_cpu_ms,render_cpu_ms,swap_cpu_ms,"
		"sort_cpu_ms,world_submit_cpu_ms,text_submit_cpu_ms,world_draws,text_draws,polygons,triangles,texts,"
		"instance_upload_bytes,visited_nodes,culled_branches,submitted_actors,text_cache_hits_total,"
		"text_cache_misses_total,text_cache_resets_total,text_cache_entries,atlas_count,atlas_storage_bytes,"
		"gpu_source_frame,gpu_elapsed_ms,gpu_query_skipped,previous_logger_cpu_ms\n";

	return static_cast<bool>(m_Frames);
}

void AnalysisLog::Event(const std::string& kind, const std::string& detail)
{
	if (!m_Enabled || (m_EventBytes >= MAX_EVENT_BYTES && kind != "session.stop"))
	{
		return;
	}

	std::ostringstream line;
	line.imbue(std::locale::classic());
	line << "1," << std::fixed << std::setprecision(3) << NowMs() - m_StartMs
		<< ',' << Csv(kind) << ',' << Csv(detail) << '\n';
	const std::string value = line.str();
	m_Events << value;
	m_EventBytes += value.size();

	if (m_EventBytes >= MAX_EVENT_BYTES && kind != "session.stop")
	{
		m_Events << "1," << NowMs() - m_StartMs
			<< ",\"events.stopped\",\"size_limit; summaries_continue\"\n";
	}
}

void AnalysisLog::RecordFrame(const FrameSample& frame, const Renderer& renderer,
	const SceneGraph& graph, const char* scene, int state)
{
	if (!m_Enabled)
	{
		return;
	}

	const double begin = NowMs();
	const RenderStats& render = renderer.GetStats();
	const bool changed = m_Scene != scene || m_State != state;

	if (changed)
	{
		WriteSummary();
		m_Scene = scene;
		m_State = state;
		m_SceneFirstFrame = render.frameId;
		Event("scene.state", m_Scene + ";state=" + std::to_string(state));
	}

	const TextCacheStats text = renderer.GetTextCacheStats();
	const unsigned int draws = render.worldDrawCalls + render.textDrawCalls;
	++m_FrameCount;
	++m_WindowFrames;
	m_WindowDraws += draws;
	m_WindowMaxDraws = (std::max)(m_WindowMaxDraws, draws);
	m_WindowUploadBytes += render.instanceUploadBytes;
	m_WindowClamps += frame.clampedUpdates;
	m_WindowUpdateMs += frame.updateCpuMs;
	m_WindowRenderMs += frame.renderCpuMs;
	m_WindowSwapMs += frame.swapCpuMs;
	m_WindowLogMs += m_PreviousLogMs;

	if (frame.intervalMs >= 0.0 && !changed)
	{
		m_Intervals.push_back(frame.intervalMs);
	}

	// Delayed results from the previous scene/state do not belong to this summary.
	if (render.gpuElapsedMs >= 0.0 && render.gpuSourceFrame >= m_SceneFirstFrame)
	{
		++m_WindowGpuSamples;
		m_WindowGpuMs += render.gpuElapsedMs;
	}

	if (m_Detail)
	{
		std::ostringstream line;
		line.imbue(std::locale::classic());
		line << std::fixed << std::setprecision(3) << "1," << render.frameId << ',' << begin - m_StartMs
			<< ',' << scene << ',' << state << ',' << renderer.GetWidth() << ',' << renderer.GetHeight()
			<< ',' << frame.intervalMs << ',' << frame.updates << ',' << frame.updateWallMs
			<< ',' << frame.maxUpdateWallMs << ',' << frame.simulationMs << ',' << frame.clampedUpdates
			<< ',' << frame.updateCpuMs << ',' << frame.renderCpuMs << ',' << frame.swapCpuMs
			<< ',' << render.sortCpuMs << ',' << render.worldSubmitCpuMs << ',' << render.textSubmitCpuMs
			<< ',' << render.worldDrawCalls << ',' << render.textDrawCalls << ',' << render.polygonInstances
			<< ',' << render.triangleInstances << ',' << render.textInstances << ',' << render.instanceUploadBytes
			<< ',' << graph.GetVisitedActorCount() << ',' << graph.GetCulledActorCount()
			<< ',' << graph.GetSubmittedActorCount() << ',' << text.hits << ',' << text.misses << ',' << text.resets
			<< ',' << text.entries << ',' << text.atlases << ',' << text.storageBytes
			<< ',' << render.gpuSourceFrame << ',' << render.gpuElapsedMs << ',' << render.gpuQuerySkipped
			<< ',' << m_PreviousLogMs << '\n';
		const std::string value = line.str();
		m_Frames << value;
		m_DetailBytes += value.size();

		if (!m_Frames || m_DetailBytes >= DETAIL_PART_BYTES)
		{
			++m_Part;
			m_Detail = m_Frames.good() && m_Part < MAX_DETAIL_PARTS && OpenDetail();

			if (!m_Detail)
			{
				m_Frames.close();
				Event("detail.stopped", "file_limit_or_io_error; summaries_continue");
			}
		}
	}

	if (changed || begin - m_LastSampleMs >= SUMMARY_INTERVAL_MS)
	{
		size_t actors = 0;
		size_t depth = 0;
		CountGraph(graph.GetRoot(), 0, actors, depth);
		PROCESS_MEMORY_COUNTERS_EX memory = {};
		memory.cb = sizeof(memory);
		const bool memoryValid = GetProcessMemoryInfo(GetCurrentProcess(),
			reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != 0;
		Event("resources", "scene=" + m_Scene + ";actors=" + std::to_string(actors)
			+ ";max_depth=" + std::to_string(depth) + ";memory_valid=" + std::to_string(memoryValid)
			+ ";working_set_bytes=" + std::to_string(memory.WorkingSetSize)
			+ ";private_bytes=" + std::to_string(memory.PrivateUsage));
		m_LastSampleMs = begin;
	}

	if (begin - m_WindowStartMs >= SUMMARY_INTERVAL_MS)
	{
		WriteSummary();
	}

	if (begin - m_LastFlushMs >= SUMMARY_INTERVAL_MS)
	{
		Flush();
	}

	m_PreviousLogMs = NowMs() - begin;
}

void AnalysisLog::WriteSummary()
{
	const double now = NowMs();

	if (m_WindowFrames == 0)
	{
		m_WindowStartMs = now;
		return;
	}

	std::sort(m_Intervals.begin(), m_Intervals.end());
	const double sum = std::accumulate(m_Intervals.begin(), m_Intervals.end(), 0.0);
	const double mean = m_Intervals.empty() ? -1.0 : sum / m_Intervals.size();
	const size_t slow = static_cast<size_t>(std::count_if(m_Intervals.begin(), m_Intervals.end(),
		[](double value)
		{
			return value > SLOW_FRAME_MS;
		}));
	m_Summary << std::fixed << std::setprecision(3) << "1," << m_WindowStartMs - m_StartMs
		<< ',' << now - m_StartMs << ',' << m_Scene << ',' << m_State << ',' << m_WindowFrames
		<< ',' << m_Intervals.size() << ',' << (mean > 0.0 ? 1000.0 / mean : -1.0) << ',' << mean
		<< ',' << Percentile(m_Intervals, 0.50) << ',' << Percentile(m_Intervals, 0.95)
		<< ',' << Percentile(m_Intervals, 0.99) << ',' << (m_Intervals.empty() ? -1.0 : m_Intervals.back())
		<< ',' << slow << ',' << m_WindowUpdateMs / m_WindowFrames << ',' << m_WindowRenderMs / m_WindowFrames
		<< ',' << m_WindowSwapMs / m_WindowFrames << ',' << static_cast<double>(m_WindowDraws) / m_WindowFrames
		<< ',' << m_WindowMaxDraws << ',' << static_cast<double>(m_WindowUploadBytes) / m_WindowFrames
		<< ',' << m_WindowClamps << ',' << m_WindowGpuSamples
		<< ',' << (m_WindowGpuSamples == 0 ? -1.0 : m_WindowGpuMs / m_WindowGpuSamples)
		<< ',' << m_WindowLogMs / m_WindowFrames << '\n';
	m_WindowFrames = 0;
	m_WindowDraws = 0;
	m_WindowUploadBytes = 0;
	m_WindowClamps = 0;
	m_WindowGpuSamples = 0;
	m_WindowUpdateMs = 0.0;
	m_WindowRenderMs = 0.0;
	m_WindowSwapMs = 0.0;
	m_WindowGpuMs = 0.0;
	m_WindowLogMs = 0.0;
	m_WindowMaxDraws = 0;
	m_Intervals.clear();
	m_WindowStartMs = now;
}

void AnalysisLog::Flush()
{
	m_LastFlushMs = NowMs();
	m_Events.flush();
	m_Summary.flush();

	if (m_Detail)
	{
		m_Frames.flush();
	}

	if (!m_Events || !m_Summary)
	{
		std::cerr << "Analysis logs disabled after an I/O error.\n";
		m_Enabled = false;
	}
}

void AnalysisLog::Stop(const std::string& reason)
{
	if (!m_Enabled)
	{
		return;
	}

	WriteSummary();
	Event("session.stop", reason + ";frames=" + std::to_string(m_FrameCount));
	Flush();
	m_Frames.close();
	m_Events.close();
	m_Summary.close();
	m_Enabled = false;
}
