// installer.cpp - native replacement for install-v18d.ps1 / restore-v18d.ps1
//
// Usage (double-click for an interactive menu):
//   installer.exe [--install | --restore] [--path <WarframeDir>] [--payload <dll>] [--yes]
//
//   --install          copy payload\dwmapi.dll (v18-diag) into the game folder (default)
//   --restore          copy payload-v17\dwmapi.dll (known-good v17) back into the game folder
//   --path <dir>       Warframe folder; otherwise auto-detected from Steam, then the v18d default
//   --payload <dll>    install a custom dwmapi.dll instead of payload\dwmapi.dll
//   --yes              non-interactive: no menu, no prompts, no "press Enter" at the end
//
// Build (MinGW / WinLibs):
//   g++ -std=c++17 -O2 -static -municode installer.cpp -o installer.exe -lbcrypt -ladvapi32
// Build (MSVC, Developer Prompt):
//   cl /std:c++17 /O2 /EHsc /utf-8 installer.cpp /Fe:installer.exe bcrypt.lib advapi32.lib

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <tlhelp32.h>

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static const wchar_t *kDefaultPath = L"E:\\SteamLibrary\\steamapps\\common\\Warframe";
static const wchar_t *kGameExe = L"Warframe.x64.exe";
static const wchar_t *kTargetDll = L"dwmapi.dll";
static const wchar_t *kBackupSubdir = L"OpenWF\\u44-v18d-backup";
static const wchar_t *kLogName = L"OpenWF\\spaceninja-u44-v18.log";

struct InstallError {
    std::wstring msg;
};

static void fail(const std::wstring &msg) { throw InstallError{msg}; }

static fs::path exeDir() {
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
        if (n < buf.size()) {
            buf.resize(n);
            break;
        }
        buf.resize(buf.size() * 2);
    }
    return fs::path(buf).parent_path();
}

static std::wstring trim(std::wstring s) {
    const wchar_t *ws = L" \t\r\n\"";
    s.erase(0, s.find_first_not_of(ws));
    size_t end = s.find_last_not_of(ws);
    s.erase(end == std::wstring::npos ? 0 : end + 1);
    return s;
}

static std::wstring readLine() {
    std::wstring line;
    std::getline(std::wcin, line);
    return trim(line);
}

// ---------------------------------------------------------------------------
// Game folder detection

static bool isWarframeDir(const fs::path &p) {
    std::error_code ec;
    return !p.empty() && fs::exists(p / kGameExe, ec);
}

static std::optional<std::wstring> regString(HKEY root, const wchar_t *key, const wchar_t *value) {
    wchar_t buf[MAX_PATH * 2];
    DWORD size = sizeof(buf);
    if (RegGetValueW(root, key, value, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS)
        return std::nullopt;
    return std::wstring(buf);
}

// Steam library roots: the Steam install itself plus every "path" in libraryfolders.vdf.
static std::vector<fs::path> steamLibraries() {
    std::vector<fs::path> libs;
    std::optional<std::wstring> steam = regString(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath");
    if (!steam)
        steam = regString(HKEY_LOCAL_MACHINE, L"SOFTWARE\\WOW6432Node\\Valve\\Steam", L"InstallPath");
    if (!steam)
        return libs;

    fs::path root = fs::path(*steam).make_preferred();
    libs.push_back(root);

    std::ifstream vdf(root / L"steamapps" / L"libraryfolders.vdf");
    if (!vdf)
        return libs;
    std::stringstream ss;
    ss << vdf.rdbuf();
    std::string text = ss.str();

    static const std::regex re("\"path\"\\s+\"([^\"]+)\"");
    for (std::sregex_iterator it(text.begin(), text.end(), re), end; it != end; ++it) {
        std::string raw = (*it)[1].str();
        std::string unescaped;
        for (size_t i = 0; i < raw.size(); ++i) {
            if (raw[i] == '\\' && i + 1 < raw.size() && raw[i + 1] == '\\')
                ++i;
            unescaped += raw[i];
        }
        int n = MultiByteToWideChar(CP_UTF8, 0, unescaped.c_str(), -1, nullptr, 0);
        std::wstring w(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, unescaped.c_str(), -1, w.data(), n);
        w.resize(n - 1);
        libs.push_back(fs::path(w).make_preferred());
    }
    return libs;
}

static std::optional<fs::path> detectWarframe() {
    for (const fs::path &lib : steamLibraries()) {
        fs::path candidate = lib / L"steamapps" / L"common" / L"Warframe";
        if (isWarframeDir(candidate))
            return candidate;
    }
    if (isWarframeDir(kDefaultPath))
        return fs::path(kDefaultPath);
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// Helpers mirroring the PowerShell script

static bool warframeRunning() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    bool found = false;
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, kGameExe) == 0) {
            found = true;
            break;
        }
    }
    CloseHandle(snap);
    return found;
}

static std::wstring sha256(const fs::path &file) {
    std::ifstream in(file, std::ios::binary);
    if (!in)
        fail(L"Cannot open " + file.wstring() + L" for hashing.");

    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) != 0)
        fail(L"BCryptOpenAlgorithmProvider failed.");
    if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) != 0) {
        BCryptCloseAlgorithmProvider(alg, 0);
        fail(L"BCryptCreateHash failed.");
    }

    std::vector<char> buf(1 << 16);
    while (in) {
        in.read(buf.data(), buf.size());
        if (std::streamsize n = in.gcount())
            BCryptHashData(hash, reinterpret_cast<PUCHAR>(buf.data()), (ULONG)n, 0);
    }
    UCHAR digest[32];
    BCryptFinishHash(hash, digest, sizeof(digest), 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);

    wchar_t hex[65];
    for (int i = 0; i < 32; ++i)
        swprintf(hex + i * 2, 3, L"%02X", digest[i]);
    return hex;
}

static std::wstring timestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    wchar_t buf[32];
    wcsftime(buf, 32, L"%Y%m%d-%H%M%S", &tm);
    return buf;
}

static void copyVerified(const fs::path &src, const fs::path &dst) {
    std::error_code ec;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec)
        fail(L"Copy to " + dst.wstring() + L" failed: " + fs::path(ec.message()).wstring());
    if (sha256(src) != sha256(dst))
        fail(L"Copy verification failed.");
}

static void checkPreconditions(const fs::path &game, const fs::path &payload) {
    if (!isWarframeDir(game))
        fail(std::wstring(kGameExe) + L" not found in " + game.wstring());
    if (warframeRunning())
        fail(L"Close Warframe first (the DLL is locked while the game runs).");
    std::error_code ec;
    if (!fs::exists(payload, ec))
        fail(L"Payload not found: " + payload.wstring());
}

// ---------------------------------------------------------------------------
// Actions

static void doInstall(const fs::path &game, const fs::path &payload) {
    checkPreconditions(game, payload);
    fs::path target = game / kTargetDll;
    fs::path backupDir = game / kBackupSubdir;

    std::error_code ec;
    fs::create_directories(backupDir, ec);
    if (ec)
        fail(L"Cannot create " + backupDir.wstring());
    if (fs::exists(target, ec)) {
        fs::path backup = backupDir / (L"dwmapi-" + timestamp() + L".dll");
        fs::copy_file(target, backup, fs::copy_options::overwrite_existing, ec);
        if (ec)
            fail(L"Backup of existing " + target.wstring() + L" failed.");
    }

    copyVerified(payload, target);
    std::wcout << L"Installed " << payload.filename().wstring() << L" as " << target.wstring()
               << L" (hash verified). Previous dwmapi.dll is in " << backupDir.wstring() << L"\n"
               << L"Launch Warframe, log in, then check " << (game / kLogName).wstring() << L"\n";
}

static void doRestore(const fs::path &game, const fs::path &good) {
    checkPreconditions(game, good);
    fs::path target = game / kTargetDll;
    copyVerified(good, target);
    std::wcout << L"Restored the known-good v17 DLL to " << target.wstring() << L" (hash verified).\n";
}

// ---------------------------------------------------------------------------

enum class Mode { Menu, Install, Restore };

static void usage() {
    std::wcout << L"Usage: installer.exe [--install | --restore] [--path <WarframeDir>] [--payload <dll>] [--yes]\n";
}

static fs::path askForPath() {
    for (;;) {
        std::wcout << L"Enter the Warframe folder (contains " << kGameExe << L"): ";
        fs::path p = readLine();
        if (isWarframeDir(p))
            return fs::absolute(p);
        std::wcout << L"  " << kGameExe << L" not found there, try again.\n";
    }
}

int wmain(int argc, wchar_t **argv) {
    Mode mode = Mode::Menu;
    bool yes = false;
    fs::path game, payload;

    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--install")
            mode = Mode::Install;
        else if (a == L"--restore")
            mode = Mode::Restore;
        else if (a == L"--yes" || a == L"-y")
            yes = true;
        else if ((a == L"--path" || a == L"-WarframePath") && i + 1 < argc)
            game = argv[++i];
        else if (a == L"--payload" && i + 1 < argc)
            payload = argv[++i];
        else {
            usage();
            return a == L"--help" || a == L"-h" || a == L"/?" ? 0 : 2;
        }
    }
    if (yes && mode == Mode::Menu)
        mode = Mode::Install;

    const fs::path here = exeDir();
    const fs::path v18 = payload.empty() ? here / L"payload" / kTargetDll : fs::absolute(payload);
    const fs::path v17 = here / L"payload-v17" / kTargetDll;

    int rc = 0;
    try {
        if (game.empty()) {
            if (std::optional<fs::path> found = detectWarframe())
                game = *found;
            else if (yes)
                fail(L"Warframe folder not found; pass --path <dir>.");
            else
                game = askForPath();
        } else {
            game = fs::absolute(game).make_preferred();
        }

        if (mode == Mode::Menu) {
            std::wcout << L"WF Bootstrapper U44 - v18-diag installer\n"
                       << L"Warframe folder: " << game.wstring() << L"\n\n"
                       << L"  [1] Install v18-diag  (" << v18.wstring() << L")\n"
                       << L"  [2] Restore v17       (" << v17.wstring() << L")\n"
                       << L"  [3] Change Warframe folder\n"
                       << L"  [Q] Quit\n";
            for (;;) {
                std::wcout << L"> ";
                std::wstring c = readLine();
                if (c == L"1") { mode = Mode::Install; break; }
                if (c == L"2") { mode = Mode::Restore; break; }
                if (c == L"3") { game = askForPath(); std::wcout << L"Warframe folder: " << game.wstring() << L"\n"; continue; }
                if (c == L"q" || c == L"Q" || std::wcin.eof()) return 0;
            }
        }

        if (mode == Mode::Install)
            doInstall(game, v18);
        else
            doRestore(game, v17);
    } catch (const InstallError &e) {
        std::wcerr << L"ERROR: " << e.msg << L"\n";
        rc = 1;
    } catch (const std::exception &e) {
        std::wcerr << L"ERROR: " << e.what() << L"\n";
        rc = 1;
    }

    if (!yes) {
        std::wcout << L"\nPress Enter to close...";
        readLine();
    }
    return rc;
}
