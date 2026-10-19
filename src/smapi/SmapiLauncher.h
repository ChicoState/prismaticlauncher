#pragma once

#include <filesystem>
#include <string>

class SmapiLauncher {
   public:
    bool launch(const std::filesystem::path& gameDirectory,
                const std::wstring& arguments = L"") const;
};
