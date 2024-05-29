#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"
#include "uDwmProjection.hpp"

namespace OpenGlass::ConfigurationFramework
{
	enum UpdateType : UCHAR
	{
		None = 0,
		Framework = 1 << 0,
		Backdrop = 1 << 1,
		Theme = 1 << 2,
		All = Backdrop | Framework
	};

	void Update(UpdateType type);
	HKEY GetDwmKey();
	HKEY GetPersonalizeKey();
	void Load(bool updateNow = true);
	void Unload();
}