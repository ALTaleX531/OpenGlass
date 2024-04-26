#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"
#include "Utils.hpp"
#include "HookHelper.hpp"

namespace OpenGlass::dwmcore
{
	inline HMODULE g_moduleHandle{ GetModuleHandleW(L"dwmcore.dll") };

	namespace CCommonRegistryData
	{
		inline PULONGLONG m_backdropBlurCachingThrottleQPCTimeDelta{ nullptr };
	}

	inline void InitializeFromSymbol(std::string_view fullyUnDecoratedFunctionName, const HookHelper::OffsetStorage& offset)
	{
		if (fullyUnDecoratedFunctionName == "CCommonRegistryData::m_backdropBlurCachingThrottleQPCTimeDelta")
		{
			offset.To(g_moduleHandle, CCommonRegistryData::m_backdropBlurCachingThrottleQPCTimeDelta);
		}
	}
}