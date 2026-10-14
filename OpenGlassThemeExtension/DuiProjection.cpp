#include "DuiProjection.hpp"
#include <wil/resource.h>

using namespace OpenGlass;

void DirectUI::InitializeProjection()
{
	const wil::unique_hmodule dui{ LoadLibraryExW(L"dui70.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32) };
	THROW_LAST_ERROR_IF(!dui);

	const struct
	{
		PVOID* m_slot;
		PCSTR m_name;
	} exports[]
	{
		{ Projection::ProjectedSlotStorage<&XResourceProvider::Create>(), XResourceProvider_Create_Name },
		{ Projection::ProjectedSlotStorage<&DUIXmlParser::Create>(), "?Create@DUIXmlParser@DirectUI@@SAJPEAPEAV12@P6APEAVValue@2@PEBGPEAX@Z2P6AX11H2@Z2@Z" },
		{ Projection::ProjectedSlotStorage<&DUIXmlParser::Destroy>(), "?Destroy@DUIXmlParser@DirectUI@@QEAAXXZ" },
		{ Projection::ProjectedSlotStorage<&DUIXmlParser::SetXMLFromResourceWithTheme>(), "?SetXMLFromResourceWithTheme@DUIXmlParser@DirectUI@@QEAAJIPEAUHINSTANCE__@@00@Z" },
		{ Projection::ProjectedSlotStorage<&DUIXmlParser::CreateElement>(), "?CreateElement@DUIXmlParser@DirectUI@@QEAAJPEBGPEAVElement@2@1PEAKPEAPEAV32@@Z" },
		{ Projection::ProjectedSlotStorage<&Element::Destroy>(), "?Destroy@Element@DirectUI@@QEAAJ_N@Z" },
		{ Projection::ProjectedSlotStorage<&StrToID>(), "StrToID" },
		{ Projection::ProjectedSlotStorage<&Element::FindDescendent>(), "?FindDescendent@Element@DirectUI@@QEAAPEAV12@G@Z" },
		{ Projection::ProjectedSlotStorage<&Element::AddListener>(), "?AddListener@Element@DirectUI@@QEAAJPEAUIElementListener@2@@Z" },
		{ Projection::ProjectedSlotStorage<&Element::RemoveListener>(), "?RemoveListener@Element@DirectUI@@QEAAXPEAUIElementListener@2@@Z" },
		{ Projection::ProjectedSlotStorage<&Element::GetSelected>(), "?GetSelected@Element@DirectUI@@QEAA_NXZ" },
		{ Projection::ProjectedSlotStorage<&Element::SetSelected>(), "?SetSelected@Element@DirectUI@@QEAAJ_N@Z" },
		{ Projection::ProjectedSlotStorage<&Element::SelectedProp>(), "?SelectedProp@Element@DirectUI@@SAPEBUPropertyInfo@2@XZ" },
		{ Projection::ProjectedSlotStorage<&Selector::GetSelection>(), "?GetSelection@Selector@DirectUI@@QEAAPEAVElement@2@XZ" },
		{ Projection::ProjectedSlotStorage<&Selector::SelectionProp>(), "?SelectionProp@Selector@DirectUI@@SAPEBUPropertyInfo@2@XZ" },
		{ Projection::ProjectedSlotStorage<&Element::GetAccName>(), "?GetAccName@Element@DirectUI@@QEAAPEBGPEAPEAVValue@2@@Z" },
		{ Projection::ProjectedSlotStorage<&Element::SetContentString>(), "?SetContentString@Element@DirectUI@@QEAAJPEBG@Z" },
		{ Projection::ProjectedSlotStorage<&Value::Release>(), "?Release@Value@DirectUI@@QEAAXXZ" }
	};
	for (const auto& entry : exports)
	{
		*entry.m_slot = reinterpret_cast<PVOID>(GetProcAddress(dui.get(), entry.m_name));
		THROW_LAST_ERROR_IF_MSG(!*entry.m_slot, "Missing DirectUI export %hs", entry.m_name);
	}

	// Function pointers and callbacks live as long as the already pinned extension.
	HMODULE pinned{};
	THROW_IF_WIN32_BOOL_FALSE(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<PCWSTR>(dui.get()), &pinned));
}
