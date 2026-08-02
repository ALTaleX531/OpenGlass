#include "pch.h"
#include "GlassEffectBrush.hpp"

using namespace OpenGlass;
namespace OpenGlass::GlassEffectBrush
{
	std::unordered_map<uDWM::CTopLevelWindow*, winrt::com_ptr<uDWM::CSolidColorLegacyMilBrushProxy>> g_glassEffectBrushMap{};
}

winrt::com_ptr<uDWM::CSolidColorLegacyMilBrushProxy> GlassEffectBrush::GetOrCreate(uDWM::CTopLevelWindow* window, bool createIfNecessary)
{
	if (createIfNecessary)
	{
		auto& glassBrush = g_glassEffectBrushMap[window];
		if (!glassBrush)
		{
			THROW_IF_FAILED(
				uDWM::CDesktopManager::GetInstance()->GetCompositor()->CreateSolidColorLegacyMilBrushProxy(
					glassBrush.put()
				)
			);
		}

		return glassBrush;
	}

	const auto it = g_glassEffectBrushMap.find(window);
	return it == g_glassEffectBrushMap.end() ? nullptr : it->second;
}

void GlassEffectBrush::Remove(uDWM::CTopLevelWindow* window)
{
	g_glassEffectBrushMap.erase(window);
}

void GlassEffectBrush::RemoveAll()
{
	g_glassEffectBrushMap.clear();
}
