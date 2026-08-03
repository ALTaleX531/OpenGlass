#pragma once
#include "pch.h"

namespace OpenGlass::ConfigurationMigration
{
	[[nodiscard]] bool EnsureCanonicalConfiguration(const std::wstring& userSid);
}
