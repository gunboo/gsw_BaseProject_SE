#include "stdafx.h"
#include "Platform.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>

#ifdef _WIN32
#include <Windows.h>
#include <Psapi.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <mach/mach.h>
#endif
#endif

std::string Platform::Environment(const char* name)
{
	const char* value = std::getenv(name);

	return value != NULL ? value : "";
}

unsigned long Platform::ProcessId()
{
#ifdef _WIN32
	return GetCurrentProcessId();
#else
	return static_cast<unsigned long>(getpid());
#endif
}

bool Platform::MakeDirectory(const std::string& path)
{
#ifdef _WIN32
	return CreateDirectoryA(path.c_str(), NULL) != 0;
#else
	return mkdir(path.c_str(), 0755) == 0;
#endif
}

std::string Platform::LogDirectoryName()
{
	const auto now = std::chrono::system_clock::now();
	const std::time_t seconds = std::chrono::system_clock::to_time_t(now);
	const long long milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
	std::tm utc = {};
#ifdef _WIN32
	gmtime_s(&utc, &seconds);
#else
	gmtime_r(&seconds, &utc);
#endif
	char name[128];
	std::snprintf(name, sizeof(name), "./Data/Logs/%04d%02d%02d_%02d%02d%02d_%03lld_%lu",
		utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec, milliseconds, ProcessId());

	return name;
}

Platform::MemoryStats Platform::ProcessMemory()
{
	MemoryStats result;
#ifdef _WIN32
	PROCESS_MEMORY_COUNTERS_EX memory = {};
	memory.cb = sizeof(memory);
	result.valid = GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memory), sizeof(memory)) != 0;
	result.workingSet = memory.WorkingSetSize;
	result.privateBytes = memory.PrivateUsage;
#elif defined(__linux__)
	std::ifstream status("/proc/self/statm");
	unsigned long long total = 0;
	unsigned long long resident = 0;
	unsigned long long shared = 0;
	result.valid = static_cast<bool>(status >> total >> resident >> shared);
	const long page = sysconf(_SC_PAGESIZE);
	result.workingSet = resident * page;
	result.privateBytes = (resident > shared ? resident - shared : 0) * page;
#elif defined(__APPLE__)
	mach_task_basic_info_data_t info = {};
	mach_msg_type_number_t size = MACH_TASK_BASIC_INFO_COUNT;
	result.valid = task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &size) == KERN_SUCCESS;
	result.workingSet = result.privateBytes = info.resident_size;
#endif

	return result;
}

bool Platform::SelectAssetDirectory(const char* executable)
{
#ifdef _WIN32
	wchar_t path[32768];
	const DWORD length = GetModuleFileNameW(NULL, path, 32768);

	if (length > 0 && length < 32768)
	{
		const std::wstring file(path, length);
		const std::wstring folder = file.substr(0, file.find_last_of(L"/\\"));

		if (GetFileAttributesW((folder + L"/Data/dialogue.txt").c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			return SetCurrentDirectoryW(folder.c_str()) != 0;
		}
	}
#else
	char path[4096] = {};
	std::string file;
#ifdef __APPLE__
	uint32_t size = sizeof(path);

	if (_NSGetExecutablePath(path, &size) == 0)
	{
		file = path;
	}
#else
	const ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1);

	if (length > 0)
	{
		file.assign(path, static_cast<size_t>(length));
	}
#endif

	if (file.empty() && executable != NULL)
	{
		file = executable;
	}

	const std::string folder = file.substr(0, file.find_last_of('/'));

	if (std::ifstream(folder + "/Data/dialogue.txt").good())
	{
		return chdir(folder.c_str()) == 0;
	}
#endif

	return std::ifstream("./Data/dialogue.txt").good() && std::ifstream("./Shaders/Batch.vs").good();
}
