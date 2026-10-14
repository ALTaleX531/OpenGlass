#include "pch.h"
#include "OpenGlassGUI.hpp"
#include "MainFrame.hpp"
#include "Elevation.hpp"
#include "EditorScope.hpp"
#include <wx/cmdline.h>

// IMPLEMENT_APP must be in global scope
IMPLEMENT_APP(OpenGlass::OpenGlassApp)

namespace OpenGlass
{
	void OpenGlassApp::OnInitCmdLine(wxCmdLineParser& parser)
	{
		wxApp::OnInitCmdLine(parser);
		parser.AddLongOption(L"scope", L"Configuration target: hkcu or hklm", wxCMD_LINE_VAL_STRING);
		parser.AddLongSwitch(L"no-system-transparency", L"Use the editing layer's ColorizationOpaqueBlend instead of Windows transparency");
		parser.AddLongOption(
			L"elevated-pipe",
			L"internal elevation handshake pipe",
			wxCMD_LINE_VAL_STRING,
			wxCMD_LINE_HIDDEN
		);
	}

	bool OpenGlassApp::OnInit()
	{
		MSWEnableDarkMode(wxApp::DarkMode_Auto);
		if (!wxApp::OnInit())
			return false;

		std::vector<std::wstring> argumentStorage;
		for (int index = 1; index < argc; ++index) argumentStorage.emplace_back(argv[index]);
		std::vector<std::wstring_view> arguments(argumentStorage.begin(), argumentStorage.end());
		const auto scope = Settings::ParseEditorScope(arguments);
		if (!scope)
		{
			wxMessageBox(L"Use --scope=hkcu or --scope=hklm. Conflicting scopes are not allowed.", L"Invalid scope", wxOK | wxICON_ERROR);
			return false;
		}
		const bool systemTransparency = std::ranges::find(arguments, L"--no-system-transparency") == arguments.end();
		const auto startup = Elevation::PrepareElevatedStartup(*scope, systemTransparency);
		if (!startup.continueStartup)
		{
			return false;
		}

		// Accent color previews are shared by both editor scopes in this session.
		if (!m_singleInstanceChecker.Create(L"OpenGlassGUI.SingleInstance"))
		{
			wxMessageBox(L"Unable to establish the session editor lock. Close the other editor or retry.", L"OpenGlass", wxOK | wxICON_ERROR);
			return false;
		}
		if (m_singleInstanceChecker.IsAnotherRunning())
			return false;

		MainFrame* frame = new MainFrame(startup.userSid, *scope, systemTransparency);
		frame->Show(true);
		return true;
	}
}
