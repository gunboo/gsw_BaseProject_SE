#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

// Stable IEEE-754 depth order without comparison-sort iterator overhead.
// Equal depths (including signed zero) retain transparent submission order.
class PainterOrder
{
public:
	template <typename DepthAt>
	void Sort(std::vector<int>& order, DepthAt depthAt)
	{
		const size_t count = order.size();

		if (count < 2)
		{
			return;
		}

		m_Keys.resize(count);
		m_Scratch.resize(count);
		std::uint32_t* keys = m_Keys.data();
		int* original = order.data();
		size_t histogram[4][256] = {};

		for (size_t i = 0; i < count; ++i)
		{
			const int index = original[i];
			const float depth = depthAt(index);
			std::uint32_t bits;
			static_assert(sizeof(bits) == sizeof(depth), "Depth sorting requires 32-bit float");
			std::memcpy(&bits, &depth, sizeof(bits));
			const std::uint32_t key = depth == 0.0f ? 0x80000000u
				: bits ^ ((bits & 0x80000000u) != 0 ? 0xffffffffu : 0x80000000u);
			keys[index] = key;

			for (int pass = 0; pass < 4; ++pass)
			{
				++histogram[pass][(key >> (pass * 8)) & 255u];
			}
		}

		int* source = original;
		int* destination = m_Scratch.data();

		for (int pass = 0; pass < 4; ++pass)
		{
			size_t* offsets = histogram[pass];
			const unsigned int shift = pass * 8;

			if (offsets[(keys[source[0]] >> shift) & 255u] == count)
			{
				continue;
			}

			size_t total = 0;

			for (int bucket = 0; bucket < 256; ++bucket)
			{
				const size_t size = offsets[bucket];
				offsets[bucket] = total;
				total += size;
			}

			for (size_t i = 0; i < count; ++i)
			{
				const int index = source[i];
				destination[offsets[(keys[index] >> shift) & 255u]++] = index;
			}

			int* swap = source;
			source = destination;
			destination = swap;
		}

		if (source != original)
		{
			std::memcpy(original, source, count * sizeof(int));
		}
	}

private:
	std::vector<std::uint32_t> m_Keys;
	std::vector<int> m_Scratch;
};
