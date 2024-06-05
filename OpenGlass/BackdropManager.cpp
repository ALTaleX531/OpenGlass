#include "pch.h"
#include "GlassFramework.hpp"
#include "uDwmProjection.hpp"
#include "dcompProjection.hpp"
#include "BackdropManager.hpp"
#include "CanvasGeometry.hpp"
#include "GlassReflection.hpp"
#include "Utils.hpp"
#include "wucUtils.hpp"
#include "BackdropFactory.hpp"

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
			window->SetDirtyFlags(0x4000);
			window->SetDirtyFlags(0x10000);
			window->SetDirtyFlags(0x20000);
			window->SetDirtyFlags(0x40000);
			window->SetDirtyFlags(0x100000);
			window->SetDirtyFlags(0x400000);
			window->SetDirtyFlags(0x2000000);
			window->SetDirtyFlags(0x4000000);
			if (os::buildNumber >= os::build_w11_22h2)
			{
				if (kind == CompositedBackdropKind::SystemBackdrop)
				{
					window->OnSystemBackdropUpdated();
				}
			}
			if (kind == CompositedBackdropKind::Accent)
			{
				window->OnAccentPolicyUpdated();
			}
			window->OnClipUpdated();
		}
		__except (EXCEPTION_EXECUTE_HANDLER) {}
	}

	class CCompositedBackdropVisual : public winrt::implements<CCompositedBackdropVisual, ICompositedBackdropVisual>, uDwm::CBackdropVisual
	{
		bool m_backdropDataChanged{ false };
		bool m_backdropBrushChanged{ false };
		bool m_activate{ false };
		bool m_visible{ true };
		bool m_clipApplied{ false };
		DWORD m_state{};
		DWORD m_color{};
		DWORD m_accentFlags{};
		std::chrono::steady_clock::time_point m_backdropTimeStamp{};

		CompositedBackdropKind m_kind{ CompositedBackdropKind::None };
		uDwm::CTopLevelWindow* m_window{ nullptr };
		uDwm::CWindowData* m_data{ nullptr };

		wuc::CompositionBrush m_backdropBrush{ nullptr };
		wuc::CompositionBrush m_previousBackdropBrush{ nullptr };
		wuc::ContainerVisual m_wucRootVisual{ nullptr };
		wuc::ContainerVisual m_containerVisual{ nullptr };
		wuc::SpriteVisual m_spriteVisual{ nullptr };

		bool m_isWaitingForAnimationComplete{ false };
		bool m_shouldPlayCrossFadeAnimation{ false };
		wuc::CompositionScopedBatch m_waitingForAnimationCompleteBatch{ nullptr };
		winrt::event_token m_waitingForAnimationCompleteToken{};

		winrt::com_ptr<CGlassReflectionVisual> m_reflectionVisual{ nullptr };

		wil::unique_hrgn m_gdiWindowRgn{ nullptr };
		wil::unique_hrgn m_clientBlurRgn{ nullptr };
		wil::unique_hrgn m_borderRgn{ nullptr };
		RECT m_captionRect{};
		RECT m_windowRect{};
		std::optional<RECT> m_accentRect{ std::nullopt };
		wil::unique_hrgn m_compositedRgn{ nullptr };

		float m_cornerRadius{ 0.f };
		wuc::CompositionRoundedRectangleGeometry m_roundedGeometry{ nullptr };
		wuc::CompositionGeometricClip m_roundedClip{ nullptr };
		wuc::CompositionPathGeometry m_pathGeometry{ nullptr };

		HRESULT InitializeVisual() override;
		void UninitializeVisual() override;

		void OnDeviceLost();
		void HandleChanges();
		void OnRoundedClipUpdated();
		void OnBackdropKindUpdated(CompositedBackdropKind kind);
		void OnBackdropRegionChanged(wil::unique_hrgn& newBackdropRegion);
		void OnBackdropBrushUpdated();
		void OnBackdropBrushChanged();
		void CancelAnimationCompleteWait();

		POINT GetClientAreaOffset() const;
		bool DoesBorderParticipateInBackdropRegion() const
		{
			return Configuration::g_overrideBorder || m_data->IsFrameExtendedIntoClientAreaLRB();
		}
		bool ShouldUpdateBackdropRegion() const
		{
			// window probably minimized or not visible on the screen,
			// no need to update
			RECT borderRect{};
			THROW_HR_IF_NULL(E_INVALIDARG, m_window->GetActualWindowRect(&borderRect, false, true, false));
			auto virtualScreenX{ GetSystemMetrics(SM_XVIRTUALSCREEN) };
			auto virtualScreenY{ GetSystemMetrics(SM_YVIRTUALSCREEN) };
			auto virtualScreenCX{ GetSystemMetrics(SM_CXVIRTUALSCREEN) };
			auto virtualScreenCY{ GetSystemMetrics(SM_CYVIRTUALSCREEN) };
			if (
				borderRect.right <= virtualScreenX ||
				borderRect.bottom <= virtualScreenY ||
				borderRect.left >= virtualScreenX + virtualScreenCX ||
				borderRect.top >= virtualScreenY + virtualScreenCY ||
				!m_data->IsWindowVisibleAndUncloaked()
			)
			{
				return false;
			}
#ifdef _DEBUG
			OutputDebugStringW(
				std::format(
					L"borderRect: [{},{},{},{}]\n",
					borderRect.left,
					borderRect.top,
					borderRect.right,
					borderRect.bottom
				).c_str()
			);
#endif
			return true;
		}
		wil::unique_hrgn CompositeNewBackdropRegion() const
		{
			wil::unique_hrgn compositedRgn{ CreateRectRgn(0, 0, 0, 0) };
			wil::unique_hrgn nonClientRgn{ CreateRectRgn(0, 0, 0, 0) };
			wil::unique_hrgn realClientBlurRgn{ CreateRectRgn(0, 0, 0, 0) };
			wil::unique_hrgn captionRgn{ CreateRectRgnIndirect(&m_captionRect) };
			wil::unique_hrgn windowRgn{ CreateRectRgnIndirect(&m_windowRect) };

			auto includedBorder{ DoesBorderParticipateInBackdropRegion() };
			CombineRgn(nonClientRgn.get(), captionRgn.get(), includedBorder ? m_borderRgn.get() : nullptr, includedBorder ? RGN_OR : RGN_COPY);
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

				// TO-DO: round corner specialization for SIB task thumbnail window
			}

			CombineRgn(compositedRgn.get(), compositedRgn.get(), windowRgn.get(), RGN_AND);
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
		auto GetuDwmVisual() const
		{
			return m_udwmVisual.get();
		}
	};

	// temporary workaround for aero peek/live preview
	class CClonedPeekingBackdropVisual : public winrt::implements<CCompositedBackdropVisual, ICompositedBackdropVisual>, uDwm::CBackdropVisual
	{
		uDwm::CTopLevelWindow* m_window{ nullptr };
		uDwm::CWindowData* m_data{ nullptr };
		wuc::ContainerVisual m_containerVisual{ nullptr };
		wuc::CompositionGeometricClip m_roundedClip{ nullptr };
		wuc::CompositionRoundedRectangleGeometry m_roundedGeometry{ nullptr };
		winrt::com_ptr<CGlassReflectionVisual> m_reflectionVisual{ nullptr };

		HRESULT InitializeVisual() override
		{
			RETURN_IF_FAILED(uDwm::CBackdropVisual::InitializeVisual());

			auto compositor{ m_dcompDevice.as<wuc::Compositor>() };
			m_reflectionVisual->InitializeVisual(compositor);
			m_containerVisual = compositor.CreateContainerVisual();
			m_roundedGeometry = compositor.CreateRoundedRectangleGeometry();
			m_roundedClip = compositor.CreateGeometricClip(m_roundedGeometry);
			{
				HWND hwnd{ m_data->GetHwnd() };
				RECT windowRect{}, borderRect{};
				auto window{ m_data->GetWindow() };
				window->GetActualWindowRect(&windowRect, false, true, true);
				window->GetActualWindowRect(&borderRect, false, true, false);
				MARGINS margins{};
				window->GetBorderMargins(&margins);
				winrt::Windows::Foundation::Numerics::float3 offset
				{
					static_cast<float>(0.f),
					static_cast<float>(!IsMaximized(hwnd) ? 0.f : margins.cyTopHeight),
					1.f
				};
				winrt::Windows::Foundation::Numerics::float2 size
				{
					static_cast<float>(wil::rect_width(borderRect) + (IsMaximized(hwnd) ? margins.cxRightWidth + margins.cxLeftWidth : 0)),
					static_cast<float>(wil::rect_height(borderRect))
				};
				m_containerVisual.Offset(offset);
				m_containerVisual.Size(size);
				m_roundedGeometry.Size(size);
				m_roundedGeometry.CornerRadius(wfn::float2{ Configuration::g_roundRectRadius, Configuration::g_roundRectRadius });
				m_reflectionVisual->NotifyOffsetToWindow(offset);
			}
			m_containerVisual.Clip(m_roundedClip);
			m_containerVisual.Children().InsertAtTop(m_reflectionVisual->GetVisual());
			m_reflectionVisual->ValidateVisual();
			m_visualCollection.InsertAtBottom(m_containerVisual);

			return S_OK;
		}
		void UninitializeVisual() override
		{
			m_roundedClip = nullptr;
			m_roundedGeometry = nullptr;
			m_containerVisual = nullptr;

			m_reflectionVisual->UninitializeVisual();
			uDwm::CBackdropVisual::UninitializeVisual();
		}
		void OnDeviceLost()
		{
			UninitializeVisual();
			InitializeInteropDevice(uDwm::CDesktopManager::s_pDesktopManagerInstance->GetDCompDevice());
			InitializeVisual();
		}
	public:
		CClonedPeekingBackdropVisual(uDwm::CTopLevelWindow* source, uDwm::CTopLevelWindow* target) :
			m_window{ target },
			m_data{ source->GetData() },
			m_reflectionVisual{ winrt::make_self<CGlassReflectionVisual>(source, m_data, true) },
			uDwm::CBackdropVisual{ target->GetNonClientVisual() }
		{
			OnDeviceLost();
		}
		~CClonedPeekingBackdropVisual()
		{
			UninitializeVisual();
		}

		void SetClientBlurRegion(HRGN region) override {}
		void SetCaptionRegion(HRGN region) override {}
		void SetBorderRegion(HRGN region) override {}
		void SetAccentRect(LPCRECT lprc) override {}
		void SetGdiWindowRegion(HRGN region) override {}
		void ValidateVisual() override {}
		void UpdateNCBackground() override {}
	};
	class CClonedCompositedBackdropVisual : public winrt::implements<CClonedCompositedBackdropVisual, ICompositedBackdropVisual>, uDwm::CClonedBackdropVisual
	{
		winrt::com_ptr<CCompositedBackdropVisual> m_compositedBackdropVisual{ nullptr };
	public:
		CClonedCompositedBackdropVisual(uDwm::CTopLevelWindow* window, const CCompositedBackdropVisual* compositedBackdropVisual) :
			uDwm::CClonedBackdropVisual{ window->GetNonClientVisual(), compositedBackdropVisual->GetuDwmVisual() }
		{
			winrt::copy_from_abi(m_compositedBackdropVisual, compositedBackdropVisual);
			InitializeVisual();
		}
		virtual ~CClonedCompositedBackdropVisual()
		{
			UninitializeVisual();
		}

		void SetClientBlurRegion(HRGN region) override {}
		void SetCaptionRegion(HRGN region) override {}
		void SetBorderRegion(HRGN region) override {}
		void SetAccentRect(LPCRECT lprc) override {}
		void SetGdiWindowRegion(HRGN region) override {}
		void ValidateVisual() override {}
		void UpdateNCBackground() override {}
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
	m_wucRootVisual = compositor.CreateContainerVisual();
	m_containerVisual = compositor.CreateContainerVisual();
	m_spriteVisual = compositor.CreateSpriteVisual();
	m_spriteVisual.RelativeSizeAdjustment({ 1.f, 1.f });
	
	// prepare backdrop brush
	OnBackdropBrushUpdated();

	// if the effect brush is compiled during the maximize/minimize animation
	// then it will NOT show normally, that's because the compilation is asynchronous
	m_reflectionVisual->InitializeVisual(compositor);
	m_wucRootVisual.Children().InsertAtTop(m_containerVisual);
	m_containerVisual.Children().InsertAtBottom(m_spriteVisual);
	m_containerVisual.Children().InsertAtTop(m_reflectionVisual->GetVisual());
	m_pathGeometry = compositor.CreatePathGeometry();
	m_roundedGeometry = compositor.CreateRoundedRectangleGeometry();
	m_roundedClip = compositor.CreateGeometricClip(m_roundedGeometry);
	
	winrt::com_ptr<ID2D1Factory> factory{ nullptr };
	uDwm::CDesktopManager::s_pDesktopManagerInstance->GetD2DDevice()->GetFactory(factory.put());
	// since build 22621, Path propertie cannot be empty or the dwmcore will raise a null pointer exception
	m_pathGeometry.Path(wuc::CompositionPath{ Win2D::CanvasGeometry::CreateGeometryFromHRGN(factory.get(), m_compositedRgn.get()).as<wg::IGeometrySource2D>() });

	m_dcompVisual.as<wuc::Visual>().Clip(compositor.CreateGeometricClip(m_pathGeometry));
	m_visualCollection.InsertAtBottom(m_wucRootVisual);

	return S_OK;
}

void BackdropManager::CCompositedBackdropVisual::UninitializeVisual()
{
	if (m_isWaitingForAnimationComplete)
	{
		CancelAnimationCompleteWait();
	}

	m_roundedGeometry = nullptr;
	m_pathGeometry = nullptr;
	m_spriteVisual = nullptr;
	m_wucRootVisual = nullptr;
	m_containerVisual = nullptr;
	m_backdropBrush = nullptr;
	m_previousBackdropBrush = nullptr;

	m_reflectionVisual->UninitializeVisual();
	uDwm::CBackdropVisual::UninitializeVisual();
}

void BackdropManager::CCompositedBackdropVisual::OnBackdropRegionChanged(wil::unique_hrgn& newBackdropRegion)
{
	m_compositedRgn = std::move(newBackdropRegion);

	bool isVisible{};
	RECT regionBox{};
	if (
		GetRgnBox(m_compositedRgn.get(), &regionBox) == NULLREGION || 
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
		m_containerVisual.IsVisible(isVisible);
		m_visible = isVisible;
	}

	wfn::float3 offset{ static_cast<float>(regionBox.left), static_cast<float>(regionBox.top), 0.f };
	m_containerVisual.Offset(offset);
	m_containerVisual.Size({ static_cast<float>(max(wil::rect_width(regionBox), 0)), static_cast<float>(max(wil::rect_height(regionBox), 0)) });
	m_reflectionVisual->NotifyOffsetToWindow(offset);

	winrt::com_ptr<ID2D1Factory> factory{ nullptr };
	uDwm::CDesktopManager::s_pDesktopManagerInstance->GetD2DDevice()->GetFactory(factory.put());
	auto canvasGeometry
	{
		Win2D::CanvasGeometry::CreateGeometryFromHRGN(
			factory.get(),
			m_compositedRgn.get()
		)
	};
	m_pathGeometry.Path(wuc::CompositionPath{ canvasGeometry.as<wg::IGeometrySource2D>() });
}

void BackdropManager::CCompositedBackdropVisual::OnBackdropBrushUpdated()
{
	m_backdropBrushChanged = false;
	m_shouldPlayCrossFadeAnimation = false;
	if (m_kind == CompositedBackdropKind::Accent)
	{
		auto policy{ m_data->GetAccentPolicy() };
		if (
			policy->AccentState != m_state ||
			policy->dwGradientColor != m_color ||
			policy->AccentFlags != m_accentFlags
		)
		{
			m_state = policy->AccentState;
			m_color = policy->dwGradientColor;
			m_accentFlags = policy->AccentFlags;
			m_backdropBrushChanged = true;
		}

		if (m_activate != true)
		{
			m_activate = true;
			m_backdropBrushChanged = true;
		}
	}
	else
	{
		auto active{ m_window->TreatAsActiveWindow() };
		if (active != m_activate)
		{
			m_shouldPlayCrossFadeAnimation = true;
			m_activate = active;
			m_backdropBrushChanged = true;
		}

		auto color{ m_window->GetCurrentColorizationColor() };
		if (color != m_color)
		{
			m_shouldPlayCrossFadeAnimation = true;
			m_color = color;
			m_backdropBrushChanged = true;
		}
	}

	auto timeStamp{ BackdropFactory::GetBackdropBrushTimeStamp() };
	if (m_backdropTimeStamp != timeStamp)
	{
		m_backdropTimeStamp = timeStamp;
		m_backdropBrushChanged = true;
	}

	if (m_backdropBrushChanged || !m_backdropBrush)
	{
		m_backdropBrush = BackdropFactory::GetOrCreateBackdropBrush(
			m_dcompDevice.as<wuc::Compositor>(),
			m_color,
			m_activate,
			m_kind == CompositedBackdropKind::Accent ? m_data->GetAccentPolicy() : nullptr
		);
		OnBackdropBrushChanged();
	}
}

void BackdropManager::CCompositedBackdropVisual::OnBackdropBrushChanged()
{
	if (m_previousBackdropBrush != m_backdropBrush)
	{
		if (!m_previousBackdropBrush)
		{
			m_shouldPlayCrossFadeAnimation = false;
		}
		if (m_isWaitingForAnimationComplete)
		{
			CancelAnimationCompleteWait();
		}

		if (m_shouldPlayCrossFadeAnimation && Configuration::g_crossfadeTime.count())
		{
			auto compositor{ m_backdropBrush.Compositor() };
			auto crossfadeBrush{ Utils::CreateCrossFadeBrush(compositor, m_previousBackdropBrush, m_backdropBrush) };

			m_isWaitingForAnimationComplete = true;
			m_waitingForAnimationCompleteBatch = compositor.CreateScopedBatch(wuc::CompositionBatchTypes::Animation);
			{
				auto strongThis{ get_strong() };
				auto handler = [strongThis](auto sender, auto args)
				{
					strongThis->CancelAnimationCompleteWait();
					strongThis->m_spriteVisual.Brush(strongThis->m_backdropBrush);
				};
				Utils::ThisModule_AddRef();
				crossfadeBrush.StartAnimation(L"Crossfade.Weight", Utils::CreateCrossFadeAnimation(compositor, Configuration::g_crossfadeTime));
				m_waitingForAnimationCompleteBatch.End();
				m_waitingForAnimationCompleteToken = m_waitingForAnimationCompleteBatch.Completed(handler);
			}

			m_spriteVisual.Brush(crossfadeBrush);
		}
		else
		{
			m_spriteVisual.Brush(m_backdropBrush);
		}
		m_previousBackdropBrush = m_backdropBrush;
	}
}

void BackdropManager::CCompositedBackdropVisual::CancelAnimationCompleteWait()
{
	if (m_waitingForAnimationCompleteBatch)
	{
		m_waitingForAnimationCompleteBatch.Completed(m_waitingForAnimationCompleteToken);
		m_waitingForAnimationCompleteBatch = nullptr;
		m_waitingForAnimationCompleteToken = {};
	}
	if (m_isWaitingForAnimationComplete)
	{
		m_isWaitingForAnimationComplete = false;
		Utils::ThisModule_Release();
	}
}

// this implementation sucks because it is inefficient
// it always updates the size and offset of the geometry when ValidateVisual is called
void BackdropManager::CCompositedBackdropVisual::OnRoundedClipUpdated()
{
	if (m_cornerRadius != Configuration::g_roundRectRadius)
	{
		m_cornerRadius = Configuration::g_roundRectRadius;
		m_roundedGeometry.CornerRadius({ m_cornerRadius, m_cornerRadius });
	}

	auto update_if_round_rect_clip_applicable = [&]() -> bool
	{
		if (Configuration::g_roundRectRadius == 0.f)
		{
			return false;
		}
		if (!m_window->HasNonClientBackground(m_data) || IsMaximized(m_data->GetHwnd()))
		{
			return false;
		}

		RECT windowBox{};
		m_window->GetActualWindowRect(&windowBox, true, true, true);
		if (IsRectEmpty(&windowBox))
		{
			return false;
		}

		RECT box{};
		if (GetRgnBox(m_compositedRgn.get(), &box) == NULLREGION)
		{
			return false;
		}

		m_roundedGeometry.Size({ static_cast<float>(wil::rect_width(windowBox)), static_cast<float>(wil::rect_height(windowBox)) });
		m_roundedGeometry.Offset({ static_cast<float>(box.left), static_cast<float>(box.top) });
		return true;
	};

	auto applicable{ update_if_round_rect_clip_applicable() };
	if (applicable != m_clipApplied)
	{
		m_wucRootVisual.Clip(applicable ? m_roundedClip : nullptr);
		m_clipApplied = applicable;
	}
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

void BackdropManager::CCompositedBackdropVisual::OnDeviceLost()
{
	UninitializeVisual();
	InitializeInteropDevice(uDwm::CDesktopManager::s_pDesktopManagerInstance->GetDCompDevice());
	InitializeVisual();
	m_backdropDataChanged = true;
}

BackdropManager::CCompositedBackdropVisual::CCompositedBackdropVisual(uDwm::CTopLevelWindow* window) :
	m_window{ window },
	m_data{ window->GetData() },
	m_reflectionVisual{ winrt::make_self<CGlassReflectionVisual>(m_window, m_data) },
	uDwm::CBackdropVisual{ window->GetNonClientVisual() }
{
	m_borderRgn.reset(CreateRectRgn(0, 0, 0, 0));
	m_compositedRgn.reset(CreateRectRgn(0, 0, 0, 0));

	OnDeviceLost();
	OnBackdropKindUpdated(GlassFramework::GetActualBackdropKind(window));
}

BackdropManager::CCompositedBackdropVisual::~CCompositedBackdropVisual()
{
	UninitializeVisual();
}

void BackdropManager::CCompositedBackdropVisual::OnBackdropKindUpdated(CompositedBackdropKind kind)
{
	if (m_kind != kind)
	{
		m_kind = kind;
		m_backdropDataChanged = true;
		m_previousBackdropBrush = nullptr;
		m_backdropBrush = nullptr;
		m_backdropBrushChanged = true;
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
	}
	else
	{
		m_accentRect = std::nullopt;
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
	}
	else
	{
		m_gdiWindowRgn.reset();
	}
	m_backdropDataChanged = true;
}

void BackdropManager::CCompositedBackdropVisual::ValidateVisual()
{
	if (ShouldUpdateBackdropRegion())
	{
		if (m_visible)
		{
			BOOL valid{ FALSE };
			if (!uDwm::CheckDeviceState(m_dcompDevice))
			{
				OnDeviceLost();
			}
		}

		OnBackdropKindUpdated(GlassFramework::GetActualBackdropKind(m_window));
		if (m_backdropDataChanged) { HandleChanges(); }
		if (m_visible)
		{
			OnBackdropBrushUpdated();
			OnRoundedClipUpdated();
			m_reflectionVisual->ValidateVisual();
		}
	}
}
void BackdropManager::CCompositedBackdropVisual::UpdateNCBackground()
{
	if (!ShouldUpdateBackdropRegion())
	{
		return;
	}

	RECT borderRect{};
	THROW_HR_IF_NULL(E_INVALIDARG, m_window->GetActualWindowRect(&borderRect, true, true, false));

	if (!EqualRect(&m_windowRect, &borderRect))
	{
		m_windowRect = borderRect;
		m_backdropDataChanged = true;
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

void BackdropManager::TryCloneBackdropVisualForWindow(uDwm::CTopLevelWindow* src, uDwm::CTopLevelWindow* dst, ICompositedBackdropVisual** visual)
{
	auto legacyVisual{ src->GetLegacyVisual() };
	if (auto backdrop{ GetOrCreateBackdropVisual(src) }; backdrop && legacyVisual)
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