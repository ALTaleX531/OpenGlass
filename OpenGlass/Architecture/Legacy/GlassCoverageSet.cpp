#include "pch.h"
#include "GlassCoverageSet.hpp"

using namespace OpenGlass;
std::unordered_map<const dwmcore::CArrayBasedCoverageSet*, winrt::com_ptr<IGlassCoverageSet>> CArrayBasedGlassCoverageSet::s_map{};

void CArrayBasedGlassCoverageSet::SetDeviceTransform(const dwmcore::CMILMatrix* matrix)
{
	for (auto& zorderedRect : m_array.views())
	{
		zorderedRect.UpdateDeviceRect(matrix);
	}
}
HRESULT CArrayBasedGlassCoverageSet::Add(
	const D2D1_RECT_F& coverage,
	int depth,
	const dwmcore::CMILMatrix* matrix
)
{
	m_array.AddInPlace(coverage, depth, matrix);
	return S_OK;
}

bool CArrayBasedGlassCoverageSet::IsFullyCovered(
	const D2D1_RECT_F& coverage,
	int depth
) const
{
	for (const auto& zorderedRect : m_array.views())
	{
		if (zorderedRect.m_depth >= depth)
		{
			break;
		}

		if (
			!wil::rect_is_empty(zorderedRect.m_transformedRect) &&
			std::abs(wil::rect_height(zorderedRect.m_transformedRect) * wil::rect_width(zorderedRect.m_transformedRect)) > 1.f &&

			coverage.left >= zorderedRect.m_transformedRect.left &&
			coverage.top >= zorderedRect.m_transformedRect.top &&
			coverage.right <= zorderedRect.m_transformedRect.right &&
			coverage.bottom <= zorderedRect.m_transformedRect.bottom
		)
		{
			return true;
		}
	}

	return false;
}

bool CArrayBasedGlassCoverageSet::IsPartiallyCovered(
	const D2D1_RECT_F& coverage,
	int depth
) const
{
	for (const auto& zorderedRect : m_array.views())
	{
		if (zorderedRect.m_depth >= depth)
		{
			break;
		}

		if (
			!wil::rect_is_empty(zorderedRect.m_transformedRect) &&
			std::abs(wil::rect_height(zorderedRect.m_transformedRect) * wil::rect_width(zorderedRect.m_transformedRect)) > 1.f &&

			RectF::DoesIntersectUnsafe(zorderedRect.m_transformedRect, coverage)
		)
		{
			return true;
		}
	}

	return false;
}

bool CArrayBasedGlassCoverageSet::IsVisible(
	const D2D1_RECT_F& coverage,
	const dwmcore::CArrayBasedCoverageSet* occlusionCoverageSet
) const
{
	for (const auto& zorderedRect : m_array.views())
	{
		if (
			D2D1_RECT_F intersectedRect = zorderedRect.m_transformedRect;
			!wil::rect_is_empty(zorderedRect.m_transformedRect) &&
			std::abs(wil::rect_height(zorderedRect.m_transformedRect) * wil::rect_width(zorderedRect.m_transformedRect)) > 1.f &&

			RectF::IntersectUnsafe(intersectedRect, coverage) &&
			!occlusionCoverageSet->IsCovered(intersectedRect, zorderedRect.m_depth)
		)
		{
			return true;
		}
	}

	return false;
}

bool CArrayBasedGlassCoverageSet::IsEmpty() const
{
	return m_array.count == 0;
}

winrt::com_ptr<IGlassCoverageSet> OpenGlass::CArrayBasedGlassCoverageSet::GetOrCreate(const dwmcore::CArrayBasedCoverageSet* coverageSet, bool createIfNecessary)
{
	auto it = s_map.find(coverageSet);

	if (createIfNecessary)
	{
		if (it == s_map.end())
		{
			auto result = s_map.emplace(coverageSet, winrt::make<CArrayBasedGlassCoverageSet>());
			if (result.second == true)
			{
				it = result.first;
			}
		}
	}

	return it == s_map.end() ? nullptr : it->second;
}
void OpenGlass::CArrayBasedGlassCoverageSet::Remove(const dwmcore::CArrayBasedCoverageSet* coverageSet)
{
	auto it = s_map.find(coverageSet);

	if (it != s_map.end())
	{
		s_map.erase(it);
	}
}
void OpenGlass::CArrayBasedGlassCoverageSet::RemoveAll()
{
	s_map.clear();
}
