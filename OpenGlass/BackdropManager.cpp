#include "pch.h"
#include "GlassFramework.hpp"
#include "uDwmProjection.hpp"
#include "dcompProjection.hpp"
#include "BackdropManager.hpp"
#include "CanvasGeometry.hpp"
#include "GlassReflection.hpp"
#include "Utils.hpp"
#include "TestBackdrop.hpp"

using namespace OpenGlass;
namespace OpenGlass::BackdropManager
{
	std::unordered_map<uDwm::CTopLevelWindow*, winrt::com_ptr<ICompositedBackdropVisual>> g_backdropMap{};
	void RedrawTopLevelWindow(uDwm::CTopLevelWindow* window)
	{
		if (os::buildNumber < os::build_w11_22h2)
		{
			// 0x10000 UpdateText
			// 0x20000 UpdateIcon
			window->SetDirtyFlags(window->GetDirtyFlags() | 0x10000 | 0x20000 | 0x4000);
		}
		// CWindowList::ShowHide didn't update the gdi clip region
		window->OnClipUpdated();
		uDwm::CDesktopManager::s_pDesktopManagerInstance->GetWindowList()->ShowHide(window->GetData(), false);
	}

	class CCompositedBackdropVisual : public winrt::implements<CCompositedBackdropVisual, ICompositedBackdropVisual, ICompositedBackdropVisualPrivate>, uDwm::CBackdropVisual
	{
		bool m_cloned{ false };
		bool m_occluded{ false };
		bool m_regionDirty{ false };
		bool m_overrideBorder{ false };
		RECT m_windowBox{};
		CompositedBackdropKind m_kind{ CompositedBackdropKind::None };
		uDwm::CTopLevelWindow* m_window{ nullptr };
		uDwm::CWindowData* m_data{ nullptr };
		wuc::ContainerVisual m_containerVisual{ nullptr };
		wuc::SpriteVisual m_spriteVisual{ nullptr };
		CGlassReflectionVisual m_reflectionVisual;

		POINT m_clientOffset{};
		wil::unique_hrgn m_clientBlurRgn{ nullptr };
		wil::unique_hrgn m_captionRgn{ nullptr };
		wil::unique_hrgn m_borderRgn{ nullptr };
		wil::unique_hrgn m_accentRgn{ nullptr };
		wil::unique_hrgn m_windowRgn{ nullptr };
		wil::unique_hrgn m_gdiWindowRgn{ nullptr };
		float m_roundRectRadius{ 0.f };

		wil::unique_hrgn m_compositedRgn{ nullptr };
		winrt::com_ptr<uDwm::CRgnGeometryProxy> m_rgnGeometryProxy{ nullptr };
		winrt::com_ptr<uDwm::CRgnGeometryProxy> m_rgnParentGeometryProxy{ nullptr };
		wuc::CompositionPathGeometry m_pathGeometry{ nullptr };
		wuc::CompositionClip m_clip{ nullptr };
		winrt::com_ptr<Win2D::CanvasGeometry> m_canvasGeometry{ nullptr };

		POINT GetClientAreaOffset();
		HRESULT InitializeVisual() override;
		void UninitializeVisual() override;
		void OnRegionUpdated();
		void OnDeviceLost();
	public:
		CCompositedBackdropVisual(uDwm::CTopLevelWindow* window);
		CCompositedBackdropVisual(uDwm::CTopLevelWindow* window, CCompositedBackdropVisual* backdrop);
		virtual ~CCompositedBackdropVisual();

		HRGN GetCompositedRegion() const override;
		void MarkAsOccluded(bool occluded) override;

		void SetBackdropKind(CompositedBackdropKind kind) override;
		void SetClientBlurRegion(HRGN region) override;
		void SetCaptionRegion(HRGN region) override;
		void SetBorderRegion(HRGN region) override;
		void SetAccentRegion(HRGN region) override;
		void SetGdiWindowRegion(HRGN region) override;
		void ValidateVisual() override;
		void UpdateNCBackground() override;
	};
}

POINT BackdropManager::CCompositedBackdropVisual::GetClientAreaOffset()
{
	// GetClientBlurVisual() somtimes cannot work in ClonedVisual
	// so here we use the original offset get from CTopLevelWindow::UpdateClientBlur
	auto margins{ m_window->GetClientAreaContainerParentVisual()->GetMargins() };
	return { margins->cxLeftWidth, margins->cyTopHeight };
}
HRESULT BackdropManager::CCompositedBackdropVisual::InitializeVisual()
{
	RETURN_IF_FAILED(uDwm::CBackdropVisual::InitializeVisual());

	auto compositor{ m_dcompDevice.as<wuc::Compositor>() };
	m_containerVisual = compositor.CreateContainerVisual();
	m_spriteVisual = compositor.CreateSpriteVisual();
	m_spriteVisual.RelativeSizeAdjustment({ 1.f, 1.f });
	m_spriteVisual.Brush(GetBrush(compositor));
	m_reflectionVisual.InitializeVisual(compositor);
	m_containerVisual.Children().InsertAtBottom(m_spriteVisual);
	m_containerVisual.Children().InsertAtTop(m_reflectionVisual.GetVisual());
	m_pathGeometry = compositor.CreatePathGeometry();
	RETURN_IF_FAILED(uDwm::ResourceHelper::CreateGeometryFromHRGN(wil::unique_hrgn{ CreateRectRgn(0, 0, 0, 0) }.get(), m_rgnGeometryProxy.put()));

	winrt::com_ptr<ID2D1Factory> factory{ nullptr };
	uDwm::CDesktopManager::s_pDesktopManagerInstance->GetD2DDevice()->GetFactory(factory.put());
	// since build 22621, Path propertie cannot be empty or the dwmcore will raise a null pointer exception
	m_pathGeometry.Path(wuc::CompositionPath{ Win2D::CanvasGeometry::CreateGeometryFromHRGN(factory.get(), m_compositedRgn.get()).as<wg::IGeometrySource2D>() });

	m_clip = compositor.CreateGeometricClip(m_pathGeometry);
	m_dcompVisual.as<wuc::Visual>().Clip(m_clip);
	RETURN_IF_FAILED(m_udwmVisual->GetVisualProxy()->SetClip(m_rgnGeometryProxy.get()));
	m_visualCollection.InsertAtBottom(m_containerVisual);

	return S_OK;
}

void BackdropManager::CCompositedBackdropVisual::UninitializeVisual()
{
	m_clip = nullptr;
	m_pathGeometry = nullptr;
	m_rgnGeometryProxy = nullptr;
	m_spriteVisual = nullptr;

	if (m_containerVisual)
	{
		m_containerVisual.Children().RemoveAll();
		m_containerVisual = nullptr;
	}

	m_reflectionVisual.UninitializeVisual();
	uDwm::CBackdropVisual::UninitializeVisual();
}
void BackdropManager::CCompositedBackdropVisual::OnRegionUpdated() try
{
	wil::unique_hrgn compositedRgn{ CreateRectRgn(0, 0, 0, 0) };
	wil::unique_hrgn nonClientRgn{ CreateRectRgn(0, 0, 0, 0) };
	wil::unique_hrgn realClientBlurRgn{ CreateRectRgn(0, 0, 0, 0) };

	RECT borderRect{};
	THROW_HR_IF_NULL(E_INVALIDARG, m_window->GetActualWindowRect(&borderRect, true, true, false));
	if (!m_cloned && (borderRect.right <= GetSystemMetrics(SM_XVIRTUALSCREEN) || borderRect.bottom <= GetSystemMetrics(SM_YVIRTUALSCREEN)))
	{
		return;
	}

	bool forceUpdate{ false };
	if (m_overrideBorder != Configuration::g_overrideBorder)
	{
		m_overrideBorder = Configuration::g_overrideBorder;
		forceUpdate = true;
	}
	if (m_roundRectRadius != Configuration::g_roundRectRadius)
	{
		m_roundRectRadius = Configuration::g_roundRectRadius;
		forceUpdate = true;
	}
	CombineRgn(nonClientRgn.get(), m_captionRgn.get(), m_overrideBorder ? m_borderRgn.get() : nullptr, m_overrideBorder ? RGN_OR : RGN_COPY);
	if (m_kind != CompositedBackdropKind::Accent)
	{
		CombineRgn(compositedRgn.get(), compositedRgn.get(), nonClientRgn.get(), RGN_OR);
		if (m_kind == CompositedBackdropKind::SystemBackdrop)
		{
			CombineRgn(compositedRgn.get(), compositedRgn.get(), m_windowRgn.get(), RGN_OR);
		}
	}
	else
	{
		CombineRgn(compositedRgn.get(), compositedRgn.get(), m_windowRgn.get(), RGN_OR);
		if (m_accentRgn)
		{
			CombineRgn(compositedRgn.get(), compositedRgn.get(), m_accentRgn.get(), RGN_AND);
		}
	}
	// DwmEnableBlurBehind
	if (m_clientBlurRgn)
	{
		CopyRgn(realClientBlurRgn.get(), m_clientBlurRgn.get());
		if (!m_cloned)
		{
			m_clientOffset = GetClientAreaOffset();
		}
		OffsetRgn(realClientBlurRgn.get(), m_clientOffset.x, m_clientOffset.y);
		CombineRgn(compositedRgn.get(), compositedRgn.get(), realClientBlurRgn.get(), RGN_OR);
	}

	CombineRgn(compositedRgn.get(), compositedRgn.get(), m_windowRgn.get(), RGN_AND);
	if (
		m_gdiWindowRgn && 
		(
			m_kind != CompositedBackdropKind::Accent ||
			(
				m_kind == CompositedBackdropKind::Accent &&
				m_data->GetAccentPolicy()->IsGdiRegionRespected()
			)
		)
	) 
	{
		CombineRgn(compositedRgn.get(), compositedRgn.get(), m_gdiWindowRgn.get(), RGN_AND); 
	}

	if (!EqualRgn(m_compositedRgn.get(), compositedRgn.get()) || forceUpdate)
	{
		m_compositedRgn = std::move(compositedRgn);

		RECT regionBox{};
		if ((GetRgnBox(m_compositedRgn.get(), &regionBox) == NULLREGION || IsRectEmpty(&regionBox)) && !m_cloned)
		{
			m_containerVisual.IsVisible(false);
		}
		else if (!m_containerVisual.IsVisible() && (m_data->IsWindowVisibleAndUncloaked() || m_cloned))
		{
			m_containerVisual.IsVisible(true);
		}
		m_containerVisual.Offset({ static_cast<float>(regionBox.left), static_cast<float>(regionBox.top), 0.f });
		m_containerVisual.Size({ static_cast<float>(max(wil::rect_width(regionBox), 0)), static_cast<float>(max(wil::rect_height(regionBox), 0)) });
		m_reflectionVisual.NotifyOffsetToWindow(m_containerVisual.Offset());

		winrt::com_ptr<ID2D1Factory> factory{ nullptr };
		uDwm::CDesktopManager::s_pDesktopManagerInstance->GetD2DDevice()->GetFactory(factory.put());
		// DO NOT USE .put() HERE, OR IT WILL RELEASE THE OBJECT WE CREATED BEFORE!
		uDwm::ResourceHelper::CreateGeometryFromHRGN(m_compositedRgn.get(), reinterpret_cast<uDwm::CRgnGeometryProxy**>(&m_rgnGeometryProxy));
		m_canvasGeometry = Win2D::CanvasGeometry::CreateGeometryFromHRGN(
			factory.get(),
			m_compositedRgn.get()
		);
		// handle round corner
		if (!m_cloned)
		{
			if (m_window->HasNonClientBackground(m_data) && !IsMaximized(m_data->GetHwnd()))
			{
				m_window->GetActualWindowRect(&m_windowBox, true, true, true);
				UnionRect(&m_windowBox, &m_windowBox, &regionBox);
			}
			else
			{
				m_windowBox = {};
			}
		}
		if (m_roundRectRadius != 0.f && !IsRectEmpty(&m_windowBox))
		{

			winrt::com_ptr<ID2D1Geometry> geometry{ nullptr };
			m_canvasGeometry->GetGeometry(geometry.put());
			winrt::com_ptr<ID2D1RoundedRectangleGeometry> roundedGeometry{ nullptr };
			THROW_IF_FAILED(
				factory->CreateRoundedRectangleGeometry(
					D2D1::RoundedRect(
						D2D1::RectF(
							static_cast<float>(m_windowBox.left),
							static_cast<float>(m_windowBox.top),
							static_cast<float>(m_windowBox.right),
							static_cast<float>(m_windowBox.bottom)
						),
						m_roundRectRadius,
						m_roundRectRadius
					),
					roundedGeometry.put()
				)
			);
			winrt::com_ptr<ID2D1PathGeometry> pathGeometry{ nullptr };
			THROW_IF_FAILED(
				factory->CreatePathGeometry(pathGeometry.put())
			);
			winrt::com_ptr<ID2D1GeometrySink> sink{ nullptr };
			THROW_IF_FAILED(pathGeometry->Open(sink.put()));
			THROW_IF_FAILED(
				geometry->CombineWithGeometry(
					roundedGeometry.get(),
					D2D1_COMBINE_MODE_INTERSECT,
					D2D1::Matrix3x2F::Identity(),
					sink.get()
				)
			);
			THROW_IF_FAILED(sink->Close());
			m_canvasGeometry->SetGeometry(pathGeometry.get());
		}
		m_pathGeometry.Path(wuc::CompositionPath{ m_canvasGeometry.as<wg::IGeometrySource2D>() });
		forceUpdate = false;
	}
}
CATCH_LOG_RETURN()

void BackdropManager::CCompositedBackdropVisual::OnDeviceLost()
{
	UninitializeVisual();
	InitializeInteropDevice(uDwm::CDesktopManager::s_pDesktopManagerInstance->GetDCompDevice());
	InitializeVisual();
	m_regionDirty = true;
}

BackdropManager::CCompositedBackdropVisual::CCompositedBackdropVisual(uDwm::CTopLevelWindow* window) :
	m_window{ window },
	m_data{ window->GetData() },
	m_reflectionVisual{ m_window, m_data },
	uDwm::CBackdropVisual{ window->GetNonClientVisual() }
{
	m_captionRgn.reset(CreateRectRgn(0, 0, 0, 0));
	m_borderRgn.reset(CreateRectRgn(0, 0, 0, 0));
	m_windowRgn.reset(CreateRectRgn(0, 0, 0, 0));
	m_compositedRgn.reset(CreateRectRgn(0, 0, 0, 0));
	OnDeviceLost();
}
BackdropManager::CCompositedBackdropVisual::CCompositedBackdropVisual(uDwm::CTopLevelWindow* window, CCompositedBackdropVisual* backdrop) :
	m_cloned{ true },
	m_kind{ backdrop->m_kind },
	m_window{ window },
	m_data{ backdrop->m_data },
	m_clientOffset{ backdrop->m_clientOffset },
	m_reflectionVisual{ m_window, m_data, true },
	m_roundRectRadius{ backdrop->m_roundRectRadius },
	m_overrideBorder{ backdrop->m_overrideBorder },
	m_windowBox{ backdrop->m_windowBox },
	uDwm::CBackdropVisual{ window->GetNonClientVisual() }
{
	if (backdrop->m_clientBlurRgn)
	{
		m_clientBlurRgn.reset(CreateRectRgn(0, 0, 0, 0));
		CopyRgn(m_clientBlurRgn.get(), backdrop->m_clientBlurRgn.get());
	}
	if (backdrop->m_captionRgn)
	{
		m_captionRgn.reset(CreateRectRgn(0, 0, 0, 0));
		CopyRgn(m_captionRgn.get(), backdrop->m_captionRgn.get());
	}
	if (backdrop->m_borderRgn)
	{
		m_borderRgn.reset(CreateRectRgn(0, 0, 0, 0));
		CopyRgn(m_borderRgn.get(), backdrop->m_borderRgn.get());
	}
	if (backdrop->m_accentRgn)
	{
		m_accentRgn.reset(CreateRectRgn(0, 0, 0, 0));
		CopyRgn(m_accentRgn.get(), backdrop->m_accentRgn.get());
	}
	if (backdrop->m_windowRgn)
	{
		m_windowRgn.reset(CreateRectRgn(0, 0, 0, 0));
		CopyRgn(m_windowRgn.get(), backdrop->m_windowRgn.get());
	}
	if (backdrop->m_gdiWindowRgn)
	{
		m_gdiWindowRgn.reset(CreateRectRgn(0, 0, 0, 0));
		CopyRgn(m_gdiWindowRgn.get(), backdrop->m_gdiWindowRgn.get());
	}
	m_compositedRgn.reset(CreateRectRgn(0, 0, 0, 0));
	OnDeviceLost();
	OnRegionUpdated();
	m_reflectionVisual.SyncReflectionData(backdrop->m_reflectionVisual);
}
BackdropManager::CCompositedBackdropVisual::~CCompositedBackdropVisual()
{
	UninitializeVisual();
}

HRGN BackdropManager::CCompositedBackdropVisual::GetCompositedRegion() const
{
	return m_compositedRgn.get();
}
void BackdropManager::CCompositedBackdropVisual::MarkAsOccluded(bool occluded)
{
	if (m_cloned) { return; }
	if (m_occluded != occluded)
	{
		m_occluded = occluded;

		m_dcompVisual.as<IDCompositionVisual3>()->SetVisible(!occluded);
	}
}

void BackdropManager::CCompositedBackdropVisual::SetBackdropKind(CompositedBackdropKind kind)
{
	if (m_cloned) { return; }
	if (m_kind != kind)
	{
		m_kind = kind;
		m_regionDirty = true;
	}
}
void BackdropManager::CCompositedBackdropVisual::SetClientBlurRegion(HRGN region)
{
	if (m_cloned) { return; }
	if (region)
	{
		if (!m_clientBlurRgn)
		{
			m_clientBlurRgn.reset(CreateRectRgn(0, 0, 0, 0));
		}
		CopyRgn(m_clientBlurRgn.get(), region);
	}
	else
	{
		m_clientBlurRgn.reset();
	}
	m_regionDirty = true;
}
void BackdropManager::CCompositedBackdropVisual::SetCaptionRegion(HRGN region)
{
	if (m_cloned) { return; }
	CopyRgn(m_captionRgn.get(), region);
	m_regionDirty = true;
}
void BackdropManager::CCompositedBackdropVisual::SetBorderRegion(HRGN region)
{
	if (m_cloned) { return; }
	CopyRgn(m_borderRgn.get(), region);
	m_regionDirty = true;
}
void BackdropManager::CCompositedBackdropVisual::SetAccentRegion(HRGN region)
{
	if (m_cloned) { return; }
	if (region)
	{
		if (!m_accentRgn)
		{
			m_accentRgn.reset(CreateRectRgn(0, 0, 0, 0));
		}
		CopyRgn(m_accentRgn.get(), region);
	}
	else
	{
		m_accentRgn.reset();
	}
	m_regionDirty = true;
}
void BackdropManager::CCompositedBackdropVisual::SetGdiWindowRegion(HRGN region)
{
	if (m_cloned) { return; }
	if (region)
	{
		if (!m_gdiWindowRgn)
		{
			m_gdiWindowRgn.reset(CreateRectRgn(0, 0, 0, 0));
		}
		CopyRgn(m_gdiWindowRgn.get(), region);
	}
	else
	{
		m_gdiWindowRgn.reset();
	}
	m_regionDirty = true;
}

void BackdropManager::CCompositedBackdropVisual::ValidateVisual()
{
	if (m_cloned) { return; }

	if (m_containerVisual.IsVisible() && m_data->IsWindowVisibleAndUncloaked())
	{
		BOOL valid{ FALSE };
		if (!uDwm::CheckDeviceState(m_dcompDevice))
		{
			OnDeviceLost();
		}
	}

	if (m_regionDirty)
	{
		OnRegionUpdated();

		m_regionDirty = false;
	}
	m_reflectionVisual.ValidateVisual();
}
void BackdropManager::CCompositedBackdropVisual::UpdateNCBackground()
{
	if (m_cloned) { return; }

	RECT borderRect{};
	THROW_HR_IF_NULL(E_INVALIDARG, m_window->GetActualWindowRect(&borderRect, true, true, false));

	wil::unique_hrgn calculatedWindowRgn{ nullptr };
	calculatedWindowRgn.reset(CreateRectRgnIndirect(&borderRect));

	if (!EqualRgn(m_windowRgn.get(), calculatedWindowRgn.get()))
	{
		m_windowRgn = std::move(calculatedWindowRgn);
		m_regionDirty = true;
	}
}


size_t BackdropManager::GetBackdropCount()
{
	return g_backdropMap.size();
}

winrt::com_ptr<BackdropManager::ICompositedBackdropVisual> BackdropManager::GetOrCreateBackdropVisual(uDwm::CTopLevelWindow* window, bool createIfNecessary, bool silent)
{
	auto it{ g_backdropMap.find(window) };

	if (createIfNecessary)
	{
		auto data{ window->GetData() };
		if (
			it == g_backdropMap.end() &&
			data->IsWindowVisibleAndUncloaked() &&
			data->GetHwnd() != uDwm::GetShellWindowForCurrentDesktop()
			)
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

	return it == g_backdropMap.end() ? nullptr : it->second;
}

void BackdropManager::TryCloneBackdropVisualForWindow(uDwm::CTopLevelWindow* src, uDwm::CTopLevelWindow* dst, ICompositedBackdropVisual** visual)
{
	auto legacyVisual{ src->GetLegacyVisual() };
	if (auto backdrop{ GetOrCreateBackdropVisual(src) }; backdrop && (legacyVisual && legacyVisual->IsCloneAllowed()))
	{
		auto it{ g_backdropMap.find(dst) };
		if (it == g_backdropMap.end())
		{
			winrt::com_ptr<ICompositedBackdropVisual> clonedBackdrop{ nullptr };

			clonedBackdrop = winrt::make<CCompositedBackdropVisual>(dst, reinterpret_cast<CCompositedBackdropVisual*>(backdrop.get()));

			auto result{ g_backdropMap.emplace(dst, clonedBackdrop) };
			if (result.second == true) { it = result.first; }
		}

		if (visual)
		{
			*visual = (it == g_backdropMap.end() ? nullptr : it->second.get());
		}
	}
}

void BackdropManager::RemoveBackdrop(uDwm::CTopLevelWindow* window, bool silent)
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

void BackdropManager::Shutdown()
{
	std::vector<uDwm::CTopLevelWindow*> windowCollection{};
	for (const auto& window : g_backdropMap)
	{
		windowCollection.push_back(window.first);
	}
	g_backdropMap.clear();

	for (auto& window : windowCollection)
	{
		RedrawTopLevelWindow(window);
	}
}