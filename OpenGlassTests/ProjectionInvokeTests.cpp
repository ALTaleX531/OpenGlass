#include "pch.h"
#include "../Common/ProjectionInvoke.hpp"

using namespace OpenGlass;

namespace OpenGlassTests
{
	struct AbiSample
	{
		int Member(double) const noexcept;
	};

	struct Aggregate
	{
		ULONG_PTR first;
		ULONG_PTR second;
	};

	using AggregateFunction = Aggregate (*)(Aggregate, const int&);
	static_assert(std::is_same_v<Projection::projected_abi_t<&AbiSample::Member>, int (*)(const AbiSample*, double)>);
	static_assert(std::is_same_v<Projection::projected_abi_t<static_cast<AggregateFunction>(nullptr)>, AggregateFunction>);
	static_assert(Projection::is_discard_return_compatible_v<int (*)(AbiSample*, double), void (*)(AbiSample*, double)>);
	static_assert(!Projection::is_discard_return_compatible_v<void (*)(AbiSample*, double), void (*)(AbiSample*, double)>);
	static_assert(Projection::is_extra_trailing_argument_compatible_v<
		void (*)(const AbiSample*, double), void (*)(const AbiSample*, double, bool)>);
	static_assert(!Projection::is_extra_trailing_argument_compatible_v<
		void (*)(const AbiSample*, double), void (*)(const AbiSample*, int, bool)>);
}
