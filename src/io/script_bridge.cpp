#include "io/script_bridge.hpp"

#include <cstdlib>
#ifdef _WIN32
#include <process.h>
#endif
#include <sstream>
#include <stdexcept>

#include <vector>

namespace {

std::string quote_argument(const std::string& value) {
    std::string escaped = value;
    std::size_t position = 0;
    while ((position = escaped.find('"', position)) != std::string::npos) {
        escaped.insert(position, 1, '\\');
        position += 2;
    }
    return '"' + escaped + '"';
}

bool looks_like_workspace_root(const std::filesystem::path& path) {
    return std::filesystem::exists(path / "CMakeLists.txt")
        && std::filesystem::exists(path / "scripts")
        && std::filesystem::exists(path / "configs");
}

std::wstring quote_windows_spawn_argument(const std::wstring& value) {
    if (value.find_first_of(L" \t\"") == std::wstring::npos) {
        return value;
    }

    std::wstring quoted = L"\"";
    for (wchar_t ch : value) {
        if (ch == L'\"') {
            quoted += L'\\';
        }
        quoted += ch;
    }
    quoted += L"\"";
    return quoted;
}

}  // namespace

std::filesystem::path find_workspace_root(const std::filesystem::path& start_path) {
    std::filesystem::path cursor = std::filesystem::absolute(start_path).lexically_normal();
    if (std::filesystem::is_regular_file(cursor)) {
        cursor = cursor.parent_path();
    }

    while (!cursor.empty()) {
        if (looks_like_workspace_root(cursor)) {
            return cursor;
        }
        if (!cursor.has_parent_path() || cursor.parent_path() == cursor) {
            break;
        }
        cursor = cursor.parent_path();
    }

    throw std::runtime_error("unable to locate SABR workspace root from: " + start_path.string());
}

std::filesystem::path find_python_executable(const std::filesystem::path& workspace_root) {
    const std::vector<std::filesystem::path> candidates = {
        workspace_root / ".venv" / "Scripts" / "python.exe",
        workspace_root / ".venv" / "bin" / "python",
    };

    for (const std::filesystem::path& candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate.lexically_normal();
        }
    }

    return std::filesystem::path("python");
}

std::filesystem::path find_script_path(const std::filesystem::path& workspace_root,
                                       const std::string& script_name) {
    const std::filesystem::path script_path = (workspace_root / "scripts" / script_name).lexically_normal();
    if (!std::filesystem::exists(script_path)) {
        throw std::runtime_error("missing script: " + script_path.string());
    }
    return script_path;
}

int run_python_script(const std::filesystem::path& workspace_root,
                      const std::string& script_name,
                      const std::vector<std::string>& arguments) {
    const std::filesystem::path python = find_python_executable(workspace_root);
    const std::filesystem::path script = find_script_path(workspace_root, script_name);

#ifdef _WIN32
    std::vector<std::wstring> owned_arguments;
    owned_arguments.push_back(python.wstring());
    owned_arguments.push_back(quote_windows_spawn_argument(script.wstring()));
    for (const std::string& argument : arguments) {
        owned_arguments.push_back(quote_windows_spawn_argument(std::wstring(argument.begin(), argument.end())));
    }

    std::vector<const wchar_t*> argv;
    argv.reserve(owned_arguments.size() + 1);
    for (const std::wstring& argument : owned_arguments) {
        argv.push_back(argument.c_str());
    }
    argv.push_back(nullptr);

    const int exit_code = _wspawnvp(_P_WAIT, owned_arguments.front().c_str(), argv.data());
    if (exit_code == -1) {
        throw std::runtime_error("failed to launch Python interpreter: " + python.string());
    }
    return exit_code;
#else
    std::ostringstream command;
    command << quote_argument(python.string())
            << ' '
            << quote_argument(script.string());
    for (const std::string& argument : arguments) {
        command << ' ' << quote_argument(argument);
    }

    return std::system(command.str().c_str());
#endif
}