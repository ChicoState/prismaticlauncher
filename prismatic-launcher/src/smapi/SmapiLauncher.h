#pragma once

#include <filesystem>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>

class SmapiLauncher {
public:
	// Launch SMAPI from the game's installation directory.
	bool launch(const std::filesystem::path& gameDirectory,
				const std::wstring& arguments = L"") const {
		const auto executable = gameDirectory / L"StardewModdingAPI.exe";
		std::error_code error;
        
		if (!std::filesystem::is_regular_file(executable, error) || error)
			return false;

		std::wstring commandLine = L"\"" + executable.wstring() + L"\"";
		if (!arguments.empty())
			commandLine += L" " + arguments;

		STARTUPINFOW startupInfo{};
		startupInfo.cb = sizeof(startupInfo);
		PROCESS_INFORMATION processInfo{};
		if (!CreateProcessW(executable.c_str(), commandLine.data(), nullptr,
							nullptr, FALSE, 0, nullptr, gameDirectory.c_str(),
							&startupInfo, &processInfo))
			return false;

		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return true;
	}
};
#endif
