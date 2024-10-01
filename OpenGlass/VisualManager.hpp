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

	namespace VisualManager
	{
		winrt::com_ptr<ILegacyVisualOverrider> GetOrCreateLegacyVisualOverrider(uDwm::CTopLevelWindow* window, bool createIfNecessary = false);
		void RemoveLegacyVisualOverrider(uDwm::CTopLevelWindow* window);
		void ShutdownLegacyVisualOverrider();

		void RedrawTopLevelWindow(uDwm::CTopLevelWindow* window);
	}
}