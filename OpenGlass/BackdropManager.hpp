#pragma once
#include "framework.hpp"
#include "cpprt.hpp"
#include "winrt.hpp"
#include "uDwmProjection.hpp"
#include "ConfigurationFramework.hpp"

namespace OpenGlass::BackdropManager
{
	enum class CompositedBackdropKind : CHAR
	{
		None = -1,
		Legacy,
		Accent,
		SystemBackdrop
	};
	enum class CompositedBackdropType : CHAR
	{
		None = -1,
		Aero,
		Acrylic,
		Mica,
		Blur
	};

	// [Guid("B1FDFCD4-F35C-44FD-8BF0-C2E7E6571461")]
	DECLARE_INTERFACE_IID_(ICompositedBackdropVisual, IUnknown, "B1FDFCD4-F35C-44FD-8BF0-C2E7E6571461")
	{
		virtual void SetClientBlurRegion(HRGN region) = 0;
		virtual void SetCaptionRegion(HRGN region) = 0;
		virtual void SetBorderRegion(HRGN region) = 0;
		virtual void SetAccentRect(LPCRECT lprc) = 0;
		virtual void SetGdiWindowRegion(HRGN region) = 0;

		virtual void ValidateVisual() = 0;
		virtual void UpdateNCBackground() = 0;
		virtual bool CanBeTrimmed() = 0;
	};

	namespace Configuration
	{
		inline float g_roundRectRadius{ 0.f };
		inline bool g_overrideBorder{ false };
		inline wf::TimeSpan g_crossfadeTime{ std::chrono::milliseconds{ 87 } };
	}

	size_t GetCount();
	winrt::com_ptr<ICompositedBackdropVisual> GetOrCreate(uDwm::CTopLevelWindow* window, bool createIfNecessary = false, bool silent = false);
	winrt::com_ptr<ICompositedBackdropVisual> GetOrCreateForAccentBlurRect(uDwm::CTopLevelWindow* window, LPCRECT accentBlurRect, bool createIfNecessary = false, bool silent = false);
	void TryClone(uDwm::CTopLevelWindow* src, uDwm::CTopLevelWindow* dst, ICompositedBackdropVisual** visual = nullptr);
	void Remove(uDwm::CTopLevelWindow* window, bool silent = false);
	void Trim(uDwm::CTopLevelWindow* window);
	void Shutdown();
}