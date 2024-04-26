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
		virtual void SetBackdropKind(CompositedBackdropKind kind) = 0;

		virtual void SetClientBlurRegion(HRGN region) = 0;
		virtual void SetCaptionRegion(HRGN region) = 0;
		virtual void SetBorderRegion(HRGN region) = 0;
		virtual void SetAccentRegion(HRGN region) = 0;
		virtual void SetGdiWindowRegion(HRGN region) = 0;

		virtual void ValidateVisual() = 0;
		virtual void UpdateNCBackground() = 0;
	};
	// [Guid("A108448A-FA93-4131-921C-D5D29F800F4B")]
	DECLARE_INTERFACE_IID_(ICompositedBackdropVisualPrivate, IUnknown, "A108448A-FA93-4131-921C-D5D29F800F4B")
	{
		virtual HRGN GetCompositedRegion() const = 0;
		virtual void MarkAsOccluded(bool occluded) = 0;
	};
	namespace Configuration
	{
		inline float g_roundRectRadius{ 0.f };
		inline bool g_overrideBorder{ true };
	}

	size_t GetBackdropCount();
	winrt::com_ptr<ICompositedBackdropVisual> GetOrCreateBackdropVisual(uDwm::CTopLevelWindow* window, bool createIfNecessary = false, bool silent = false);
	void TryCloneBackdropVisualForWindow(uDwm::CTopLevelWindow* src, uDwm::CTopLevelWindow* dst, ICompositedBackdropVisual** visual = nullptr);
	void RemoveBackdrop(uDwm::CTopLevelWindow* window, bool silent = false);
	void Shutdown();
}