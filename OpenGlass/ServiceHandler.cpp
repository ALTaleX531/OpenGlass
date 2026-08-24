#include "pch.h"
#include "ServiceHandler.h"
#include "GlassService.hpp"
#include "Util.hpp"

struct ServiceHandlerContext
{
	SERVICE_STATUS_HANDLE statusHandle{ nullptr };
	wil::unique_handle serverThreadHandle{ nullptr };
	wil::unique_handle injectionThreadHandle{ nullptr };
	wil::unique_handle serverReadyEvent{ nullptr };
	wil::unique_handle serverStopEvent{ nullptr };
	wil::unique_handle injectionReadyEvent{ nullptr };
	wil::unique_handle injectionStopEvent{ nullptr };
	wil::unique_handle injectionRunEvent{ nullptr };
	wil::unique_handle injectionWakeEvent{ nullptr };
	OpenGlass::GlassService::ThreadControl serverControl{};
	OpenGlass::GlassService::ThreadControl injectionControl{};
};
HRESULT WaitForThreadReady(
	HANDLE threadHandle,
	HANDLE readyEvent,
	HANDLE dependencyThreadHandle = nullptr
)
{
	HANDLE handles[]{ threadHandle, readyEvent, dependencyThreadHandle };
	const DWORD handleCount = dependencyThreadHandle ? 3ul : 2ul;
	if (dependencyThreadHandle)
	{
		std::swap(handles[1], handles[2]);
	}
	const HANDLE* waitHandles = handles;
	const auto waitResult = WaitForMultipleObjects(
		handleCount,
		waitHandles,
		FALSE,
		INFINITE
	);
	RETURN_LAST_ERROR_IF(waitResult == WAIT_FAILED);

	const auto signaledIndex = waitResult - WAIT_OBJECT_0;
	RETURN_HR_IF(E_UNEXPECTED, signaledIndex >= handleCount);
	if (signaledIndex == handleCount - 1)
	{
		return S_OK;
	}

	DWORD exitCode{};
	RETURN_IF_WIN32_BOOL_FALSE(GetExitCodeThread(waitHandles[signaledIndex], &exitCode));
	const auto result = static_cast<HRESULT>(exitCode);
	return FAILED(result) ? result : E_UNEXPECTED;
}

void ReportServiceStatus(
	SERVICE_STATUS_HANDLE statusHandle,
	DWORD currentState,
	DWORD win32ExitCode = NO_ERROR,
	DWORD waitHint = 0
)
{
	static SERVICE_STATUS s_status
	{
		.dwServiceType = SERVICE_WIN32_OWN_PROCESS,
		.dwCurrentState = SERVICE_START_PENDING,
		.dwControlsAccepted = 0,
		.dwWin32ExitCode = NO_ERROR,
		.dwServiceSpecificExitCode = 0,
		.dwCheckPoint = 0,
		.dwWaitHint = 0
	};
	s_status.dwCurrentState = currentState;
	s_status.dwWin32ExitCode = win32ExitCode;
	s_status.dwWaitHint = waitHint;

	switch (currentState)
	{
	case SERVICE_START_PENDING:
	case SERVICE_STOP_PENDING:
	case SERVICE_PAUSE_PENDING:
	case SERVICE_CONTINUE_PENDING:
	{
		s_status.dwControlsAccepted = 0;
		s_status.dwCheckPoint++;
		break;
	}
	case SERVICE_RUNNING:
	{
		s_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE;
		s_status.dwCheckPoint = 0;
		break;
	}
	case SERVICE_PAUSED:
	{
		s_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE;
		s_status.dwCheckPoint = 0;
		break;
	}
	default:
		s_status.dwControlsAccepted = 0;
		s_status.dwCheckPoint = 0;
		break;
	}

	LOG_IF_WIN32_BOOL_FALSE(SetServiceStatus(statusHandle, &s_status));
}

DWORD WINAPI HandlerEx(
	DWORD dwControl,
	[[maybe_unused]] DWORD dwEventType,
	[[maybe_unused]] LPVOID lpEventData,
	[[maybe_unused]] LPVOID lpContext
)
{
	using namespace OpenGlass;

	const auto& context = *reinterpret_cast<ServiceHandlerContext*>(lpContext);

	switch (dwControl)
	{
	case SERVICE_CONTROL_INTERROGATE:
	{
		return NO_ERROR;
	}

	case SERVICE_CONTROL_STOP:
	{
		ReportServiceStatus(context.statusHandle, SERVICE_STOP_PENDING, NO_ERROR, 10);
		LOG_IF_FAILED(
			GlassService::ControlThread(
				context.injectionControl,
				GlassService::ThreadStatus::Stopped
			)
		);
		return NO_ERROR;
	}

	case SERVICE_CONTROL_PAUSE:
	{
		ReportServiceStatus(context.statusHandle, SERVICE_PAUSE_PENDING, NO_ERROR, 10);
		LOG_IF_FAILED(
			GlassService::ControlThread(
				context.injectionControl,
				GlassService::ThreadStatus::Paused
			)
		);
		ReportServiceStatus(context.statusHandle, SERVICE_PAUSED);
		return NO_ERROR;
	}

	case SERVICE_CONTROL_CONTINUE:
	{
		ReportServiceStatus(context.statusHandle, SERVICE_CONTINUE_PENDING, NO_ERROR, 10);
		LOG_IF_FAILED(
			GlassService::ControlThread(
				context.injectionControl,
				GlassService::ThreadStatus::Running
			)
		);
		ReportServiceStatus(context.statusHandle, SERVICE_RUNNING);
		return NO_ERROR;
	}

	default:
		return ERROR_CALL_NOT_IMPLEMENTED;
	}
}

EXTERN_C VOID WINAPI ServiceMain(
	[[maybe_unused]] DWORD dwNumServicesArgs,
	[[maybe_unused]] LPWSTR* lpServiceArgVectors
)
{
	using namespace OpenGlass;

	wil::SetResultLoggingCallback([](const wil::FailureInfo& failure) static noexcept
	{
		WCHAR logMessage[MAX_PATH]{};
		if (FAILED(wil::GetFailureLogString(logMessage, MAX_PATH, failure)))
		{
			return;
		}

		OutputDebugStringW(logMessage);
	});

	ServiceHandlerContext context{};

	// Register service control handler
	context.statusHandle = RegisterServiceCtrlHandlerExW(c_serviceName, HandlerEx, &context);
	if (!context.statusHandle)
	{
		LOG_WIN32(GetLastError());
		return;
	}

	// Initialize service state
	ReportServiceStatus(context.statusHandle, SERVICE_START_PENDING, NO_ERROR, 10);

	const auto initializeControls = [&context]() -> HRESULT
	{
		const auto createEvent = [](wil::unique_handle& event, BOOL manualReset, BOOL initialState) -> HRESULT
		{
			event.reset(CreateEventW(nullptr, manualReset, initialState, nullptr));
			RETURN_LAST_ERROR_IF_NULL(event);
			return S_OK;
		};

		RETURN_IF_FAILED(createEvent(context.serverReadyEvent, TRUE, FALSE));
		RETURN_IF_FAILED(createEvent(context.serverStopEvent, TRUE, FALSE));
		RETURN_IF_FAILED(createEvent(context.injectionReadyEvent, TRUE, FALSE));
		RETURN_IF_FAILED(createEvent(context.injectionStopEvent, TRUE, FALSE));
		RETURN_IF_FAILED(createEvent(context.injectionRunEvent, TRUE, TRUE));
		RETURN_IF_FAILED(createEvent(context.injectionWakeEvent, FALSE, FALSE));

		context.serverControl =
		{
			.readyEvent = context.serverReadyEvent.get(),
			.stopEvent = context.serverStopEvent.get()
		};
		context.injectionControl =
		{
			.readyEvent = context.injectionReadyEvent.get(),
			.stopEvent = context.injectionStopEvent.get(),
			.runEvent = context.injectionRunEvent.get(),
			.wakeEvent = context.injectionWakeEvent.get(),
			.dependencyReadyEvent = context.serverReadyEvent.get()
		};
		return S_OK;
	};
	if (const auto result = initializeControls(); FAILED(result))
	{
		LOG_HR(result);
		ReportServiceStatus(context.statusHandle, SERVICE_STOPPED, HRESULT_CODE(result), 0);
		return;
	}

	// Start the pipe server and wait until the pipe exists before injection can begin.
	context.serverThreadHandle.reset(
		CreateThread(
			nullptr,
			0,
			GlassService::ServerThreadEntryPoint,
			&context.serverControl,
			0,
			nullptr
		)
	);
	if (!context.serverThreadHandle)
	{
		const auto error = GetLastError();
		LOG_WIN32(error);
		ReportServiceStatus(context.statusHandle, SERVICE_STOPPED, error, 0);
		return;
	}
	if (const auto result = WaitForThreadReady(context.serverThreadHandle.get(), context.serverReadyEvent.get()); FAILED(result))
	{
		LOG_HR(result);
		LOG_IF_FAILED(GlassService::ControlThread(context.serverControl, GlassService::ThreadStatus::Stopped));
		LOG_LAST_ERROR_IF(WaitForSingleObject(context.serverThreadHandle.get(), INFINITE) == WAIT_FAILED);
		ReportServiceStatus(context.statusHandle, SERVICE_STOPPED, HRESULT_CODE(result), 0);
		return;
	}

	context.injectionThreadHandle.reset(
		CreateThread(
			nullptr,
			0,
			GlassService::InjectionThreadEntryPoint,
			&context.injectionControl,
			0,
			nullptr
		)
	);
	if (!context.injectionThreadHandle)
	{
		const auto error = GetLastError();
		LOG_WIN32(error);
		ReportServiceStatus(context.statusHandle, SERVICE_STOP_PENDING, NO_ERROR, 10);
		LOG_IF_FAILED(GlassService::ControlThread(context.serverControl, GlassService::ThreadStatus::Stopped));
		LOG_LAST_ERROR_IF(WaitForSingleObject(context.serverThreadHandle.get(), INFINITE) == WAIT_FAILED);
		ReportServiceStatus(context.statusHandle, SERVICE_STOPPED, error, 0);
		return;
	}
	if (
		const auto result = WaitForThreadReady(
			context.injectionThreadHandle.get(),
			context.injectionReadyEvent.get(),
			context.serverThreadHandle.get()
		);
		FAILED(result)
	)
	{
		LOG_HR(result);
		ReportServiceStatus(context.statusHandle, SERVICE_STOP_PENDING, NO_ERROR, 10);
		LOG_IF_FAILED(GlassService::ControlThread(context.injectionControl, GlassService::ThreadStatus::Stopped));
		LOG_LAST_ERROR_IF(WaitForSingleObject(context.injectionThreadHandle.get(), INFINITE) == WAIT_FAILED);
		LOG_IF_FAILED(GlassService::ControlThread(context.serverControl, GlassService::ThreadStatus::Stopped));
		LOG_LAST_ERROR_IF(WaitForSingleObject(context.serverThreadHandle.get(), INFINITE) == WAIT_FAILED);
		ReportServiceStatus(context.statusHandle, SERVICE_STOPPED, HRESULT_CODE(result), 0);
		return;
	}

	// Both workers have published their control state.
	ReportServiceStatus(context.statusHandle, SERVICE_RUNNING);

	LOG_LAST_ERROR_IF(WaitForSingleObject(context.injectionThreadHandle.get(), INFINITE) == WAIT_FAILED);

	LOG_IF_FAILED(GlassService::ControlThread(context.serverControl, GlassService::ThreadStatus::Stopped));
	LOG_LAST_ERROR_IF(WaitForSingleObject(context.serverThreadHandle.get(), INFINITE) == WAIT_FAILED);
	ReportServiceStatus(context.statusHandle, SERVICE_STOPPED);

	return;
}
