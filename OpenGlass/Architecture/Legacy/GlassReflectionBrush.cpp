#include "pch.h"
#include "GlassReflectionBrush.hpp"
#include "GlassKernel.hpp"

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
	return GlassKernel::CalculateReflectionViewport(offset, parallaxIntensity, mirrored, width,
		{ static_cast<float>(scale.width), static_cast<float>(scale.height) });
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
