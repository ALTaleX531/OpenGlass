#include "ThemeAtlasLayout.hpp"

#include <charconv>
#include <new>
#include <string_view>

namespace OpenGlass::ThemeAtlasLayout
{
	namespace
	{
		constexpr HRESULT BadFormat = HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);

		[[nodiscard]] std::string_view Trim(std::string_view value) noexcept
		{
			while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
			while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
			return value;
		}

		[[nodiscard]] bool ParseInteger(std::string_view token, std::int32_t& value) noexcept
		{
			token = Trim(token);
			if (token.empty()) return false;
			if (token.front() == '+')
			{
				token.remove_prefix(1);
				if (token.empty()) return false;
			}
			const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value, 10);
			return error == std::errc{} && end == token.data() + token.size();
		}

		template <std::size_t Count>
		[[nodiscard]] bool ParseIntegerList(
			std::string_view value,
			char delimiter,
			std::array<std::int32_t, Count>& output
		) noexcept
		{
			for (std::size_t index = 0; index < Count; ++index)
			{
				const auto separator = value.find(delimiter);
				if ((index + 1 < Count) != (separator != std::string_view::npos)) return false;
				const auto token = separator == std::string_view::npos ? value : value.substr(0, separator);
				if (!ParseInteger(token, output[index])) return false;
				if (separator == std::string_view::npos) value = {};
				else value.remove_prefix(separator + 1);
			}
			return value.empty();
		}

		[[nodiscard]] bool IsPropertyName(std::string_view value) noexcept
		{
			if (value.empty()) return false;
			const auto isAlpha = [](unsigned char character)
			{
				return (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
			};
			const auto first = static_cast<unsigned char>(value.front());
			if (!isAlpha(first) && first != '_') return false;
			for (const unsigned char character : value.substr(1))
			{
				if (!isAlpha(character) && (character < '0' || character > '9') && character != '_') return false;
			}
			return true;
		}
	}

	HRESULT Parse(std::span<const std::byte> bytes, Document& document, std::size_t* errorLine) noexcept try
	{
		document = {};
		if (errorLine) *errorLine = 0;
		if (bytes.size() > MaximumFileSize) return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);

		Document candidate;
		const std::string_view text
		{
			bytes.empty() ? "" : reinterpret_cast<const char*>(bytes.data()),
			bytes.size()
		};
		std::size_t offset{};
		std::size_t lineNumber{};
		while (offset < text.size())
		{
			if (++lineNumber > MaximumLineCount)
			{
				if (errorLine) *errorLine = lineNumber;
				return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
			}

			const auto newline = text.find('\n', offset);
			const auto end = newline == std::string_view::npos ? text.size() : newline;
			auto line = text.substr(offset, end - offset);
			offset = newline == std::string_view::npos ? text.size() : newline + 1;
			if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
			if (line.size() > MaximumLineLength)
			{
				if (errorLine) *errorLine = lineNumber;
				return HRESULT_FROM_WIN32(ERROR_FILE_TOO_LARGE);
			}

			for (const unsigned char character : line)
			{
				if (character < 0x20 && character != '\t')
				{
					if (errorLine) *errorLine = lineNumber;
					return BadFormat;
				}
			}

			line = Trim(line);
			const auto comment = line.find('#');
			if (comment != std::string_view::npos) line = Trim(line.substr(0, comment));
			if (line.empty()) continue;
			for (const unsigned char character : line)
			{
				if (character >= 0x80)
				{
					if (errorLine) *errorLine = lineNumber;
					return BadFormat;
				}
			}

			const auto equals = line.find('=');
			if (equals == std::string_view::npos || line.find('=', equals + 1) != std::string_view::npos)
			{
				if (errorLine) *errorLine = lineNumber;
				return BadFormat;
			}
			const auto key = Trim(line.substr(0, equals));
			const auto value = Trim(line.substr(equals + 1));
			if (key.find(';') != std::string_view::npos)
			{
				std::array<std::int32_t, 3> keyParts{};
				Mapping mapping{};
				if (!ParseIntegerList(key, ';', keyParts) || !ParseIntegerList(value, ',', mapping.value))
				{
					if (errorLine) *errorLine = lineNumber;
					return BadFormat;
				}
				mapping.part = keyParts[0];
				mapping.state = keyParts[1];
				mapping.property = keyParts[2];
				candidate.records.emplace_back(mapping);
			}
			else
			{
				Property property{ std::string{ key } };
				if (!IsPropertyName(key) || !ParseInteger(value, property.value))
				{
					if (errorLine) *errorLine = lineNumber;
					return BadFormat;
				}
				candidate.records.emplace_back(std::move(property));
			}
		}

		document = std::move(candidate);
		return S_OK;
	}
	catch (const std::bad_alloc&)
	{
		document = {};
		return E_OUTOFMEMORY;
	}
}
