#include "stdafx.h"
#include "MeshCache.h"

#include <cmath>
#include <fstream>

namespace
{
	const unsigned int CACHE_MAGIC = 0x3148534d;
	const unsigned int CACHE_VERSION = 2;
	const unsigned int MAX_MESHES = 100000;
	const unsigned int MAX_NAME_LENGTH = 256;
	const unsigned int MAX_MESH_VERTICES = 4096;
	const unsigned int MAX_TOTAL_FLOATS = 16000000;
	const int MIN_SEGMENTS = 3;
	const int MAX_SEGMENTS = 48;
	const float TWO_PI = 6.2831853f;

	template<typename T>
	bool Read(std::ifstream& stream, T& value)
	{
		return static_cast<bool>(stream.read(reinterpret_cast<char*>(&value), sizeof(value)));
	}

	template<typename T>
	void Write(std::ofstream& stream, const T& value)
	{
		stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
	}

	unsigned int Fingerprint(const float* xy, int count)
	{
		unsigned int hash = 2166136261u;
		const unsigned char* bytes = reinterpret_cast<const unsigned char*>(xy);

		for (int i = 0; i < count * 2 * static_cast<int>(sizeof(float)); ++i)
		{
			hash = (hash ^ bytes[i]) * 16777619u;
		}

		return hash;
	}
}

bool MeshCache::Load(const char* path)
{
	std::ifstream stream(path, std::ios::binary);
	unsigned int magic = 0;
	unsigned int version = 0;
	unsigned int count = 0;

	if (!Read(stream, magic) || !Read(stream, version) || !Read(stream, count)
		|| magic != CACHE_MAGIC || version != CACHE_VERSION || count > MAX_MESHES)
	{
		return false;
	}

	MeshCache loaded;

	for (unsigned int i = 0; i < count; ++i)
	{
		unsigned int length = 0;
		unsigned int checksum = 0;
		MeshRange range = {};

		if (!Read(stream, length) || length == 0 || length > MAX_NAME_LENGTH)
		{
			return false;
		}

		std::string name(length, '\0');
		stream.read(&name[0], length);

		if (!Read(stream, range.count) || !Read(stream, range.fingerprint) || !Read(stream, checksum)
			|| range.count < 3 || range.count > static_cast<int>(MAX_MESH_VERTICES)
			|| range.count % 3 != 0 || loaded.Find(name) >= 0)
		{
			return false;
		}

		range.first = static_cast<int>(loaded.m_Vertices.size() / 2);
		const size_t first = loaded.m_Vertices.size();
		const size_t end = first + static_cast<size_t>(range.count) * 2;

		if (end > MAX_TOTAL_FLOATS)
		{
			return false;
		}

		loaded.m_Vertices.resize(end);
		stream.read(reinterpret_cast<char*>(&loaded.m_Vertices[first]), (end - first) * sizeof(float));

		if (!stream || checksum != Fingerprint(&loaded.m_Vertices[first], range.count))
		{
			return false;
		}

		for (size_t vertex = first; vertex < end; ++vertex)
		{
			if (!std::isfinite(loaded.m_Vertices[vertex]))
			{
				return false;
			}
		}

		loaded.m_Names[name] = static_cast<int>(loaded.m_Ranges.size());
		loaded.m_Ranges.push_back(range);
	}

	m_Names.swap(loaded.m_Names);
	m_Ranges.swap(loaded.m_Ranges);
	m_Vertices.swap(loaded.m_Vertices);
	m_Dirty = false;

	return true;
}

bool MeshCache::Save(const char* path) const
{
	std::ofstream stream(path, std::ios::binary | std::ios::trunc);
	Write(stream, CACHE_MAGIC);
	Write(stream, CACHE_VERSION);
	Write(stream, static_cast<unsigned int>(m_Names.size()));

	for (const auto& entry : m_Names)
	{
		const MeshRange& range = m_Ranges[entry.second];
		Write(stream, static_cast<unsigned int>(entry.first.size()));
		stream.write(entry.first.data(), entry.first.size());
		Write(stream, range.count);
		Write(stream, range.fingerprint);
		Write(stream, Fingerprint(&m_Vertices[range.first * 2], range.count));
		stream.write(reinterpret_cast<const char*>(&m_Vertices[range.first * 2]),
			static_cast<size_t>(range.count) * 2 * sizeof(float));
	}

	stream.close();

	return !stream.fail();
}

int MeshCache::Find(const std::string& name) const
{
	const auto found = m_Names.find(name);

	return found == m_Names.end() ? -1 : found->second;
}

int MeshCache::Register(const std::string& name, const float* xy, int count)
{
	if (xy == NULL || name.empty() || name.size() > MAX_NAME_LENGTH || count < 3
		|| count > static_cast<int>(MAX_MESH_VERTICES / 3) + 2)
	{
		return -1;
	}

	for (int i = 0; i < count * 2; ++i)
	{
		if (!std::isfinite(xy[i]))
		{
			return -1;
		}
	}

	const unsigned int hash = Fingerprint(xy, count);
	const int existing = Find(name);

	if (existing >= 0 && m_Ranges[existing].fingerprint == hash
		&& m_Ranges[existing].count == (count - 2) * 3)
	{
		return existing;
	}

	if ((existing < 0 && m_Ranges.size() >= MAX_MESHES)
		|| m_Vertices.size() + static_cast<size_t>(count - 2) * 6 > MAX_TOTAL_FLOATS)
	{
		return -1;
	}

	// This is an initialization-only operation, never called by a draw command.
	MeshRange range = { static_cast<int>(m_Vertices.size() / 2), (count - 2) * 3, hash };

	for (int triangle = 1; triangle + 1 < count; ++triangle)
	{
		const int corners[3] = { 0, triangle, triangle + 1 };

		for (int corner : corners)
		{
			m_Vertices.push_back(xy[corner * 2]);
			m_Vertices.push_back(xy[corner * 2 + 1]);
		}
	}

	const int handle = existing >= 0 ? existing : static_cast<int>(m_Ranges.size());

	if (existing >= 0)
	{
		m_Ranges[handle] = range;
	}
	else
	{
		m_Ranges.push_back(range);
	}

	m_Names[name] = handle;
	m_Dirty = true;

	return handle;
}

bool MeshCache::PreparePrimitives()
{
	const float triangle[] = { 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f };
	const float quad[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
	if (Register("primitive/triangle", triangle, 3) < 0 || Register("primitive/quad", quad, 4) < 0)
	{
		return false;
	}

	for (int segments = MIN_SEGMENTS; segments <= MAX_SEGMENTS; ++segments)
	{
		const std::string name = "primitive/ellipse/" + std::to_string(segments);
		const int existing = Find(name);

		if (existing >= 0 && Get(existing).count == (segments - 2) * 3)
		{
			continue;
		}

		float xy[MAX_SEGMENTS * 2];

		for (int i = 0; i < segments; ++i)
		{
			const float angle = TWO_PI * static_cast<float>(i) / static_cast<float>(segments);
			xy[i * 2] = cosf(angle);
			xy[i * 2 + 1] = sinf(angle);
		}

		if (Register(name, xy, segments) < 0)
		{
			return false;
		}
	}

	return true;
}

const MeshRange& MeshCache::Get(int handle) const
{
	return m_Ranges[handle];
}

const std::vector<float>& MeshCache::GetVertices() const
{
	return m_Vertices;
}

int MeshCache::GetCount() const
{
	return static_cast<int>(m_Ranges.size());
}

bool MeshCache::IsDirty() const
{
	return m_Dirty;
}

void MeshCache::MarkSaved()
{
	m_Dirty = false;
}
