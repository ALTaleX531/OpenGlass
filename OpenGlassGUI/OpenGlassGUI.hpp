#pragma once
#include "pch.h"

namespace OpenGlass
{
	class OpenGlassApp : public wxApp
	{
	public:
		bool OnInit() override;
		void OnInitCmdLine(wxCmdLineParser& parser) override;
	private:
		wxSingleInstanceChecker m_singleInstanceChecker;
	};
}

DECLARE_APP(OpenGlass::OpenGlassApp)
