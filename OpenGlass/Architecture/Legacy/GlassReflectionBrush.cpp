#include "pch.h"
#include "GlassReflectionBrush.hpp"

using namespace OpenGlass;
namespace OpenGlass::GlassReflectionBrush
{
	std::unordered_map<void*, std::array<winrt::com_ptr<uDWM::CImageLegacyMilBrushProxy>, 4>> g_glassReflectionBrushMap{};
}

D2D1_RECT_F GlassReflectionBrush::CalculateTargetViewport(
	const POINT& offset,
	float parallaxIntensity,
	bool mirrored,
	LONG width,
	const DWM::MilSizeD& scale
)
{
	D2D1_RECT_F viewport{};
	LONG left = offset.x;
	if (mirrored)
	{
		left = GetSystemMetrics(SM_XVIRTUALSCREEN) + (GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN)) - (offset.x + width);
	}
	viewport.left = (GetSystemMetrics(SM_XVIRTUALSCREEN) - left) + ((left + width / 2.f) - (GetSystemMetrics(SM_XVIRTUALSCREEN) + GetSystemMetrics(SM_CXVIRTUALSCREEN) / 2.f)) * parallaxIntensity;
	viewport.top = static_cast<float>(
		GetSystemMetrics(SM_YVIRTUALSCREEN) -
		offset.y
	);

	viewport.right = viewport.left + static_cast<float>(GetSystemMetrics(SM_CXVIRTUALSCREEN));
	viewport.bottom = viewport.top + static_cast<float>(GetSystemMetrics(SM_CYVIRTUALSCREEN));

	viewport.left /= static_cast<float>(scale.width);
	viewport.top /= static_cast<float>(scale.height);
	viewport.right /= static_cast<float>(scale.width);
	viewport.bottom /= static_cast<float>(scale.height);

	return viewport;
}

winrt::com_ptr<uDWM::CImageLegacyMilBrushProxy> GlassReflectionBrush::GetOrCreate(
	void* owner,
	unsigned int slot,
	bool createIfNecessary
)
{
	auto& array = g_glassReflectionBrushMap.try_emplace(owner, std::array<winrt::com_ptr<uDWM::CImageLegacyMilBrushProxy>, 4>{}).first->second;
	auto& brush = array.at(slot);

	if (!brush && createIfNecessary)
	{
		THROW_IF_FAILED(
			uDWM::CDesktopManager::GetInstance()->GetCompositor()->CreateImageLegacyMilBrushProxy(
				brush.put()
			)
		);
	}

	return brush;
}
void GlassReflectionBrush::Remove(void* owner)
{
	auto it = g_glassReflectionBrushMap.find(owner);

	if (it != g_glassReflectionBrushMap.end())
	{
		g_glassReflectionBrushMap.erase(it);
	}
}
void GlassReflectionBrush::RemoveAll()
{
	g_glassReflectionBrushMap.clear();
}
