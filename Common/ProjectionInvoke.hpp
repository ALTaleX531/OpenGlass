#pragma once
#include <windows.h>
#include <wil/result.h>
#include <bit>
#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

namespace OpenGlass::Projection
{
	template <typename T> struct ProjectedAbi;
	[[noreturn]] inline void ProjectedFailFastCommon() noexcept
	{
		FAIL_FAST_HR(E_UNEXPECTED);
		__assume(0);
	}

#define OPENGLASS_DEFINE_MEMBER_ABI(QUALIFIERS, THIS_TYPE)                                                           \
	template <typename R, typename C, typename... Args> struct ProjectedAbi<R (C::*)(Args...) QUALIFIERS>            \
	{                                                                                                                \
		using type = R (*)(THIS_TYPE C*, Args...);                                                                     \
		[[noreturn]] static R FailFast(THIS_TYPE C*, Args...)                                                          \
		{                                                                                                              \
			ProjectedFailFastCommon();                                                                                   \
		}                                                                                                              \
	};

	OPENGLASS_DEFINE_MEMBER_ABI(, )
	OPENGLASS_DEFINE_MEMBER_ABI(const, const)
	OPENGLASS_DEFINE_MEMBER_ABI(noexcept, )
	OPENGLASS_DEFINE_MEMBER_ABI(const noexcept, const)
#undef OPENGLASS_DEFINE_MEMBER_ABI

	template <typename R, typename... Args> struct ProjectedAbi<R (*)(Args...)>
	{
		using type = R (*)(Args...);
		[[noreturn]] static R FailFast(Args...)
		{
			ProjectedFailFastCommon();
		}
	};

	template <typename R, typename... Args> struct ProjectedAbi<R (*)(Args...) noexcept>
	{
		using type = R (*)(Args...) noexcept;
		[[noreturn]] static R FailFast(Args...) noexcept
		{
			ProjectedFailFastCommon();
		}
	};

	template <auto Target> using projected_abi_t = typename ProjectedAbi<decltype(Target)>::type;

	template <typename T> struct FunctionPointerTraits;
	template <typename R, typename... Args> struct FunctionPointerTraits<R (*)(Args...)>
	{
		using result_type = R;
		using arguments = std::tuple<Args...>;
	};
	template <typename R, typename... Args> struct FunctionPointerTraits<R (*)(Args...) noexcept>
	{
		using result_type = R;
		using arguments = std::tuple<Args...>;
	};

	template <typename Source, typename Target, typename = void> struct IsDiscardReturnCompatible : std::false_type
	{
	};
	template <typename Source, typename Target>
	struct IsDiscardReturnCompatible<
		Source,
		Target,
		std::void_t<
			typename FunctionPointerTraits<Source>::result_type,
			typename FunctionPointerTraits<Source>::arguments,
			typename FunctionPointerTraits<Target>::result_type,
			typename FunctionPointerTraits<Target>::arguments
		>
	> : std::bool_constant<
		!std::is_void_v<typename FunctionPointerTraits<Source>::result_type> &&
		std::is_void_v<typename FunctionPointerTraits<Target>::result_type> &&
		std::same_as<
			typename FunctionPointerTraits<Source>::arguments,
			typename FunctionPointerTraits<Target>::arguments
		>
	>
	{
	};

	template <typename SourceTuple, typename TargetTuple, size_t... Indices>
	consteval bool IsTuplePrefix(std::index_sequence<Indices...>)
	{
		return (std::same_as<
			std::tuple_element_t<Indices, SourceTuple>,
			std::tuple_element_t<Indices, TargetTuple>
		> && ...);
	}

	template <typename Source, typename Target, typename = void> struct IsExtraTrailingArgumentCompatible : std::false_type
	{
	};
	template <typename Source, typename Target>
	struct IsExtraTrailingArgumentCompatible<
		Source,
		Target,
		std::void_t<
			typename FunctionPointerTraits<Source>::result_type,
			typename FunctionPointerTraits<Source>::arguments,
			typename FunctionPointerTraits<Target>::result_type,
			typename FunctionPointerTraits<Target>::arguments
		>
	> : std::bool_constant<
		std::same_as<
			typename FunctionPointerTraits<Source>::result_type,
			typename FunctionPointerTraits<Target>::result_type
		> &&
		(std::tuple_size_v<typename FunctionPointerTraits<Target>::arguments> ==
		 std::tuple_size_v<typename FunctionPointerTraits<Source>::arguments> + 1) &&
		IsTuplePrefix<
			typename FunctionPointerTraits<Source>::arguments,
			typename FunctionPointerTraits<Target>::arguments
		>(std::make_index_sequence<std::tuple_size_v<typename FunctionPointerTraits<Source>::arguments>>{})
	>
	{
	};

	template <typename Source, typename Target>
	inline constexpr bool is_discard_return_compatible_v = IsDiscardReturnCompatible<Source, Target>::value;
	template <typename Source, typename Target>
	inline constexpr bool is_extra_trailing_argument_compatible_v =
		IsExtraTrailingArgumentCompatible<Source, Target>::value;

	template <auto Target> inline projected_abi_t<Target> g_projectedSlot{};

	template <auto Target> inline constexpr projected_abi_t<Target> ProjectedFailFast =
		&ProjectedAbi<decltype(Target)>::FailFast;

	template <auto Target> PVOID* ProjectedSlotStorage() noexcept
	{
		static_assert(sizeof(g_projectedSlot<Target>) == sizeof(PVOID));
		return reinterpret_cast<PVOID*>(&g_projectedSlot<Target>);
	}

	template <auto Target> PVOID* ProjectedVariableStorage() noexcept
	{
		static_assert(std::is_pointer_v<decltype(Target)>);
		static_assert(sizeof(*Target) == sizeof(PVOID));
		return reinterpret_cast<PVOID*>(reinterpret_cast<ULONG_PTR>(Target));
	}

	template <typename T> PVOID ProjectedAddress(T pointer) noexcept
	{
		static_assert(sizeof(pointer) == sizeof(PVOID));
		return std::bit_cast<PVOID>(pointer);
	}

	template <auto Target> inline projected_abi_t<Target>& Invoke = g_projectedSlot<Target>;

#ifdef _DEBUG
#define OPENGLASS_MUSTTAIL
#else
#define OPENGLASS_MUSTTAIL [[msvc::musttail]]
#endif
}
