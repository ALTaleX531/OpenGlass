#include "pch.h"
#include "TestHarness.hpp"
#include "ThemeAtlasLayout.hpp"

using namespace OpenGlass;

namespace
{
	std::span<const std::byte> AsBytes(std::string_view value)
	{
		return { reinterpret_cast<const std::byte*>(value.data()), value.size() };
	}
}

namespace OpenGlassTests
{
	void TestThemeAtlasLayoutParser()
	{
		std::string valid = "# legacy comment ";
		valid.push_back(static_cast<char>(0xE9));
		valid += "\r\n1; 2; 3 = 4, 5, 6, 7\t# mapping comment ";
		valid.push_back(static_cast<char>(0xE9));
		valid += "\n1;2;3=8,9,10,11\nUnknownProperty=9 # ignored by the consumer\nRS1Compatibility=1\n12;2;3=-1,+2,3,4\nCaptionHeight=20\nCaptionHeight=21\n";
		ThemeAtlasLayout::Document document;
		Check(SUCCEEDED(ThemeAtlasLayout::Parse(AsBytes(valid), document)));
		Check(document.records.size() == 7);
		if (document.records.size() == 7)
		{
			const auto& first = std::get<ThemeAtlasLayout::Mapping>(document.records[0]);
			Check(first.part == 1 && first.state == 2 && first.property == 3);
			Check(first.value == std::array<std::int32_t, 4>{ 4, 5, 6, 7 });
			Check(std::get<ThemeAtlasLayout::Property>(document.records[2]).name == "UnknownProperty");
			Check(std::get<ThemeAtlasLayout::Property>(document.records[3]).name == "RS1Compatibility");
			Check(std::get<ThemeAtlasLayout::Mapping>(document.records[4]).part == 12);
			Check(std::get<ThemeAtlasLayout::Property>(document.records[6]).value == 21);
		}

		const auto rejected = [&document](std::string_view value)
		{
			document.records.emplace_back(ThemeAtlasLayout::Property{ "stale", 1 });
			const auto result = ThemeAtlasLayout::Parse(AsBytes(value), document);
			return FAILED(result) && document.records.empty();
		};
		Check(rejected("1;2;3=1,2,3"));
		Check(rejected("1;2;3=1,2,3,4,5"));
		Check(rejected("1;2;3=2147483648,2,3,4"));
		Check(rejected("Property=1=2"));
		Check(rejected(std::string("Property=1\0ignored", 18)));
		Check(rejected("Property=1\x01"));
		Check(rejected(std::string(ThemeAtlasLayout::MaximumLineLength + 1, 'a')));
		std::string tooManyLines;
		for (std::size_t line = 0; line <= ThemeAtlasLayout::MaximumLineCount; ++line) tooManyLines += "#\n";
		Check(rejected(tooManyLines));
	}
}
