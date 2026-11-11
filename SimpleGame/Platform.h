#pragma once

#include <string>

namespace Platform
{
	struct MemoryStats
	{
		bool valid = false;
		unsigned long long workingSet = 0;
		unsigned long long privateBytes = 0;
	};

	std::string Environment(const char* name);
	unsigned long ProcessId();
	bool MakeDirectory(const std::string& path);
	std::string LogDirectoryName();
	MemoryStats ProcessMemory();
	bool SelectAssetDirectory(const char* executable);
}
