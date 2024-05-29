#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "HookHelper.hpp"
#include "uDwmProjection.hpp"

namespace OpenGlass::GeometryRecorder
{
	void BeginCapture();
	HRGN GetRegionFromGeometry(uDwm::CBaseGeometryProxy* geometry);
	size_t GetGeometryCount();
	void EndCapture();

	HRESULT Startup();
	void Shutdown();
}