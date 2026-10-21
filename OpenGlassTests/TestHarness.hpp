#pragma once
#include <source_location>

namespace OpenGlassTests
{
	void Check(bool condition, const std::source_location& location = std::source_location::current());
}
