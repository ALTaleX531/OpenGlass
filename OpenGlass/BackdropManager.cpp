#include "pch.h"
#include "GlassFramework.hpp"
#include "uDwmProjection.hpp"
#include "dcompProjection.hpp"
#include "BackdropManager.hpp"
#include "Utils.hpp"

using namespace OpenGlass;
namespace OpenGlass::GlassFramework { extern BackdropManager::CompositedBackdropKind GetActualBackdropKind(uDwm::CTopLevelWindow* This); }
namespace OpenGlass::BackdropManager
{
	std::unordered_map<uDwm::CTopLevelWindow*, winrt::com_ptr<ICompositedBackdropVisual>> g_backdropMap{};
	void RedrawTopLevelWindow(uDwm::CTopLevelWindow* window)
	{
		__try
		{
			auto kind{ GlassFramework::GetActualBackdropKind(window) };
			// 0x10000 UpdateText
			// 0x20000 UpdateIcon
			// 0x100000 UpdateColorization
			// ...

			if (os::buildNumber >= os::build_w11_22h2)
			{
				if (kind == CompositedBackdropKind::SystemBackdrop)
				{
					window->OnSystemBackdropUpdated();
				}
			}
			else
			{
				window->SetDirtyFlags(0x10000);
			}
			if (kind == CompositedBackdropKind::Accent)
			{
				window->OnAccentPolicyUpdated();
			}
			if (os::buildNumber >= os::build_w10_1903)
			{
				window->OnClipUpdated();
			}
			else
			{
				window->OnBlurBehindUpdated();
			}
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {}
	}

	class CCompositedBackdropVisual : public winrt::implements<CCompositedBackdropVisual, ICompositedBackdropVisual>
	{
		bool m_backdropDataChanged{ false };
		bool m_activate{ false };
		bool m_visible{ true };
		DWORD m_state{};
		DWORD m_color{};
		DWORD m_accentFlags{};

		CompositedBackdropKind m_kind{ CompositedBackdropKind::None };
		uDwm::CTopLevelWindow* m_window{ nullptr };
		uDwm::CWindowData* m_data{ nullptr };

		wil::unique_hrgn m_gdiWindowRgn{ nullptr };
		wil::unique_hrgn m_clientBlurRgn{ nullptr };
		wil::unique_hrgn m_borderRgn{ nullptr };
		RECT m_captionRect{};
		RECT m_windowRect{};
		std::optional<RECT> m_accentRect{ std::nullopt };
		wil::unique_hrgn m_compositedRgn{ nullptr };

		void HandleChanges();
		void OnBackdropKindUpdated(CompositedBackdropKind kind);
		void OnBackdropRegionChanged(wil::unique_hrgn& newBackdropRegion);

		POINT GetClientAreaOffset() const;
		wil::unique_hrgn CompositeNewBackdropRegion() const
		{
			wil::unique_hrgn compositedRgn{ CreateRectRgn(0, 0, 0, 0) };
			wil::unique_hrgn nonClientRgn{ CreateRectRgn(0, 0, 0, 0) };
			wil::unique_hrgn realClientBlurRgn{ CreateRectRgn(0, 0, 0, 0) };
			wil::unique_hrgn captionRgn{ CreateRectRgnIndirect(&m_captionRect) };
			wil::unique_hrgn windowRgn{ CreateRectRgnIndirect(&m_windowRect) };

			CombineRgn(nonClientRgn.get(), captionRgn.get(), m_borderRgn.get(), RGN_OR);
			// DwmEnableBlurBehind
			if (m_clientBlurRgn)
			{
				CopyRgn(realClientBlurRgn.get(), m_clientBlurRgn.get());
				auto clientOffset{ GetClientAreaOffset() };
				OffsetRgn(realClientBlurRgn.get(), clientOffset.x, clientOffset.y);
				CombineRgn(compositedRgn.get(), compositedRgn.get(), realClientBlurRgn.get(), RGN_OR);
			}
			if (m_kind != CompositedBackdropKind::Accent)
			{
				CombineRgn(compositedRgn.get(), compositedRgn.get(), nonClientRgn.get(), RGN_OR);
				if (m_kind == CompositedBackdropKind::SystemBackdrop)
				{
					CombineRgn(compositedRgn.get(), compositedRgn.get(), windowRgn.get(), RGN_OR);
				}
			}
			else
			{
				CombineRgn(compositedRgn.get(), compositedRgn.get(), windowRgn.get(), RGN_OR);
				if (m_accentRect)
				{
					wil::unique_hrgn accentRgn{ CreateRectRgnIndirect(&m_accentRect.value()) };
					CombineRgn(compositedRgn.get(), compositedRgn.get(), accentRgn.get(), RGN_AND);
				}

				if (
					m_gdiWindowRgn &&
					m_data->GetAccentPolicy()->IsGdiRegionRespected()
				)
				{
					CombineRgn(compositedRgn.get(), compositedRgn.get(), m_gdiWindowRgn.get(), RGN_AND);
				}
			}
			CombineRgn(compositedRgn.get(), compositedRgn.get(), windowRgn.get(), RGN_AND);

			return compositedRgn;
		}
	public:
		CCompositedBackdropVisual(uDwm::CTopLevelWindow* window);
		virtual ~CCompositedBackdropVisual();

		void SetClientBlurRegion(HRGN region) override;
		void SetCaptionRegion(HRGN region) override;
		void SetBorderRegion(HRGN region) override;
		void SetAccentRect(LPCRECT lprc) override;
		void SetGdiWindowRegion(HRGN region) override;
		void ValidateVisual() override;
		void UpdateNCBackground() override;

		bool CanBeTrimmed() override
		{
			if (m_kind != CompositedBackdropKind::Accent && (!m_visible || m_window->IsTrullyMinimized()))
			{
				return true;
			}
			if (m_accentRect.has_value())
			{
				return false;
			}
			if (m_gdiWindowRgn && m_data->GetAccentPolicy()->IsGdiRegionRespected())
			{
				return false;
			}

			return true;
		}
	};
}

POINT BackdropManager::CCompositedBackdropVisual::GetClientAreaOffset() const
{
	// GetClientBlurVisual() somtimes cannot work in ClonedVisual
	// so here we use the original offset get from CTopLevelWindow::UpdateClientBlur
	auto margins{ m_window->GetClientAreaContainerParentVisual()->GetMargins() };
	return { margins->cxLeftWidth, margins->cyTopHeight };
}

void BackdropManager::CCompositedBackdropVisual::OnBackdropRegionChanged(wil::unique_hrgn& newBackdropRegion)
{
	m_compositedRgn = std::move(newBackdropRegion);

	bool isVisible{};
	RECT regionBox{};
	auto regionType{ GetRgnBox(m_compositedRgn.get(), &regionBox) };
	if (
		regionType == NULLREGION ||
		IsRectEmpty(&regionBox)
	)
	{
		isVisible = false;
	}
	else
	{
		isVisible = true;
	}

	if (m_visible != isVisible)
	{
		m_visible = isVisible;
	}

	if (!m_visible)
	{
		return;
	}

	// ...
}

void BackdropManager::CCompositedBackdropVisual::HandleChanges() try
{
	wil::unique_hrgn compositedRgn{ CompositeNewBackdropRegion() };
	if (!EqualRgn(m_compositedRgn.get(), compositedRgn.get()))
	{
		OnBackdropRegionChanged(compositedRgn);
	}
	m_backdropDataChanged = false;
}
CATCH_LOG_RETURN()

BackdropManager::CCompositedBackdropVisual::CCompositedBackdropVisual(uDwm::CTopLevelWindow* window) :
	m_window{ window },
	m_data{ window->GetData() }
{
	m_borderRgn.reset(CreateRectRgn(0, 0, 0, 0));
	m_compositedRgn.reset(CreateRectRgn(0, 0, 0, 0));

	OnBackdropKindUpdated(GlassFramework::GetActualBackdropKind(window));
	if (m_kind == CompositedBackdropKind::Accent)
	{
		wil::unique_hrgn clipRgn{ CreateRectRgn(0, 0, 0, 0) };
		if (GetWindowRgn(m_data->GetHwnd(), clipRgn.get()) != ERROR)
		{
			SetGdiWindowRegion(clipRgn.get());
		}
		else
		{
			SetGdiWindowRegion(nullptr);
		}
	}
}

BackdropManager::CCompositedBackdropVisual::~CCompositedBackdropVisual()
{
}

void BackdropManager::CCompositedBackdropVisual::OnBackdropKindUpdated(CompositedBackdropKind kind)
{
	if (m_kind != kind)
	{
		m_kind = kind;
		m_backdropDataChanged = true;
	}
}
void BackdropManager::CCompositedBackdropVisual::SetClientBlurRegion(HRGN region)
{
	if (!m_clientBlurRgn)
	{
		m_clientBlurRgn.reset(CreateRectRgn(0, 0, 0, 0));
	}
	CopyRgn(m_clientBlurRgn.get(), region);
	m_backdropDataChanged = true;
}
void BackdropManager::CCompositedBackdropVisual::SetCaptionRegion(HRGN region)
{
	GetRgnBox(region, &m_captionRect);
	m_backdropDataChanged = true;
}
void BackdropManager::CCompositedBackdropVisual::SetBorderRegion(HRGN region)
{
	CopyRgn(m_borderRgn.get(), region);
	m_backdropDataChanged = true;
}
void BackdropManager::CCompositedBackdropVisual::SetAccentRect(LPCRECT lprc)
{
	if (lprc)
	{
		m_accentRect = *lprc;
		m_gdiWindowRgn.reset();
	}
	else if (m_accentRect.has_value())
	{
		m_accentRect = std::nullopt;
		if (m_kind == CompositedBackdropKind::Accent)
		{
			wil::unique_hrgn clipRgn{ CreateRectRgn(0, 0, 0, 0) };
			if (GetWindowRgn(m_data->GetHwnd(), clipRgn.get()) != ERROR)
			{
				m_gdiWindowRgn.reset(clipRgn.release());
			}
			else
			{
				m_gdiWindowRgn.reset();
			}
		}
	}
	m_backdropDataChanged = true;
}
void BackdropManager::CCompositedBackdropVisual::SetGdiWindowRegion(HRGN region)
{
	if (region)
	{
		if (!m_gdiWindowRgn)
		{
			m_gdiWindowRgn.reset(CreateRectRgn(0, 0, 0, 0));
		}
		CopyRgn(m_gdiWindowRgn.get(), region);
		m_accentRect = std::nullopt;
	}
	else if (m_gdiWindowRgn)
	{
		m_gdiWindowRgn.reset();
		m_accentRect = std::nullopt;
	}
	m_backdropDataChanged = true;
}

void BackdropManager::CCompositedBackdropVisual::ValidateVisual()
{
	OnBackdropKindUpdated(GlassFramework::GetActualBackdropKind(m_window));
	if (m_backdropDataChanged) { HandleChanges(); }
	if (m_visible)
	{
		
	}
}
void BackdropManager::CCompositedBackdropVisual::UpdateNCBackground()
{
	RECT borderRect{};
	THROW_HR_IF_NULL(E_INVALIDARG, m_window->GetActualWindowRect(&borderRect, true, true, false));

	if (!EqualRect(&m_windowRect, &borderRect))
	{
		m_windowRect = borderRect;
		m_backdropDataChanged = true;
	}
}


size_t BackdropManager::GetCount()
{
	return g_backdropMap.size();
}

winrt::com_ptr<BackdropManager::ICompositedBackdropVisual> BackdropManager::GetOrCreate(uDwm::CTopLevelWindow* window, bool createIfNecessary, bool silent)
{
	auto it{ g_backdropMap.find(window) };

	if (createIfNecessary)
	{
		auto data{ window->GetData() };

		if (
			data &&
			it == g_backdropMap.end()
		)
		{
			HWND targetWindow{ data->GetHwnd() };
			HWND shellWindow{ uDwm::GetShellWindowForCurrentDesktop() };

			if (targetWindow != shellWindow)
			{
				auto result{ g_backdropMap.emplace(window, winrt::make<BackdropManager::CCompositedBackdropVisual>(window)) };
				if (result.second == true)
				{
					it = result.first;

					if (!silent)
					{
						RedrawTopLevelWindow(window);
					}
				}
			}
		}
	}

	return it == g_backdropMap.end() ? nullptr : it->second;
}

winrt::com_ptr<BackdropManager::ICompositedBackdropVisual> BackdropManager::GetOrCreateForAccentBlurRect(uDwm::CTopLevelWindow* window, LPCRECT accentBlurRect, bool createIfNecessary, bool silent)
{
	auto it{ g_backdropMap.find(window) };

	auto result{ BackdropManager::GetOrCreate(window, createIfNecessary, true) };
	if (result && it == g_backdropMap.end())
	{
		result->SetAccentRect(accentBlurRect);
		if (!silent)
		{
			RedrawTopLevelWindow(window);
		}
	}

	return result;
}

void BackdropManager::TryClone(uDwm::CTopLevelWindow* src, uDwm::CTopLevelWindow* dst, ICompositedBackdropVisual** visual)
{
	/*auto legacyVisual{ src->GetLegacyVisual() };
	if (auto backdrop{ GetOrCreate(src) }; backdrop && legacyVisual)
	{
		auto it{ g_backdropMap.find(dst) };
		if (it == g_backdropMap.end())
		{
			winrt::com_ptr<ICompositedBackdropVisual> clonedBackdrop{ nullptr };

			clonedBackdrop = legacyVisual->IsCloneAllowed() ? winrt::make<CClonedCompositedBackdropVisual>(dst, reinterpret_cast<CCompositedBackdropVisual*>(backdrop.get())) : winrt::make<CClonedPeekingBackdropVisual>(src, dst);

			auto result{ g_backdropMap.emplace(dst, clonedBackdrop) };
			if (result.second == true) { it = result.first; }
		}

		if (visual)
		{
			*visual = (it == g_backdropMap.end() ? nullptr : it->second.get());
		}
	}*/
}

void BackdropManager::Remove(uDwm::CTopLevelWindow* window, bool silent)
{
	auto it{ g_backdropMap.find(window) };

	if (it != g_backdropMap.end())
	{
		g_backdropMap.erase(it);

		if (!silent)
		{
			RedrawTopLevelWindow(window);
		}
	}
}

void BackdropManager::Trim(uDwm::CTopLevelWindow* window)
{
	auto it{ g_backdropMap.find(window) };

	if (it != g_backdropMap.end() && it->second->CanBeTrimmed())
	{
		g_backdropMap.erase(it);
	}
}

void BackdropManager::Shutdown()
{
	std::vector<uDwm::CTopLevelWindow*> windowCollection{};
	for (const auto& [window, backdrop] : g_backdropMap)
	{
		windowCollection.push_back(window);
	}
	g_backdropMap.clear();

	for (auto& window : windowCollection)
	{
		RedrawTopLevelWindow(window);
	}
}