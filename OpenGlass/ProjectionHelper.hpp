#pragma once
#include "HookHelper.hpp"
#include "OSHelper.hpp"
#include "Util.hpp"

namespace OpenGlass
{
	enum class ProjectionType : UCHAR
	{
		Variable = 0,
		Function = 1 << 0,
		Optional = 1 << 1
	};
	using ProjectionEntry = std::tuple<ProjectionType, LPCSTR, PVOID, PVOID, ULONG, ULONG>;

	template <size_t N>
	struct ProjectionArray : std::array<ProjectionEntry, N>
	{
		ULONG _build{ 0 };
		size_t _lastEmptyEntryIndex{ 0ull };

		static bool IsProjectionEntryValid(ULONG build, const ProjectionEntry& entry)
		{
			if (build == 0)
			{
				return true;
			}
			const auto& min_build = std::get<4>(entry);
			const auto& max_build = std::get<5>(entry);
			if (min_build == 0 && max_build == 0)
			{
				return true;
			}
			if (min_build == 0)
			{
				return build < max_build;
			}
			if (max_build == 0)
			{
				return build >= min_build;
			}
			return build >= min_build && build < max_build;
		}
		void _Apply(ProjectionEntry& entry, PVOID symbolAddress)
		{
			[[maybe_unused]] auto& [type, name, from, to, min_build, max_build] = entry;
			to = symbolAddress;

			if (type == ProjectionType::Function)
			{
				UCHAR jmpInstructionBytes[]
				{
					// jmp qword ptr [rip+0]
					0xFF, 0x25,
					0x00, 0x00, 0x00, 0x00,

					0x00, 0x00, 0x00, 0x00,
					0x00, 0x00, 0x00, 0x00
				};
				auto UnprotectedScope = HookHelper::Unprotect({ reinterpret_cast<uint8_t*>(from), sizeof(jmpInstructionBytes)});
				*reinterpret_cast<PVOID*>(&jmpInstructionBytes[6]) = to;

				HookHelper::PatchInstructions(
					reinterpret_cast<uint8_t*>(from),
					jmpInstructionBytes
				);
			}
			if (type == ProjectionType::Variable && from)
			{
				*reinterpret_cast<PVOID*>(from) = to;
			}
		}
		void ApplyByIndex(size_t index, PVOID symbolAddress)
		{
			auto& entry = this->operator[](index);
			if (!IsProjectionEntryValid(_build, entry))
			{
				return;
			}

			_Apply(entry, symbolAddress);

			if (_lastEmptyEntryIndex == index && std::get<3>(entry))
			{
				_lastEmptyEntryIndex += 1ull;
			}
		}
		void Apply(LPCSTR symbolName, PVOID symbolAddress)
		{
			bool updated{ false };
			for (auto i = _lastEmptyEntryIndex; i < N; i++)
			{
				auto& entry = this->operator[](i);
				if (!IsProjectionEntryValid(_build, entry))
				{
					continue;
				}

				if (!strcmp(std::get<1>(entry), symbolName))
				{
					_Apply(entry, symbolAddress);

					if (_lastEmptyEntryIndex == i && std::get<3>(entry))
					{
						_lastEmptyEntryIndex += 1ull;
					}
					break;
				}
				else
				{
					if (!updated && !std::get<3>(entry))
					{
						_lastEmptyEntryIndex = i;
						updated = true;
					}
				}
			}
		}
		void ApplyToVariable(LPCSTR symbolName, PVOID* variableAddress)
		{
			for (auto i = 0ull; i < N; i++)
			{
				const auto& entry = this->operator[](i);
				if (!IsProjectionEntryValid(_build, entry))
				{
					continue;
				}
				if (!strcmp(std::get<1>(entry), symbolName))
				{
					*variableAddress = std::get<3>(entry);
					break;
				}
			}
		}
		template <typename T>
		FORCEINLINE void ApplyToVariable(LPCSTR symbolName, T& variableAddress)
		{
			return ApplyToVariable(symbolName, reinterpret_cast<PVOID*>(&variableAddress));
		}

		bool IsAllReady() const
		{
			if (_lastEmptyEntryIndex == 0ull)
			{
				return false;
			}
			if (_lastEmptyEntryIndex == N)
			{
				return true;
			}
			for (auto i = _lastEmptyEntryIndex; i < N; i++)
			{
				const auto& entry = this->operator[](i);
				if (!IsProjectionEntryValid(_build, entry))
				{
					continue;
				}

				if (!std::get<3>(entry))
				{
					return false;
				}
			}

			return true;
		}
		void ReportMissing(std::string& missingFunctionsOrVariables, std::string_view prefix) const
		{
			if (_lastEmptyEntryIndex == N)
			{
				return;
			}
			for (auto i = _lastEmptyEntryIndex; i < N; i++)
			{
				const auto& entry = this->operator[](i);
				if (!IsProjectionEntryValid(_build, entry))
				{
					continue;
				}

				if (!std::get<3>(entry) && !(static_cast<UCHAR>(std::get<0>(entry)) & static_cast<UCHAR>(ProjectionType::Optional)))
				{
					missingFunctionsOrVariables.append(prefix);
					missingFunctionsOrVariables.append(std::get<1>(entry));
					missingFunctionsOrVariables.append("\n");
				}
			}
		}
	};
	template <typename First, typename... Rest>
	ProjectionArray(First, Rest...) -> ProjectionArray<1 + sizeof...(Rest)>;
	template <typename... Args>
	FORCEINLINE auto make_projection_array(ULONG build, Args&&... args)
	{
		auto result = ProjectionArray{ std::forward<Args>(args)... };
		result._build = build;

		return result;
	}
}

// use direct projection for small, hot functions to allow inlining the stub into the caller for better performance
#define DECLSPEC_DIRECT_PROJECTION inline
// use indirect projection for symbol-dependent functions to avoid inlining the stub
#define DECLSPEC_INDIRECT_PROJECTION DECLSPEC_NOINLINE inline
#define HANDLE_PROJECTION_FUNCTION(function, ...) \
std::invoke(Util::force_cast_to<decltype(&function)>(reinterpret_cast<PVOID>(Util::compile_time_hash(#function, 0ull))), ##__VA_ARGS__)

#define MAKE_FUNCTION_PROJECTION_TUPLE(function, min_build, max_build) \
OpenGlass::ProjectionEntry \
{ \
	OpenGlass::ProjectionType::Function, \
	LPCSTR{ #function }, \
	Util::force_cast_from(&function), \
	PVOID{ nullptr }, \
	ULONG{ min_build }, \
	ULONG{ max_build }, \
}
#define MAKE_FUNCTION_PROJECTION_TUPLE_BY_ALIAS(function, name, min_build, max_build) \
OpenGlass::ProjectionEntry \
{ \
	OpenGlass::ProjectionType::Function, \
	LPCSTR{ name }, \
	Util::force_cast_from(&function), \
	PVOID{ nullptr }, \
	ULONG{ min_build }, \
	ULONG{ max_build }, \
}

#define MAKE_VARIABLE_PROJECTION_TUPLE(variable, min_build, max_build) \
OpenGlass::ProjectionEntry \
{ \
	OpenGlass::ProjectionType::Variable, \
	LPCSTR{ #variable }, \
	Util::force_cast_from(&variable), \
	PVOID{ nullptr }, \
	ULONG{ min_build }, \
	ULONG{ max_build }, \
}
#define MAKE_VARIABLE_PROJECTION_TUPLE_BY_ALIAS(variable, name, min_build, max_build) \
OpenGlass::ProjectionEntry \
{ \
	OpenGlass::ProjectionType::Variable, \
	LPCSTR{ name }, \
	Util::force_cast_from(&variable), \
	PVOID{ nullptr }, \
	ULONG{ min_build }, \
	ULONG{ max_build }, \
}

#define MAKE_EMPTY_PROJECTION_TUPLE(name, min_build, max_build) \
OpenGlass::ProjectionEntry \
{ \
	OpenGlass::ProjectionType::Variable, \
	LPCSTR{ name }, \
	PVOID{ nullptr }, \
	PVOID{ nullptr }, \
	ULONG{ min_build }, \
	ULONG{ max_build }, \
}

#define MAKE_OPTIONAL_FUNCTION_PROJECTION_TUPLE(function, min_build, max_build) \
OpenGlass::ProjectionEntry \
{ \
	static_cast<OpenGlass::ProjectionType>(static_cast<UCHAR>(OpenGlass::ProjectionType::Function) | static_cast<UCHAR>(OpenGlass::ProjectionType::Optional)), \
	LPCSTR{ #function }, \
	Util::force_cast_from(&function), \
	PVOID{ nullptr }, \
	ULONG{ min_build }, \
	ULONG{ max_build }, \
}

#define MAKE_OPTIONAL_EMPTY_PROJECTION_TUPLE_BY_ALIAS(name, min_build, max_build) \
OpenGlass::ProjectionEntry \
{ \
	static_cast<OpenGlass::ProjectionType>(static_cast<UCHAR>(OpenGlass::ProjectionType::Variable) | static_cast<UCHAR>(OpenGlass::ProjectionType::Optional)), \
	LPCSTR{ name }, \
	PVOID{ nullptr }, \
	PVOID{ nullptr }, \
	ULONG{ min_build }, \
	ULONG{ max_build }, \
}
