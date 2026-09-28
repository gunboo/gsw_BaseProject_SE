#pragma once

#include <map>
#include <string>
#include <vector>

struct MeshRange
{
	int first;
	int count;
	unsigned int fingerprint;
};

// GPU-ready triangle positions. Runtime placement and colours are instances,
// never part of a mesh key. Handles are local to this cache's lifetime.
class MeshCache
{
public:
	bool Load(const char* path);
	bool Save(const char* path) const;
	int Find(const std::string& name) const;
	int Register(const std::string& name, const float* xy, int count);
	bool PreparePrimitives();
	const MeshRange& Get(int handle) const;
	const std::vector<float>& GetVertices() const;
	int GetCount() const;
	bool IsDirty() const;
	void MarkSaved();

private:
	std::map<std::string, int> m_Names;
	std::vector<MeshRange> m_Ranges;
	std::vector<float> m_Vertices;
	bool m_Dirty = false;
};
