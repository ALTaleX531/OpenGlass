#include "pch.h"
#include "GlassFramework.hpp"
#include "uDwmProjection.hpp"
#include "VisualManager.hpp"
#include "Utils.hpp"

using namespace OpenGlass;
namespace OpenGlass::VisualManager
{
	std::unordered_map<uDwm::CTopLevelWindow*, winrt::com_ptr<ILegacyVisualOverrider>> g_visualMap{};

	class CLegacyVisualOverrider : public winrt::implements<CLegacyVisualOverrider, ILegacyVisualOverrider>
	{
		bool m_initialized{ false };
		uDwm::CTopLevelWindow* m_window{ nullptr };
		winrt::com_ptr<uDwm::CSolidColorLegacyMilBrushProxy> m_brush{ nullptr };
		winrt::com_ptr<uDwm::CRgnGeometryProxy> m_captionRgnGeometry{ nullptr };
		winrt::com_ptr<uDwm::CRgnGeometryProxy> m_topBorderRgnGeometry{ nullptr };
		winrt::com_ptr<uDwm::CRgnGeometryProxy> m_leftBorderRgnGeometry{ nullptr };
		winrt::com_ptr<uDwm::CRgnGeometryProxy> m_bottomBorderRgnGeometry{ nullptr };
		winrt::com_ptr<uDwm::CRgnGeometryProxy> m_rightBorderRgnGeometry{ nullptr };
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
	void RedrawTopLevelWindow(uDwm::CTopLevelWindow* window);
}

void VisualManager::RedrawTopLevelWindow(uDwm::CTopLevelWindow* window)
{
	// 0x10000 UpdateText
	// 0x20000 UpdateIcon
	window->SetDirtyFlags(window->GetDirtyFlags() | 0x10000 | 0x20000 | 0x4000 | 0x400000);
	if (os::buildNumber >= os::build_w11_21h2)
	{
		LOG_IF_FAILED(window->OnSystemBackdropUpdated());
	}
	LOG_IF_FAILED(window->OnAccentPolicyUpdated());
	LOG_IF_FAILED(window->OnClipUpdated());
}

VisualManager::CLegacyVisualOverrider::CLegacyVisualOverrider(uDwm::CTopLevelWindow* window) : m_window{ window }
{
	RedrawTopLevelWindow(window);
}

VisualManager::CLegacyVisualOverrider::~CLegacyVisualOverrider()
{
	auto legacyVisual{ m_window->GetLegacyVisualAddress() };
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
	auto legacyVisual{ m_window->GetLegacyVisual() };
	RETURN_IF_FAILED(legacyVisual->ClearInstructions());
	if (!captionRgn || !borderRgn || !m_window->GetData()->IsWindowVisibleAndUncloaked() || m_window->IsTrullyMinimized())
	{
		return S_OK;
	}
	RETURN_IF_FAILED(legacyVisual->AddInstruction(m_captionDrawInstruction.get()));
	RETURN_IF_FAILED(legacyVisual->AddInstruction(m_topBorderDrawInstruction.get()));
	RETURN_IF_FAILED(legacyVisual->AddInstruction(m_leftBorderDrawInstruction.get()));
	RETURN_IF_FAILED(legacyVisual->AddInstruction(m_bottomBorderDrawInstruction.get()));
	RETURN_IF_FAILED(legacyVisual->AddInstruction(m_rightBorderDrawInstruction.get()));

	auto color{ m_window->GetTitlebarColorizationParameters()->getArgbcolor() };
	color.a *= 0.99f;
	RETURN_IF_FAILED(m_brush->Update(1.0, color));

	wil::unique_hrgn emptyRegion{ CreateRectRgn(0, 0, 0, 0) };
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

	return S_OK;
}

winrt::com_ptr<VisualManager::ILegacyVisualOverrider> VisualManager::GetOrCreateLegacyVisualOverrider(uDwm::CTopLevelWindow* window, bool createIfNecessary)
{
	auto it{ g_visualMap.find(window) };

	if (createIfNecessary)
	{
		auto data{ window->GetData() };

		if (
			data &&
			it == g_visualMap.end()
		)
		{
			auto result{ g_visualMap.emplace(window, winrt::make<CLegacyVisualOverrider>(window)) };
			if (result.second == true)
			{
				it = result.first;
			}
		}
	}

	return it == g_visualMap.end() ? nullptr : it->second;
}

void VisualManager::RemoveLegacyVisualOverrider(uDwm::CTopLevelWindow* window)
{
	auto it{ g_visualMap.find(window) };

	if (it != g_visualMap.end())
	{
		g_visualMap.erase(it);
	}
}

void VisualManager::ShutdownLegacyVisualOverrider()
{
	g_visualMap.clear();
}