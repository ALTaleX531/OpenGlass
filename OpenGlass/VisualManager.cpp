#include "pch.h"
#include "GlassFramework.hpp"
#include "uDwmProjection.hpp"
#include "VisualManager.hpp"
#include "Shared.hpp"
#include "Utils.hpp"

using namespace OpenGlass;
namespace OpenGlass::VisualManager
{
	std::unordered_map<uDwm::CTopLevelWindow*, winrt::com_ptr<ILegacyVisualOverrider>> g_visualMap{};
	std::unordered_map<uDwm::CAnimatedGlassSheet*, winrt::com_ptr<IAnimatedGlassSheetOverrider>> g_sheetMap{};

	class CLegacyVisualOverrider : public winrt::implements<CLegacyVisualOverrider, ILegacyVisualOverrider>
	{
		bool m_initialized{ false };
		uDwm::CTopLevelWindow* m_window{ nullptr };
		winrt::com_ptr<uDwm::CSolidColorLegacyMilBrushProxy> m_brush{ nullptr };
		winrt::com_ptr<uDwm::CRgnGeometryProxy> m_geometry{ nullptr };
		winrt::com_ptr<uDwm::CRgnGeometryProxy> m_captionRgnGeometry{ nullptr };
		winrt::com_ptr<uDwm::CRgnGeometryProxy> m_topBorderRgnGeometry{ nullptr };
		winrt::com_ptr<uDwm::CRgnGeometryProxy> m_leftBorderRgnGeometry{ nullptr };
		winrt::com_ptr<uDwm::CRgnGeometryProxy> m_bottomBorderRgnGeometry{ nullptr };
		winrt::com_ptr<uDwm::CRgnGeometryProxy> m_rightBorderRgnGeometry{ nullptr };
		winrt::com_ptr<uDwm::CDrawGeometryInstruction> m_instruction{ nullptr };
		winrt::com_ptr<uDwm::CDrawGeometryInstruction> m_captionDrawInstruction{ nullptr };
		winrt::com_ptr<uDwm::CDrawGeometryInstruction> m_topBorderDrawInstruction{ nullptr };
		winrt::com_ptr<uDwm::CDrawGeometryInstruction> m_leftBorderDrawInstruction{ nullptr };
		winrt::com_ptr<uDwm::CDrawGeometryInstruction> m_bottomBorderDrawInstruction{ nullptr };
		winrt::com_ptr<uDwm::CDrawGeometryInstruction> m_rightBorderDrawInstruction{ nullptr };

		HRESULT Initialize();
	public:
		CLegacyVisualOverrider(uDwm::CTopLevelWindow* window);
		virtual ~CLegacyVisualOverrider();
		HRESULT STDMETHODCALLTYPE UpdateNCBackground(
			HRGN captionRgn,
			HRGN borderRgn
		) override;
	};
	class CAnimatedGlassSheetOverrider : public winrt::implements<CAnimatedGlassSheetOverrider, IAnimatedGlassSheetOverrider>
	{
		bool m_initialized{ false };
		uDwm::CAnimatedGlassSheet* m_sheet{ nullptr };
		winrt::com_ptr<uDwm::CCanvasVisual> m_visual{ nullptr };
		winrt::com_ptr<uDwm::CRgnGeometryProxy> m_geometry{ nullptr };
		RECT m_offsets{};

		HRESULT Initialize()
		{
			RETURN_IF_FAILED(
				uDwm::CCanvasVisual::Create(
					m_visual.put()
				)
			);
			winrt::com_ptr<uDwm::CSolidColorLegacyMilBrushProxy> brush{ nullptr };
			RETURN_IF_FAILED(
				uDwm::CDesktopManager::s_pDesktopManagerInstance->GetCompositor()->CreateSolidColorLegacyMilBrushProxy(
					brush.put()
				)
			);
			RETURN_IF_FAILED(brush->Update(1.0, D2D1::ColorF(0x000000, 0.25f)));

			wil::unique_hrgn emptyRegion{ CreateRectRgn(0, 0, 0, 0) };
			RETURN_LAST_ERROR_IF_NULL(emptyRegion);

			RETURN_IF_FAILED(
				uDwm::ResourceHelper::CreateGeometryFromHRGN(
					emptyRegion.get(),
					m_geometry.put()
				)
			);
			winrt::com_ptr<uDwm::CDrawGeometryInstruction> instruction{ nullptr };
			RETURN_IF_FAILED(
				uDwm::CDrawGeometryInstruction::Create(
					brush.get(),
					m_geometry.get(),
					instruction.put()
				)
			);
			RETURN_IF_FAILED(m_visual->AddInstruction(instruction.get()));
			RETURN_IF_FAILED(
				m_sheet->GetVisualCollection()->InsertRelative(
					m_visual.get(),
					nullptr,
					false,
					true
				)
			);

			m_initialized = true;
			return S_OK;
		}
	public:
		CAnimatedGlassSheetOverrider(uDwm::CAnimatedGlassSheet* sheet) : m_sheet{ sheet } {};
		virtual ~CAnimatedGlassSheetOverrider()
		{
			if (m_initialized && m_visual)
			{
				LOG_IF_FAILED(m_sheet->GetVisualCollection()->Remove(m_visual.get()));
			}
		}
		HRESULT STDMETHODCALLTYPE OnRectUpdated(LPCRECT lprc) override
		{
			if (!m_initialized)
			{
				RETURN_IF_FAILED(Initialize());
			}

			RETURN_IF_FAILED(
				uDwm::ResourceHelper::CreateGeometryFromHRGN(
					wil::unique_hrgn
					{ 
						CreateRoundRectRgn(
							0 - m_sheet->GetAtlasPaddingLeft(),
							0 - m_sheet->GetAtlasPaddingTop(),
							wil::rect_width(*lprc) - m_sheet->GetAtlasPaddingRight(),
							wil::rect_height(*lprc) - m_sheet->GetAtlasPaddingBottom(),
							Shared::g_roundRectRadius,
							Shared::g_roundRectRadius
						) 
					}.get(), 
					reinterpret_cast<uDwm::CRgnGeometryProxy**>(&m_geometry)
				)
			);
			return S_OK;
		}
	};
}

void VisualManager::RedrawTopLevelWindow(uDwm::CTopLevelWindow* window)
{
	if (auto clientBlurVisual = window->GetClientBlurVisual(); clientBlurVisual)
	{
		LOG_IF_FAILED(clientBlurVisual->ClearInstructions());
	}
	if (os::buildNumber >= os::build_w11_21h2)
	{
		LOG_IF_FAILED(window->OnSystemBackdropUpdated());
	}
	// 0x10000 UpdateText
	// 0x20000 UpdateIcon
	window->SetDirtyFlags(window->GetDirtyFlags() | 0x8 | 0x4000 | 0x10000 | 0x20000 | 0x80000 | 0x2000000 | 0x400000 | 0x4000000);
}

VisualManager::CLegacyVisualOverrider::CLegacyVisualOverrider(uDwm::CTopLevelWindow* window) : m_window{ window }
{
	RedrawTopLevelWindow(window);
}

VisualManager::CLegacyVisualOverrider::~CLegacyVisualOverrider()
{
	auto legacyVisual = m_window->GetLegacyVisualAddress();
	if (*legacyVisual)
	{
		m_window->GetNonClientVisual()->GetVisualCollection()->Remove(*legacyVisual);
		(*legacyVisual)->Release();
		*legacyVisual = nullptr;
	}
}

HRESULT VisualManager::CLegacyVisualOverrider::Initialize()
{
	RETURN_IF_FAILED(
		uDwm::CDesktopManager::s_pDesktopManagerInstance->GetCompositor()->CreateSolidColorLegacyMilBrushProxy(
			m_brush.put()
		)
	);
	wil::unique_hrgn emptyRegion{ CreateRectRgn(0, 0, 0, 0) };
	RETURN_LAST_ERROR_IF_NULL(emptyRegion);

	RETURN_IF_FAILED(
		uDwm::ResourceHelper::CreateGeometryFromHRGN(
			emptyRegion.get(),
			m_geometry.put()
		)
	);
	RETURN_IF_FAILED(
		uDwm::CDrawGeometryInstruction::Create(
			m_brush.get(),
			m_geometry.get(),
			m_instruction.put()
		)
	);
	// caption
	RETURN_IF_FAILED(
		uDwm::ResourceHelper::CreateGeometryFromHRGN(
			emptyRegion.get(),
			m_captionRgnGeometry.put()
		)
	);
	RETURN_IF_FAILED(
		uDwm::CDrawGeometryInstruction::Create(
			m_brush.get(),
			m_captionRgnGeometry.get(),
			m_captionDrawInstruction.put()
		)
	);
	// top
	RETURN_IF_FAILED(
		uDwm::ResourceHelper::CreateGeometryFromHRGN(
			emptyRegion.get(),
			m_topBorderRgnGeometry.put()
		)
	);
	RETURN_IF_FAILED(
		uDwm::CDrawGeometryInstruction::Create(
			m_brush.get(),
			m_topBorderRgnGeometry.get(),
			m_topBorderDrawInstruction.put()
		)
	);
	// left
	RETURN_IF_FAILED(
		uDwm::ResourceHelper::CreateGeometryFromHRGN(
			emptyRegion.get(),
			m_leftBorderRgnGeometry.put()
		)
	);
	RETURN_IF_FAILED(
		uDwm::CDrawGeometryInstruction::Create(
			m_brush.get(),
			m_leftBorderRgnGeometry.get(),
			m_leftBorderDrawInstruction.put()
		)
	);
	// bottom
	RETURN_IF_FAILED(
		uDwm::ResourceHelper::CreateGeometryFromHRGN(
			emptyRegion.get(),
			m_bottomBorderRgnGeometry.put()
		)
	);
	RETURN_IF_FAILED(
		uDwm::CDrawGeometryInstruction::Create(
			m_brush.get(),
			m_bottomBorderRgnGeometry.get(),
			m_bottomBorderDrawInstruction.put()
		)
	);
	// right
	RETURN_IF_FAILED(
		uDwm::ResourceHelper::CreateGeometryFromHRGN(
			emptyRegion.get(),
			m_rightBorderRgnGeometry.put()
		)
	);
	RETURN_IF_FAILED(
		uDwm::CDrawGeometryInstruction::Create(
			m_brush.get(),
			m_rightBorderRgnGeometry.get(),
			m_rightBorderDrawInstruction.put()
		)
	);

	m_initialized = true;
	return S_OK;
}

HRESULT STDMETHODCALLTYPE VisualManager::CLegacyVisualOverrider::UpdateNCBackground(
	HRGN captionRgn,
	HRGN borderRgn
)
{
	if (!m_initialized)
	{
		RETURN_IF_FAILED(Initialize());
	}
	auto legacyVisual = m_window->GetLegacyVisual();
	if (!m_window->GetData()->IsWindowVisibleAndUncloaked() || m_window->IsTrullyMinimized())
	{
		return S_OK;
	}
	RETURN_IF_FAILED(legacyVisual->ClearInstructions());
	if (!Shared::g_enableGeometryMerging)
	{
		RETURN_IF_FAILED(legacyVisual->AddInstruction(m_captionDrawInstruction.get()));
		RETURN_IF_FAILED(legacyVisual->AddInstruction(m_topBorderDrawInstruction.get()));
		RETURN_IF_FAILED(legacyVisual->AddInstruction(m_leftBorderDrawInstruction.get()));
		RETURN_IF_FAILED(legacyVisual->AddInstruction(m_bottomBorderDrawInstruction.get()));
		RETURN_IF_FAILED(legacyVisual->AddInstruction(m_rightBorderDrawInstruction.get()));
	}
	else
	{
		RETURN_IF_FAILED(legacyVisual->AddInstruction(m_instruction.get()));
	}

	auto color = 
		Shared::g_forceAccentColorization ?
		dwmcore::Convert_D2D1_COLOR_F_sRGB_To_D2D1_COLOR_F_scRGB(m_window->TreatAsActiveWindow() ? Shared::g_accentColor : Shared::g_accentColorInactive) :
		m_window->GetTitlebarColorizationParameters()->getArgbcolor();
	color.a = m_window->TreatAsActiveWindow() ? 0.5f : 0.0f;

	RETURN_IF_FAILED(m_brush->Update(1.0, color));

	wil::unique_hrgn emptyRegion{ CreateRectRgn(0, 0, 0, 0) };
	if (!Shared::g_enableGeometryMerging)
	{
		if (m_window->GetData()->IsFullGlass())
		{
			RETURN_IF_FAILED(
				uDwm::ResourceHelper::CreateGeometryFromHRGN(
					borderRgn,
					reinterpret_cast<uDwm::CRgnGeometryProxy**>(&m_captionRgnGeometry)
				)
			);
			RETURN_IF_FAILED(
				uDwm::ResourceHelper::CreateGeometryFromHRGN(
					emptyRegion.get(),
					reinterpret_cast<uDwm::CRgnGeometryProxy**>(&m_topBorderRgnGeometry)
				)
			);
			RETURN_IF_FAILED(
				uDwm::ResourceHelper::CreateGeometryFromHRGN(
					emptyRegion.get(),
					reinterpret_cast<uDwm::CRgnGeometryProxy**>(&m_leftBorderRgnGeometry)
				)
			);
			RETURN_IF_FAILED(
				uDwm::ResourceHelper::CreateGeometryFromHRGN(
					emptyRegion.get(),
					reinterpret_cast<uDwm::CRgnGeometryProxy**>(&m_bottomBorderRgnGeometry)
				)
			);
			RETURN_IF_FAILED(
				uDwm::ResourceHelper::CreateGeometryFromHRGN(
					emptyRegion.get(),
					reinterpret_cast<uDwm::CRgnGeometryProxy**>(&m_rightBorderRgnGeometry)
				)
			);

			return S_OK;
		}

		RECT borderBox{};
		GetRgnBox(borderRgn, &borderBox);
		RECT captionBox{};
		GetRgnBox(captionRgn, &captionBox);

		RETURN_IF_FAILED(
			uDwm::ResourceHelper::CreateGeometryFromHRGN(
				captionRgn,
				reinterpret_cast<uDwm::CRgnGeometryProxy**>(&m_captionRgnGeometry)
			)
		);
		wil::unique_hrgn borderPartRgn // top border
		{
			CreateRectRgn(
				borderBox.left,
				borderBox.top,
				borderBox.right,
				captionBox.top
			)
		};
		RETURN_LAST_ERROR_IF_NULL(borderPartRgn);
		CombineRgn(borderPartRgn.get(), borderPartRgn.get(), borderRgn, RGN_AND);
		RETURN_IF_FAILED(
			uDwm::ResourceHelper::CreateGeometryFromHRGN(
				borderPartRgn.get(),
				reinterpret_cast<uDwm::CRgnGeometryProxy**>(&m_topBorderRgnGeometry)
			)
		);

		borderPartRgn.reset( // left border 
			CreateRectRgn(
				borderBox.left,
				borderBox.top,
				captionBox.left,
				borderBox.bottom
			)
		);
		RETURN_LAST_ERROR_IF_NULL(borderPartRgn);
		CombineRgn(borderPartRgn.get(), borderPartRgn.get(), borderRgn, RGN_AND);
		RETURN_IF_FAILED(
			uDwm::ResourceHelper::CreateGeometryFromHRGN(
				borderPartRgn.get(),
				reinterpret_cast<uDwm::CRgnGeometryProxy**>(&m_leftBorderRgnGeometry)
			)
		);

		borderPartRgn.reset( // bottom border
			CreateRectRgn(
				captionBox.left,
				captionBox.bottom,
				captionBox.right,
				borderBox.bottom
			)
		);
		RETURN_LAST_ERROR_IF_NULL(borderPartRgn);
		CombineRgn(borderPartRgn.get(), borderPartRgn.get(), borderRgn, RGN_AND);
		RETURN_IF_FAILED(
			uDwm::ResourceHelper::CreateGeometryFromHRGN(
				borderPartRgn.get(),
				reinterpret_cast<uDwm::CRgnGeometryProxy**>(&m_bottomBorderRgnGeometry)
			)
		);

		borderPartRgn.reset( // right border
			CreateRectRgn(
				captionBox.right,
				borderBox.top,
				borderBox.right,
				borderBox.bottom
			)
		);
		RETURN_LAST_ERROR_IF_NULL(borderPartRgn);
		CombineRgn(borderPartRgn.get(), borderPartRgn.get(), borderRgn, RGN_AND);
		RETURN_IF_FAILED(
			uDwm::ResourceHelper::CreateGeometryFromHRGN(
				borderPartRgn.get(),
				reinterpret_cast<uDwm::CRgnGeometryProxy**>(&m_rightBorderRgnGeometry)
			)
		);
	}
	else
	{
		CombineRgn(emptyRegion.get(), captionRgn, borderRgn, RGN_OR);
		RETURN_IF_FAILED(
			uDwm::ResourceHelper::CreateGeometryFromHRGN(
				emptyRegion.get(),
				reinterpret_cast<uDwm::CRgnGeometryProxy**>(&m_geometry)
			)
		);
	}

	return S_OK;
}

winrt::com_ptr<ILegacyVisualOverrider> VisualManager::LegacyVisualOverrider::GetOrCreate(uDwm::CTopLevelWindow* window, bool createIfNecessary)
{
	auto it = g_visualMap.find(window);

	if (createIfNecessary)
	{
		auto data = window->GetData();

		if (
			data &&
			it == g_visualMap.end()
		)
		{
			auto result = g_visualMap.emplace(window, winrt::make<CLegacyVisualOverrider>(window));
			if (result.second == true)
			{
				it = result.first;
			}
		}
	}

	return it == g_visualMap.end() ? nullptr : it->second;
}

void VisualManager::LegacyVisualOverrider::Remove(uDwm::CTopLevelWindow* window)
{
	auto it = g_visualMap.find(window);

	if (it != g_visualMap.end())
	{
		g_visualMap.erase(it);
	}
}

void VisualManager::LegacyVisualOverrider::Shutdown()
{
	g_visualMap.clear();
}



winrt::com_ptr<IAnimatedGlassSheetOverrider> VisualManager::AnimatedGlassSheetOverrider::GetOrCreate(uDwm::CAnimatedGlassSheet* sheet, bool createIfNecessary)
{
	auto it = g_sheetMap.find(sheet);

	if (createIfNecessary)
	{
		if (it == g_sheetMap.end())
		{
			auto result = g_sheetMap.emplace(sheet, winrt::make<CAnimatedGlassSheetOverrider>(sheet));
			if (result.second == true)
			{
				it = result.first;
			}
		}
	}

	return it == g_sheetMap.end() ? nullptr : it->second;
}

void VisualManager::AnimatedGlassSheetOverrider::Remove(uDwm::CAnimatedGlassSheet* sheet)
{
	auto it = g_sheetMap.find(sheet);

	if (it != g_sheetMap.end())
	{
		g_sheetMap.erase(it);
	}
}

void VisualManager::AnimatedGlassSheetOverrider::Shutdown()
{
	g_sheetMap.clear();
}