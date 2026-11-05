#pragma once
#include <d2d1.h>
#include <cstdint>
#include <vector>

namespace OpenGlass
{
	// Transfer-area equivalent of one CopySubresourceRegion command.
	inline constexpr uint64_t CopyCallTexelCost = 32 * 1024;

	// Both realizers expand and clip disjoint draw rectangles. Source boxes can overlap.
	void CoalesceCopyRectangles(std::vector<D2D1_RECT_U>& rectangles);
}
