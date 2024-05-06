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
		else
		{
			window->OnSystemBackdropUpdated();
		}
		window->OnClipUpdated();
		window->OnAccentPolicyUpdated();
	}

	class CCompositedBackdropVisual : public winrt::implements<CCompositedBackdropVisual, ICompositedBackdropVisual>, uDwm::CBackdropVisual
	{
		bool m_cloned{ false };
		bool m_regionDirty{ false };
		bool m_overrideBorder{ true };
		bool m_splitBlurRegionIntoChunks{ true };
		std::unique_ptr<RECT> m_windowBox{};
		CompositedBackdropKind m_kind{ CompositedBackdropKind::None };
		uDwm::CTopLevelWindow* m_window{ nullptr };
		uDwm::CWindowData* m_data{ nullptr };

		wuc::CompositionBrush m_backdropBrush{ nullptr };
		wuc::ContainerVisual m_containerVisual{ nullptr };
		wuc::SpriteVisual m_spriteVisual{ nullptr };
		wuc::RedirectVisual m_redirectVisual{ nullptr };

		wuc::SpriteVisual m_captionVisual{ nullptr };
		wuc::SpriteVisual m_borderLeftVisual{ nullptr };
		wuc::SpriteVisual m_borderBottomVisual{ nullptr };
		wuc::SpriteVisual m_borderRightVisual{ nullptr };
		wuc::SpriteVisual m_clientVisual{ nullptr };

		winrt::com_ptr<CGlassReflectionVisual> m_reflectionVisual{ nullptr };

		POINT m_clientOffset{};
		wil::unique_hrgn m_clientBlurRgn{ nullptr };
		wil::unique_hrgn m_captionRgn{ nullptr };
		wil::unique_hrgn m_borderRgn{ nullptr };
		wil::unique_hrgn m_accentRgn{ nullptr };
		wil::unique_hrgn m_windowRgn{ nullptr };
		wil::unique_hrgn m_gdiWindowRgn{ nullptr };
		float m_roundRectRadius{ 0.f };

		wil::unique_hrgn m_compositedRgn{ nullptr };
		wuc::CompositionPathGeometry m_pathGeometry{ nullptr };

		POINT GetClientAreaOffset() const;
		HRESULT InitializeVisual() override;
		void UninitializeVisual() override;
		void OnRegionUpdated();
		void OnLayoutUpdated();
		void OnWindowStateChanged();
		void OnAccentColorChanged();
		void OnDeviceLost();

		bool DoesBorderParticipateInBlurRegion() const
		{
			return m_overrideBorder || m_data->IsFrameExtendedIntoClientAreaLRB();
		}
		bool IsBlurRegionSplittingAvailable() const
		{
			return m_splitBlurRegionIntoChunks && m_kind == CompositedBackdropKind::Legacy && !m_data->IsFullGlass();
		}
	public:
		CCompositedBackdropVisual(uDwm::CTopLevelWindow* window);
		CCompositedBackdropVisual(uDwm::CTopLevelWindow* window, CCompositedBackdropVisual* backdrop);
		virtual ~CCompositedBackdropVisual();

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

POINT BackdropManager::CCompositedBackdropVisual::GetClientAreaOffset() const
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
	m_captionVisual = compositor.CreateSpriteVisual();
	m_borderLeftVisual = compositor.CreateSpriteVisual();
	m_borderLeftVisual.RelativeSizeAdjustment({ 0.f, 1.f });
	m_borderBottomVisual = compositor.CreateSpriteVisual();
	m_borderBottomVisual.RelativeOffsetAdjustment({ 0.f, 1.f, 0.f });
	m_borderRightVisual = compositor.CreateSpriteVisual();
	m_borderRightVisual.RelativeOffsetAdjustment({ 1.f, 0.f, 0.f });
	m_borderRightVisual.RelativeSizeAdjustment({ 0.f, 1.f });
	m_clientVisual = compositor.CreateSpriteVisual();
	
	// prepare backdrop brush
	m_backdropBrush = GetBrush(compositor);
	m_captionVisual.Brush(m_backdropBrush);
	m_borderLeftVisual.Brush(m_backdropBrush);
	m_borderBottomVisual.Brush(m_backdropBrush);
	m_borderRightVisual.Brush(m_backdropBrush);
	m_clientVisual.Brush(m_backdropBrush);
	OnLayoutUpdated();

	// if the effect brush is compiled during the maximize/minimize animation
	// then it will NOT show normally, that's because the compilation is asynchronous
	m_reflectionVisual->InitializeVisual(compositor);
	m_containerVisual.Children().InsertAtBottom(m_spriteVisual);
	m_containerVisual.Children().InsertAtTop(m_reflectionVisual->GetVisual());
	m_pathGeometry = compositor.CreatePathGeometry();
	
	winrt::com_ptr<ID2D1Factory> factory{ nullptr };
	uDwm::CDesktopManager::s_pDesktopManagerInstance->GetD2DDevice()->GetFactory(factory.put());
	// since build 22621, Path propertie cannot be empty or the dwmcore will raise a null pointer exception
	m_pathGeometry.Path(wuc::CompositionPath{ Win2D::CanvasGeometry::CreateGeometryFromHRGN(factory.get(), m_compositedRgn.get()).as<wg::IGeometrySource2D>() });

	m_dcompVisual.as<wuc::Visual>().Clip(compositor.CreateGeometricClip(m_pathGeometry));
	m_visualCollection.InsertAtBottom(m_containerVisual);

	return S_OK;
}

void BackdropManager::CCompositedBackdropVisual::UninitializeVisual()
{
	m_pathGeometry = nullptr;
	m_spriteVisual = nullptr;
	m_containerVisual = nullptr;

	m_reflectionVisual->UninitializeVisual();
	uDwm::CBackdropVisual::UninitializeVisual();
}
void BackdropManager::CCompositedBackdropVisual::OnRegionUpdated() try
{
	wil::unique_hrgn compositedRgn{ CreateRectRgn(0, 0, 0, 0) };
	wil::unique_hrgn nonClientRgn{ CreateRectRgn(0, 0, 0, 0) };
	wil::unique_hrgn realClientBlurRgn{ CreateRectRgn(0, 0, 0, 0) };

	if (!m_cloned)
	{
		RECT borderRect{};
		THROW_HR_IF_NULL(E_INVALIDARG, m_window->GetActualWindowRect(&borderRect, true, true, false));
		auto virtualScreenX{ GetSystemMetrics(SM_XVIRTUALSCREEN) };
		auto virtualScreenY{ GetSystemMetrics(SM_YVIRTUALSCREEN) };
		auto virtualScreenCX{ GetSystemMetrics(SM_CXVIRTUALSCREEN) };
		auto virtualScreenCY{ GetSystemMetrics(SM_CYVIRTUALSCREEN) };
		if (
			borderRect.right <= virtualScreenX ||
			borderRect.bottom <= virtualScreenY ||
			borderRect.left >= virtualScreenX + virtualScreenCX ||
			borderRect.bottom >= virtualScreenY + virtualScreenCY ||
			!m_data->IsWindowVisibleAndUncloaked()
		)
		{
			return;
		}
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
	if (m_splitBlurRegionIntoChunks != Configuration::g_splitBlurRegionIntoChunks)
	{
		m_splitBlurRegionIntoChunks = Configuration::g_splitBlurRegionIntoChunks;
		forceUpdate = true;
	}
	auto includedBorder{ DoesBorderParticipateInBlurRegion() };
	CombineRgn(nonClientRgn.get(), m_captionRgn.get(), includedBorder ? m_borderRgn.get() : nullptr, includedBorder ? RGN_OR : RGN_COPY);
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
		else if (!m_containerVisual.IsVisible())
		{
			m_containerVisual.IsVisible(true);
		}

		m_containerVisual.Offset({ static_cast<float>(regionBox.left), static_cast<float>(regionBox.top), 0.f });
		m_containerVisual.Size({ static_cast<float>(max(wil::rect_width(regionBox), 0)), static_cast<float>(max(wil::rect_height(regionBox), 0)) });
		m_reflectionVisual->NotifyOffsetToWindow(m_containerVisual.Offset());
		OnLayoutUpdated();

		winrt::com_ptr<ID2D1Factory> factory{ nullptr };
		uDwm::CDesktopManager::s_pDesktopManagerInstance->GetD2DDevice()->GetFactory(factory.put());
		
		auto canvasGeometry
		{
			Win2D::CanvasGeometry::CreateGeometryFromHRGN(
				factory.get(),
				m_compositedRgn.get()
			)
		};
		// handle round corner
		if (!m_cloned)
		{
			if (m_window->HasNonClientBackground(m_data) && !IsMaximized(m_data->GetHwnd()))
			{
				m_window->GetActualWindowRect(m_windowBox.get(), true, true, true);
				UnionRect(m_windowBox.get(), m_windowBox.get(), &regionBox);
			}
			else
			{
				*m_windowBox = {};
			}
		}
		if (m_roundRectRadius != 0.f && !IsRectEmpty(m_windowBox.get()))
		{
			winrt::com_ptr<ID2D1Geometry> geometry{ nullptr };
			canvasGeometry->GetGeometry(geometry.put());
			winrt::com_ptr<ID2D1RoundedRectangleGeometry> roundedGeometry{ nullptr };
			THROW_IF_FAILED(
				factory->CreateRoundedRectangleGeometry(
					D2D1::RoundedRect(
						D2D1::RectF(
							static_cast<float>(m_windowBox->left),
							static_cast<float>(m_windowBox->top),
							static_cast<float>(m_windowBox->right),
							static_cast<float>(m_windowBox->bottom)
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
			canvasGeometry->SetGeometry(pathGeometry.get());
		}
		m_pathGeometry.Path(wuc::CompositionPath{ canvasGeometry.as<wg::IGeometrySource2D>() });
		forceUpdate = false;
	}
}
CATCH_LOG_RETURN()

void BackdropManager::CCompositedBackdropVisual::OnLayoutUpdated()
{
	auto children{ m_spriteVisual.Children() };
	if (IsBlurRegionSplittingAvailable())
	{
		if (m_spriteVisual.Brush())
		{
			m_spriteVisual.Brush(nullptr);
		}
		if (children.Count() == 0)
		{
			children.InsertAtTop(m_captionVisual);
			children.InsertAtTop(m_borderLeftVisual);
			children.InsertAtTop(m_borderBottomVisual);
			children.InsertAtTop(m_borderRightVisual);
			children.InsertAtTop(m_clientVisual);
		}

		RECT compositedBox{};
		GetRgnBox(m_compositedRgn.get(), &compositedBox);
		wil::unique_hrgn boxRgn{ CreateRectRgnIndirect(&compositedBox) };
		CombineRgn(boxRgn.get(), boxRgn.get(), m_captionRgn.get(), RGN_DIFF);
		CombineRgn(boxRgn.get(), boxRgn.get(), m_borderRgn.get(), RGN_DIFF);
		RECT clientBox{};
		GetRgnBox(boxRgn.get(), &clientBox);

		m_captionVisual.Offset(
			{
				static_cast<float>(clientBox.left - compositedBox.left),
				0.f,
				0.f
			}
		);
		m_captionVisual.Size(
			{
				static_cast<float>(wil::rect_width(clientBox)),
				static_cast<float>(clientBox.top - compositedBox.top)
			}
		);

		bool bordersVisible{ DoesBorderParticipateInBlurRegion() };
		if (bordersVisible)
		{
			m_borderLeftVisual.Size(
				{
					static_cast<float>(clientBox.left - compositedBox.left),
					0.f
				}
			);

			m_borderBottomVisual.Offset(
				{
					static_cast<float>(clientBox.left - compositedBox.left),
					static_cast<float>(clientBox.bottom - compositedBox.bottom),
					0.f
				}
			);
			m_borderBottomVisual.Size(
				{
					static_cast<float>(wil::rect_width(clientBox)),
					static_cast<float>(compositedBox.bottom - clientBox.bottom)
				}
			);

			m_borderRightVisual.Offset(
				{
					static_cast<float>(clientBox.right - compositedBox.right),
					0.f,
					0.f
				}
			);
			m_borderRightVisual.Size(
				{
					static_cast<float>(compositedBox.right - clientBox.right),
					0.f
				}
			);
		}
		m_borderLeftVisual.IsVisible(bordersVisible);
		m_borderBottomVisual.IsVisible(bordersVisible);
		m_borderRightVisual.IsVisible(bordersVisible);

		bool clientBlurVisible{ m_clientBlurRgn != nullptr };
		if (clientBlurVisible)
		{
			m_clientVisual.Offset(
				{
					static_cast<float>(clientBox.left - compositedBox.left),
					static_cast<float>(clientBox.top - compositedBox.top),
					0.f
				}
			);
			m_clientVisual.Size(
				{
					static_cast<float>(wil::rect_width(clientBox)),
					static_cast<float>(wil::rect_height(clientBox))
				}
			);
		}
		m_clientVisual.IsVisible(clientBlurVisible);
	}
	else
	{
		if (children.Count())
		{
			children.RemoveAll();
		}
		if (!m_spriteVisual.Brush())
		{
			m_spriteVisual.Brush(m_backdropBrush);
		}
	}
}

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
	m_reflectionVisual{ winrt::make_self<CGlassReflectionVisual>(m_window, m_data) },
	m_windowBox{ std::make_unique<RECT>() },
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
	m_reflectionVisual{ winrt::make_self<CGlassReflectionVisual>(m_window, m_data, true) },
	m_roundRectRadius{ backdrop->m_roundRectRadius },
	m_overrideBorder{ backdrop->m_overrideBorder },
	m_windowBox{ std::make_unique<RECT>(*backdrop->m_windowBox) },
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
	m_reflectionVisual->SyncReflectionData(*backdrop->m_reflectionVisual);
}
BackdropManager::CCompositedBackdropVisual::~CCompositedBackdropVisual()
{
	UninitializeVisual();
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

	if (m_containerVisual.IsVisible())
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
	m_reflectionVisual->ValidateVisual();
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
			data &&
			it == g_backdropMap.end()
		)
		{
			HWND targetWindow{ data->GetHwnd() };
			HWND shellWindow{ uDwm::GetShellWindowForCurrentDesktop() };
			DWORD explorerPid{ 0 }, targetPid{ 0 };
			GetWindowThreadProcessId(targetWindow, &targetPid);
			GetWindowThreadProcessId(shellWindow, &explorerPid);
			WCHAR targetClassName[MAX_PATH + 1]{};
			GetClassNameW(targetWindow, targetClassName, MAX_PATH);

			if (
				targetWindow != shellWindow &&
				(targetPid != explorerPid || (wcscmp(targetClassName, L"WorkerW") != 0 && wcscmp(targetClassName, L"Progman") != 0))
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