#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr char kMagic[8] = {'L', 'D', 'R', 'V', 'P', 'K', 'G', '1'};
constexpr uint64_t kFooterSize = 16;

struct Options {
    bool silent = false;
    bool launch = true;
    bool shortcuts = true;
    fs::path installDir;
};

std::wstring Widen(const std::string& text) {
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring wide(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), wide.data(), size);
    return wide;
}

std::wstring Lower(std::wstring value) {
    for (wchar_t& ch : value) ch = static_cast<wchar_t>(towlower(ch));
    return value;
}

std::wstring KnownFolder(REFKNOWNFOLDERID folder) {
    PWSTR raw = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(folder, 0, nullptr, &raw)) && raw) {
        std::wstring result(raw);
        CoTaskMemFree(raw);
        return result;
    }
    return {};
}

std::wstring Env(const wchar_t* name) {
    DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0) return {};
    std::wstring value(size, L'\0');
    GetEnvironmentVariableW(name, value.data(), size);
    if (!value.empty() && value.back() == L'\0') value.pop_back();
    return value;
}

std::wstring PsLiteral(const fs::path& path) {
    std::wstring text = path.wstring();
    std::wstring escaped;
    escaped.reserve(text.size() + 2);
    escaped.push_back(L'\'');
    for (wchar_t ch : text) {
        escaped.push_back(ch);
        if (ch == L'\'') escaped.push_back(L'\'');
    }
    escaped.push_back(L'\'');
    return escaped;
}

void ShowError(const std::wstring& message) {
    MessageBoxW(nullptr, message.c_str(), L"Linear Drive Setup", MB_ICONERROR | MB_OK);
}

void ShowInfo(const std::wstring& message) {
    MessageBoxW(nullptr, message.c_str(), L"Linear Drive Setup", MB_ICONINFORMATION | MB_OK);
}

fs::path SelfPath() {
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (size == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }
    buffer.resize(size);
    return fs::path(buffer);
}

bool ExtractPayloadZip(const fs::path& destinationZip, std::wstring& error) {
    const fs::path self = SelfPath();
    std::ifstream input(self, std::ios::binary);
    if (!input) {
        error = L"Could not open setup executable.";
        return false;
    }

    input.seekg(0, std::ios::end);
    const uint64_t fileSize = static_cast<uint64_t>(input.tellg());
    if (fileSize < kFooterSize) {
        error = L"Setup payload is missing.";
        return false;
    }

    input.seekg(static_cast<std::streamoff>(fileSize - kFooterSize), std::ios::beg);
    uint64_t payloadSize = 0;
    char magic[8] {};
    input.read(reinterpret_cast<char*>(&payloadSize), sizeof(payloadSize));
    input.read(magic, sizeof(magic));
    if (!input || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0 || payloadSize == 0 || payloadSize > fileSize - kFooterSize) {
        error = L"Setup payload is corrupt or incomplete.";
        return false;
    }

    const uint64_t payloadOffset = fileSize - kFooterSize - payloadSize;
    input.seekg(static_cast<std::streamoff>(payloadOffset), std::ios::beg);
    fs::create_directories(destinationZip.parent_path());
    std::ofstream output(destinationZip, std::ios::binary | std::ios::trunc);
    if (!output) {
        error = L"Could not write temporary package.";
        return false;
    }

    std::vector<char> buffer(1024 * 1024);
    uint64_t remaining = payloadSize;
    while (remaining > 0) {
        const std::streamsize chunk = static_cast<std::streamsize>(std::min<uint64_t>(buffer.size(), remaining));
        input.read(buffer.data(), chunk);
        if (!input) {
            error = L"Could not read setup payload.";
            return false;
        }
        output.write(buffer.data(), chunk);
        remaining -= static_cast<uint64_t>(chunk);
    }
    return true;
}

DWORD RunHidden(const std::wstring& commandLine) {
    std::wstring mutableCommand = commandLine;
    STARTUPINFOW startup {};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process {};
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        return static_cast<DWORD>(-1);
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exitCode;
}

bool ExpandZip(const fs::path& zip, const fs::path& destination, std::wstring& error) {
    fs::remove_all(destination);
    fs::create_directories(destination);

    std::wstring command = L"powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"";
    command += L"Expand-Archive -LiteralPath ";
    command += PsLiteral(zip);
    command += L" -DestinationPath ";
    command += PsLiteral(destination);
    command += L" -Force\"";

    const DWORD exitCode = RunHidden(command);
    if (exitCode != 0) {
        std::wstringstream stream;
        stream << L"Could not extract package. PowerShell returned " << exitCode << L".";
        error = stream.str();
        return false;
    }
    return true;
}

bool CopyInstallTree(const fs::path& extractedRoot, const fs::path& installDir, std::wstring& error) {
    fs::path source = extractedRoot / L"LinearDrive-Windows";
    if (!fs::exists(source)) source = extractedRoot;
    const fs::path gameExe = source / L"LinearDrive.exe";
    if (!fs::exists(gameExe)) {
        error = L"The extracted package did not contain LinearDrive.exe.";
        return false;
    }

    fs::remove_all(installDir);
    fs::create_directories(installDir);
    fs::copy(source, installDir, fs::copy_options::recursive | fs::copy_options::overwrite_existing);
    return true;
}

bool CreateShortcut(const fs::path& linkPath, const fs::path& target, const fs::path& workingDir) {
    fs::create_directories(linkPath.parent_path());
    IShellLinkW* shellLink = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&shellLink));
    if (FAILED(hr) || !shellLink) return false;

    shellLink->SetPath(target.wstring().c_str());
    shellLink->SetWorkingDirectory(workingDir.wstring().c_str());
    shellLink->SetDescription(L"Play Linear Drive");

    IPersistFile* persist = nullptr;
    hr = shellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persist));
    if (SUCCEEDED(hr) && persist) {
        hr = persist->Save(linkPath.wstring().c_str(), TRUE);
        persist->Release();
    }
    shellLink->Release();
    return SUCCEEDED(hr);
}

void CreateShortcuts(const fs::path& installDir) {
    const fs::path gameExe = installDir / L"LinearDrive.exe";
    const std::wstring desktop = KnownFolder(FOLDERID_Desktop);
    if (!desktop.empty()) {
        CreateShortcut(fs::path(desktop) / L"Linear Drive.lnk", gameExe, installDir);
    }

    const std::wstring programs = KnownFolder(FOLDERID_Programs);
    if (!programs.empty()) {
        CreateShortcut(fs::path(programs) / L"Linear Drive" / L"Linear Drive.lnk", gameExe, installDir);
    }
}

Options ParseOptions() {
    Options options;
    std::wstring localAppData = KnownFolder(FOLDERID_LocalAppData);
    if (localAppData.empty()) localAppData = Env(L"LOCALAPPDATA");
    if (localAppData.empty()) localAppData = Env(L"TEMP");
    options.installDir = fs::path(localAppData) / L"LinearDrive";

    const int argc = __argc;
    wchar_t** argv = __wargv;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg = argv[i];
        const std::wstring lower = Lower(arg);
        if (lower == L"/silent" || lower == L"-silent" || lower == L"--silent") {
            options.silent = true;
        } else if (lower == L"/no_launch" || lower == L"/nolaunch" || lower == L"--no-launch") {
            options.launch = false;
        } else if (lower == L"/no_shortcuts" || lower == L"/noshortcuts" || lower == L"--no-shortcuts") {
            options.shortcuts = false;
        } else if (lower.rfind(L"/dir=", 0) == 0 || lower.rfind(L"--dir=", 0) == 0) {
            const size_t equals = arg.find(L'=');
            if (equals != std::wstring::npos) {
                options.installDir = fs::path(arg.substr(equals + 1));
            }
        }
    }
    return options;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    try {
        const Options options = ParseOptions();
        if (!options.silent) {
            std::wstring message = L"Install Linear Drive to:\n\n" + options.installDir.wstring();
            const int choice = MessageBoxW(nullptr, message.c_str(), L"Linear Drive Setup", MB_ICONINFORMATION | MB_OKCANCEL);
            if (choice != IDOK) {
                CoUninitialize();
                return 1;
            }
        }

        const fs::path tempRoot = fs::temp_directory_path() / L"LinearDriveSetup";
        const fs::path tempZip = tempRoot / L"LinearDrive-Windows.zip";
        const fs::path extractRoot = tempRoot / L"extract";

        std::wstring error;
        fs::remove_all(tempRoot);
        fs::create_directories(tempRoot);
        if (!ExtractPayloadZip(tempZip, error) || !ExpandZip(tempZip, extractRoot, error) || !CopyInstallTree(extractRoot, options.installDir, error)) {
            if (!options.silent) ShowError(error);
            CoUninitialize();
            return 1;
        }

        if (options.shortcuts) {
            CreateShortcuts(options.installDir);
        }

        fs::remove_all(tempRoot);
        const fs::path gameExe = options.installDir / L"LinearDrive.exe";
        if (!options.silent) {
            ShowInfo(L"Linear Drive installed successfully.");
        }
        if (options.launch) {
            ShellExecuteW(nullptr, L"open", gameExe.wstring().c_str(), nullptr, options.installDir.wstring().c_str(), SW_SHOWNORMAL);
        }
    } catch (const std::exception& ex) {
        ShowError(L"Setup failed:\n\n" + Widen(ex.what()));
        CoUninitialize();
        return 1;
    }

    CoUninitialize();
    return 0;
}
