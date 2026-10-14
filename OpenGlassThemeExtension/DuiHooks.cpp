#include "HookHelper.hpp"
#include "SystemSettings.hpp"
#include "ColorHooks.hpp"
#include "Colorization.hpp"
#include "DuiHooks.hpp"
#include "DuiProjection.hpp"
#include "resource.h"
#include <wil/registry.h>
#include <wil/win32_helpers.h>

using namespace OpenGlass;

namespace OpenGlass::DuiHooks
{
	void MyCGlassColorCplPage_HandleOK(PVOID This);
	HRESULT MyCGlassColorCplPage_HandleNavigateAway(PVOID This);
	HRESULT MyDirectUI_XResourceProvider_Create(HINSTANCE resources, PCWSTR resource, PCWSTR element, PCWSTR theme, DirectUI::IXProviderCP** output);

	struct CColorCplPageHandlerVtable
	{
		void (*HandleOK)(PVOID);
		void (*HandleCancel)(PVOID);
		void (*HandleApply)(PVOID);
		void (*HandleInput)(PVOID, DirectUI::InputEvent*);
		HRESULT (*HandleLayoutInit)(PVOID, IUnknown*, DirectUI::CCPushButton*, DirectUI::CCPushButton*, DirectUI::CCPushButton*, bool, bool*);
		HRESULT (*HandleNavigateAway)(PVOID);
		void (*PerformInitTasks)(PVOID);
	};

	class CColorPageProvider final : public DirectUI::IXProviderCP, public DirectUI::IElementListener
	{
		friend void MyCGlassColorCplPage_HandleOK(PVOID This);
		friend HRESULT MyCGlassColorCplPage_HandleNavigateAway(PVOID This);

		HRESULT _SetTransparency(bool enabled);
		void _UpdateSwatchName();

		// verified in x64 themecpl 10.0.22621.4745 and 10.0.26100.9549. See Design.md.
		static constexpr size_t CGlassColorCplPage_CColorCplPageHandler_Offset = 208;

		struct HandlerVtable
		{
			CColorCplPageHandlerVtable functions;
			CColorPageProvider* provider;
		} m_handlerVtable{ {}, this };
		CColorCplPageHandlerVtable* m_handlerVtable_Org{};
		HookHelper::PointerHookBase m_handlerHook;

		HINSTANCE m_resources;
		DirectUI::Element* m_transparencyCheckBox{};
		DirectUI::Selector* m_swatchSelector{};
		DirectUI::Element* m_swatchName{};
		DirectUI::Element* m_colorPage{};
		bool m_initialTransparency{};
	public:
		explicit CColorPageProvider(HINSTANCE resources) : m_resources(resources) {}
		void* operator new(std::size_t size);
		void operator delete(void* memory) noexcept;

		HRESULT CreateDUICP(DirectUI::Element* host, HWND, HWND, DirectUI::Element** output, DirectUI::DUIXmlParser** parser) override;
		HRESULT CreateParserCP(DirectUI::DUIXmlParser** output) override;
		void DestroyCP() override;

		void OnListenerAttach(DirectUI::Element*) override {}
		void OnListenerDetach(DirectUI::Element* element) override;
		bool OnListenedPropertyChanging(DirectUI::Element*, const DirectUI::PropertyInfo*, int, DirectUI::Value*, DirectUI::Value*) override { return true; }
		void OnListenedPropertyChanged(DirectUI::Element* element, const DirectUI::PropertyInfo* property, int, DirectUI::Value*, DirectUI::Value*) override;
		void OnListenedInput(DirectUI::Element*, DirectUI::InputEvent*) override {}
		void OnListenedEvent(DirectUI::Element*, DirectUI::Event*) override {}
	};

	PVOID g_DirectUI_XResourceProvider_Create_Org{};
	HookHelper::ImportHook g_importHook;
}

void* DuiHooks::CColorPageProvider::operator new(std::size_t size)
{
	const auto memory = HeapAlloc(GetProcessHeap(), 0, size);
	THROW_IF_NULL_ALLOC(memory);
	return memory;
}

void DuiHooks::CColorPageProvider::operator delete(void* memory) noexcept
{
	wil::process_heap_deleter{}(memory);
}

HRESULT DuiHooks::CColorPageProvider::CreateParserCP(DirectUI::DUIXmlParser** output)
{
	*output = nullptr;

	DirectUI::DUIXmlParser* parser{};
	RETURN_IF_FAILED(DirectUI::DUIXmlParser::Create(&parser, nullptr, nullptr, nullptr, nullptr));
	const auto destroy = wil::scope_exit([&] { if (parser) parser->Destroy(); });
	// XML and default resources are ours; registered page classes remain native.
	RETURN_IF_FAILED(parser->SetXMLFromResourceWithTheme(IDR_COLOR_PAGE, wil::GetModuleInstanceHandle(), wil::GetModuleInstanceHandle(), m_resources));

	*output = std::exchange(parser, nullptr);
	return S_OK;
}

HRESULT DuiHooks::CColorPageProvider::CreateDUICP(DirectUI::Element* host, HWND, HWND, DirectUI::Element** output, DirectUI::DUIXmlParser** parser) try
{
	*output = nullptr;
	*parser = nullptr;

	DirectUI::DUIXmlParser* xml{};
	RETURN_IF_FAILED(CreateParserCP(&xml));
	const auto destroy = wil::scope_exit([&] { if (xml) xml->Destroy(); });

	DirectUI::Element* root{};
	RETURN_IF_FAILED(xml->CreateElement(L"main", nullptr, host, nullptr, &root));
	const auto destroyRoot = wil::scope_exit([&]
	{
		if (!root) return;
		m_transparencyCheckBox = m_swatchName = nullptr;
		m_swatchSelector = nullptr;
		root->Destroy(false);
	});

	m_transparencyCheckBox = root->FindDescendent(DirectUI::StrToID(L"EnableGlassCheckBox"));
	m_swatchSelector = static_cast<DirectUI::Selector*>(root->FindDescendent(DirectUI::StrToID(L"SwatchSelector")));
	m_swatchName = root->FindDescendent(DirectUI::StrToID(L"ColorSwatchLabelName"));

	const auto page = root->FindDescendent(DirectUI::StrToID(L"WindowColorCPL"));
	RETURN_HR_IF(E_UNEXPECTED, !m_transparencyCheckBox || !m_swatchSelector || !m_swatchName || !page);
	m_initialTransparency = Util::IsTransparencyEnabled();
	RETURN_IF_FAILED(m_transparencyCheckBox->SetSelected(m_initialTransparency));
	RETURN_IF_FAILED(m_transparencyCheckBox->AddListener(this));
	RETURN_IF_FAILED(m_swatchSelector->AddListener(this));
	RETURN_IF_FAILED(m_swatchName->AddListener(this));
	RETURN_IF_FAILED(page->AddListener(this));
	_UpdateSwatchName();

	// Each page retains its own native methods and provider binding.
	const auto handler = reinterpret_cast<BYTE*>(page) + CGlassColorCplPage_CColorCplPageHandler_Offset;
	m_handlerVtable.functions = *HookHelper::get_vftable_from<CColorCplPageHandlerVtable>(handler);
	m_handlerVtable.functions.HandleOK = &MyCGlassColorCplPage_HandleOK;
	m_handlerVtable.functions.HandleNavigateAway = &MyCGlassColorCplPage_HandleNavigateAway;
	m_handlerHook.Prepare(reinterpret_cast<PVOID*>(handler), &m_handlerVtable.functions, reinterpret_cast<PVOID*>(&m_handlerVtable_Org));

	HookHelper::HookTransaction transaction{ HookHelper::HookMode::Install };
	transaction.Apply(m_handlerHook);
	transaction.Commit();

	m_colorPage = page;
	*output = std::exchange(root, nullptr);
	*parser = std::exchange(xml, nullptr);
	return S_OK;
}
CATCH_RETURN();

void DuiHooks::CColorPageProvider::DestroyCP()
{
	// FrameProvider can release this provider before XProvider destroys the element tree.
	if (m_colorPage) m_colorPage->RemoveListener(this);
	if (m_transparencyCheckBox) m_transparencyCheckBox->RemoveListener(this);
	if (m_swatchSelector) m_swatchSelector->RemoveListener(this);
	if (m_swatchName) m_swatchName->RemoveListener(this);
	delete this;
}

void DuiHooks::CColorPageProvider::OnListenerDetach(DirectUI::Element* element)
{
	if (element == m_transparencyCheckBox) m_transparencyCheckBox = nullptr;
	if (element == m_swatchSelector) m_swatchSelector = nullptr;
	if (element == m_swatchName) m_swatchName = nullptr;
	if (element == m_colorPage)
	{
		// Both RemoveListener and Element::OnDestroy detach before either owner is freed.
		HookHelper::HookTransaction transaction{ HookHelper::HookMode::Remove };
		transaction.Apply(m_handlerHook);
		transaction.Commit();
		m_colorPage = nullptr;
	}
}

void DuiHooks::CColorPageProvider::OnListenedPropertyChanged(DirectUI::Element* element, const DirectUI::PropertyInfo* property, int, DirectUI::Value*, DirectUI::Value*)
{
	if (element == m_swatchSelector && property == DirectUI::Selector::SelectionProp())
	{
		_UpdateSwatchName();
	}
	else if (element == m_transparencyCheckBox && property == DirectUI::Element::SelectedProp())
	{
		const auto hr = _SetTransparency(element->GetSelected());
		if (FAILED_LOG(hr))
		{
			LOG_IF_FAILED(element->SetSelected(Util::IsTransparencyEnabled()));
		}
		else if (hr == S_OK)
		{
			Colorization::ColorizationParameters parameters{};
			if (SUCCEEDED_LOG(ColorHooks::MyDwmpGetColorizationParameters(&parameters)))
			{
				LOG_IF_FAILED(ColorHooks::MyDwmpSetColorizationParameters(&parameters, TRUE));
			}
		}
	}
}

HRESULT DuiHooks::CColorPageProvider::_SetTransparency(bool enabled)
{
	if (enabled == Util::IsTransparencyEnabled()) return S_FALSE;
	return wil::reg::set_value_dword_nothrow(HKEY_CURRENT_USER, Util::g_personalizeKeyPath, L"EnableTransparency", enabled);
}

void DuiHooks::CColorPageProvider::_UpdateSwatchName()
{
	if (!m_swatchSelector || !m_swatchName) return;

	const auto selected = m_swatchSelector->GetSelection();
	DirectUI::Value* value{};
	const auto release = wil::scope_exit([&] { if (value) value->Release(); });

	PCWSTR name{};
	if (selected)
	{
		name = selected->GetAccName(&value);
	}
	else
	{
		// Common/CppProject.props enables null-terminated STRINGTABLE entries.
		LoadStringW(wil::GetModuleInstanceHandle(), IDS_CUSTOM_COLOR, reinterpret_cast<PWSTR>(&name), 0);
	}

	LOG_IF_FAILED(m_swatchName->SetContentString(name));
}

void DuiHooks::MyCGlassColorCplPage_HandleOK(PVOID This)
{
	const auto provider = HookHelper::get_vftable_from<CColorPageProvider::HandlerVtable>(This)->provider;
	provider->m_initialTransparency = provider->m_transparencyCheckBox->GetSelected();
	provider->m_handlerVtable_Org->HandleOK(This);
}

HRESULT DuiHooks::MyCGlassColorCplPage_HandleNavigateAway(PVOID This)
{
	const auto provider = HookHelper::get_vftable_from<CColorPageProvider::HandlerVtable>(This)->provider;
	HRESULT hr{ S_OK };
	if (provider->m_transparencyCheckBox->GetSelected() != provider->m_initialTransparency)
	{
		hr = provider->_SetTransparency(provider->m_initialTransparency);
		if (SUCCEEDED(hr))
		{
			LOG_IF_FAILED(provider->m_transparencyCheckBox->SetSelected(provider->m_initialTransparency));
		}
	}

	// A transparency failure must not suppress native color rollback.
	RETURN_IF_FAILED(provider->m_handlerVtable_Org->HandleNavigateAway(This));
	RETURN_IF_FAILED(hr);
	return S_OK;
}

HRESULT DuiHooks::MyDirectUI_XResourceProvider_Create(HINSTANCE resources, PCWSTR resource, PCWSTR element, PCWSTR theme, DirectUI::IXProviderCP** output) try
{
	if (resource != MAKEINTRESOURCEW(IDR_COLOR_PAGE))
	{
		// Never forward through a saved unresolved delay thunk.
		return DirectUI::XResourceProvider::Create(resources, resource, element, theme, output);
	}

	*output = new CColorPageProvider(resources);
	return S_OK;
}
CATCH_RETURN();

HookHelper::ImportHook* DuiHooks::Prepare(HookType type)
{
	if (type != HookType::CPL || g_importHook.IsInstalled()) return nullptr;

	DirectUI::InitializeProjection();
	THROW_IF_FAILED(g_importHook.Prepare(GetModuleHandleW(L"themecpl.dll"), std::initializer_list<HookHelper::ImportDllDetourInfo>
	{
		{
			"dui70.dll",
			std::initializer_list<HookHelper::ImportFunctionDetourInfo>
			{
				{ DirectUI::XResourceProvider_Create_Name, &g_DirectUI_XResourceProvider_Create_Org, &MyDirectUI_XResourceProvider_Create }
			}
		}
	}, true));
	return &g_importHook;
}
