#include "SmapiLauncher.h"

#if defined(__linux__)
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cwctype>
#include <string>
#include <utility>
#include <vector>

// ------ HELPERS (START) ------

namespace {

/// @brief Parses string of multiple arguments into vector of individual arguments.
/// @param arguments String containing multiple arguments.
/// @param result Vector containing extracted arguments.
/// @return Whether arguments was parsed successfully.
bool splitArguments(const std::wstring& arguments, std::vector<std::wstring>* result) {
    // Loop through each char in arguments.
    for (std::size_t index = 0; index < arguments.size();) {
        // Skip over whitespaces. Stop here if index reaches end of arguments.
        while (index < arguments.size() && std::iswspace(arguments[index])) ++index;
        if (index == arguments.size()) break;

        std::wstring argument;
        wchar_t quote = L'\0';

        while (index < arguments.size()) {
            const wchar_t character = arguments[index++];
            if (character == L'\\' && index < arguments.size()) {
                argument += arguments[index++];
            } else if ((character == L'\'' || character == L'\"')) {
                if (quote == L'\0') {
                    quote = character;
                } else if (quote == character) {
                    quote = L'\0';
                } else {
                    argument += character;
                }
            } else if (quote == L'\0' && std::iswspace(character)) {
                break;
            } else {
                argument += character;
            }
        }

        if (quote != L'\0') return false;
        result->push_back(std::move(argument));
    }

    return true;
}

static bool setCloseOnExec(const int descriptor) {
    const int flags = fcntl(descriptor, F_GETFD);
    return flags != -1 && fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) != -1;
}

/// @brief Parses input string into Utf8 format.
/// @param input String to parse
/// @param output input string in Utf8 format.
/// @return Whether input was parsed successfully.
static bool toUtf8(const std::wstring& input, std::string* output) {
    for (const wchar_t character : input) {
        const auto codePoint = static_cast<unsigned int>(character);
        if (codePoint > 0x10FFFF || (codePoint >= 0xD800 && codePoint <= 0xDFFF)) return false;

        if (codePoint <= 0x7F) {
            output->push_back(static_cast<char>(codePoint));
        } else if (codePoint <= 0x7FF) {
            output->push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            output->push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else if (codePoint <= 0xFFFF) {
            output->push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            output->push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            output->push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else {
            output->push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            output->push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            output->push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            output->push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    return true;
}
}  // namespace
#endif

// ------ HELPERS (END) ------

// ------ LAUNCHER (START) ------

#if defined(__linux__)
bool SmapiLauncher::launch(const std::filesystem::path& gameDirectory,
                           const std::wstring& arguments) const {
    // Only proceed if Modding API is found.
    const auto executable = gameDirectory / "StardewModdingAPI";
    std::error_code error;
    if (!std::filesystem::is_regular_file(executable, error) || error) return false;

    // Only proceed if arguments are valid.
    std::vector<std::wstring> parsedArguments;
    if (!splitArguments(arguments, &parsedArguments)) return false;

    std::vector<std::string> utf8Arguments;
    utf8Arguments.reserve(parsedArguments.size());
    for (const auto& argument : parsedArguments) {
        std::string utf8Argument;
        if (!toUtf8(argument, &utf8Argument)) return false;
        utf8Arguments.push_back(std::move(utf8Argument));
    }

    std::vector<char*> processArguments;
    processArguments.reserve(utf8Arguments.size() + 2);
    processArguments.push_back(const_cast<char*>(executable.c_str()));
    for (auto& argument : utf8Arguments) processArguments.push_back(argument.data());
    processArguments.push_back(nullptr);

    int statusPipe[2] = {-1, -1};
    if (pipe(statusPipe) == -1 || !setCloseOnExec(statusPipe[1])) {
        if (statusPipe[0] != -1) close(statusPipe[0]);
        if (statusPipe[1] != -1) close(statusPipe[1]);
        return false;
    }

    const pid_t child = fork();
    if (child == -1) {
        close(statusPipe[0]);
        close(statusPipe[1]);
        return false;
    }

    if (child == 0) {
        close(statusPipe[0]);
        const pid_t launcher = fork();
        if (launcher > 0) _exit(0);

        if (launcher == 0) {
            if (chdir(gameDirectory.c_str()) == 0)
                execv(executable.c_str(), processArguments.data());
        }

        const int launchError = errno;
        (void)write(statusPipe[1], &launchError, sizeof(launchError));
        _exit(1);
    }

    close(statusPipe[1]);
    int status;
    while (waitpid(child, &status, 0) == -1 && errno == EINTR) {
    }

    int launchError;
    ssize_t bytesRead;
    do {
        bytesRead = read(statusPipe[0], &launchError, sizeof(launchError));
    } while (bytesRead == -1 && errno == EINTR);
    close(statusPipe[0]);
    return bytesRead == 0;
}
#else
bool SmapiLauncher::launch(const std::filesystem::path& gameDirectory,
                           const std::wstring& arguments) const {
    (void)gameDirectory;
    (void)arguments;
    return false;
}
#endif

// ------ LAUNCHER (END) ------
