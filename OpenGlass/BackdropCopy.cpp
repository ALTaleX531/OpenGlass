#include "pch.h"
#include "BackdropCopy.hpp"

using namespace OpenGlass;

void OpenGlass::CoalesceCopyRectangles(std::vector<D2D1_RECT_U>& rectangles)
{
	if (rectangles.empty())
	{
		return;
	}

	const auto area = [](const D2D1_RECT_U& rect)
	{
		return static_cast<uint64_t>(rect.right - rect.left) * (rect.bottom - rect.top);
	};
	const auto unite = [](const D2D1_RECT_U& a, const D2D1_RECT_U& b)
	{
		return D2D1_RECT_U
		{
			std::min(a.left, b.left), std::min(a.top, b.top),
			std::max(a.right, b.right), std::max(a.bottom, b.bottom)
		};
	};

	auto bounds = rectangles.front();
	size_t count{};
	for (const auto rectangle : rectangles)
	{
		bounds = unite(bounds, rectangle);
		uint64_t bestSaving{};
		auto mergeIndex = count;
		for (size_t i = 0; i < count; ++i)
		{
			const auto& existing = rectangles[i];
			// Coalesce touching or overlapping footprints.
			if (existing.left > rectangle.right || existing.top > rectangle.bottom ||
				existing.right < rectangle.left || existing.bottom < rectangle.top)
			{
				continue;
			}
			const auto separateCost = area(existing) + area(rectangle) + CopyCallTexelCost;
			const auto mergedCost = area(unite(existing, rectangle));
			if (mergedCost < separateCost && separateCost - mergedCost > bestSaving)
			{
				bestSaving = separateCost - mergedCost;
				mergeIndex = i;
			}
		}
		if (mergeIndex < count)
		{
			rectangles[mergeIndex] = unite(rectangles[mergeIndex], rectangle);
		}
		else
		{
			rectangles[count++] = rectangle;
		}
	}
	rectangles.resize(count);
	uint64_t copyCost = CopyCallTexelCost * count;
	for (const auto& rect : rectangles)
	{
		copyCost += area(rect);
	}
	if (area(bounds) + CopyCallTexelCost <= copyCost)
	{
		rectangles.assign(1, bounds);
	}
}
