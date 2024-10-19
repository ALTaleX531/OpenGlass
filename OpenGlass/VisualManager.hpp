#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "uDwmProjection.hpp"
#include "ConfigurationFramework.hpp"

namespace OpenGlass
{
	// [Guid("B1FDFCD4-F35C-44FD-8BF0-C2E7E6571461")]
	DECLARE_INTERFACE_IID_(ILegacyVisualOverrider, IUnknown, "B1FDFCD4-F35C-44FD-8BF0-C2E7E6571461")
	{
		virtual HRESULT STDMETHODCALLTYPE UpdateNCBackground(
			HRGN captionRgn,
			HRGN borderRgn
		) = 0;
	};
	// [Guid("1DC3F5F6-F825-438D-A5DB-041A167BDE85")]
	DECLARE_INTERFACE_IID_(IAnimatedGlassSheetOverrider, IUnknown, "1DC3F5F6-F825-438D-A5DB-041A167BDE85")
	{
		virtual HRESULT STDMETHODCALLTYPE OnRectUpdated(LPCRECT lprc) = 0;
	};

	namespace VisualManager
	{
		namespace LegacyVisualOverrider
		{
			winrt::com_ptr<ILegacyVisualOverrider> GetOrCreate(uDwm::CTopLevelWindow* window, bool createIfNecessary = false);
			void Remove(uDwm::CTopLevelWindow* window);
			void Shutdown();
		}

		namespace AnimatedGlassSheetOverrider
		{
			winrt::com_ptr<IAnimatedGlassSheetOverrider> GetOrCreate(uDwm::CAnimatedGlassSheet* sheet, bool createIfNecessary = false);
			void Remove(uDwm::CAnimatedGlassSheet* sheet);
			void Shutdown();
		}

		void RedrawTopLevelWindow(uDwm::CTopLevelWindow* window);
	}
}