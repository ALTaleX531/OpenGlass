#pragma once
#include "framework.hpp"
#pragma warning(push)
#pragma warning(disable : 4201)

namespace OpenGlass::os
{
	FORCEINLINE PKUSER_SHARED_DATA GetKernelSharedInfo()
	{
		return reinterpret_cast<PKUSER_SHARED_DATA>(0x7FFE0000);
	}

	inline const ULONG buildNumber{ GetKernelSharedInfo()->NtBuildNumber };
	inline const ULONG minorVersion{ GetKernelSharedInfo()->NtMinorVersion };
	inline const ULONG majorVersion{ GetKernelSharedInfo()->NtMajorVersion };
	inline const NT_PRODUCT_TYPE productType{ GetKernelSharedInfo()->NtProductType };

	enum os_build : ULONG
	{
		build_w10_1507 = 10240,
		build_w10_1511 = 10586,
		build_w10_1607 = 14393,
		build_w10_1703 = 15063,
		build_w10_1709 = 16299,
		build_w10_1803 = 17134,
		build_w10_1809 = 17763,
		build_w10_1903 = 18362,
		build_w10_19h1 = 18362,
		build_w10_1909 = 18363,
		build_w10_19h2 = 18363,
		build_w10_2004 = 19041,
		build_w10_20h1 = 19041,
		build_w10_20h2 = 19042,
		build_w10_21h1 = 19043,
		build_w10_21h2 = 19044,
		build_w10_22h2 = 19045,
		build_server_2022 = 20348,
		build_w11_21h2 = 22000,
		build_w11_22h2 = 22621,
		build_w11_23h2 = 22631,
		build_w11_24h2 = 26100,
		build_w11_25h2 = 26200,
		build_w11_26h1 = 28000,
	};
	enum os_revision : ULONG
	{
		revision_21h2_rtm_0 = 194,
		revision_21h2_post_rtm_0 = 282,
		revision_24h2_rtm_1 = 2454,
		revision_24h2_with_25h2_code_staged = 4484,
		revision_24h2_kb5067036 = 7019,
		revision_25h2_kb5068861 = 7019,
		revision_kb5083631 = 8328,
		revision_kb5101684 = 8972,
	};
}

#pragma warning(pop)
