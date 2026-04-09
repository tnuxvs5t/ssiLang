#include "ssinet.hpp"

#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using ssilang::net::PipeChannel;
using ssilang::net::MapEntry;
using ssilang::net::Value;

namespace {

#ifdef _WIN32
PipeChannel open_named_client(const std::string& name, int wait_ms = 5000) {
    const std::string pipe_name = "\\\\.\\pipe\\" + name;
    const DWORD deadline = GetTickCount() + static_cast<DWORD>(wait_ms);

    for (;;) {
        HANDLE handle = CreateFileA(
            pipe_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );
        if (handle != INVALID_HANDLE_VALUE) {
            return PipeChannel(handle, handle, name, true);
        }

        const DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PIPE_BUSY) {
            throw std::runtime_error("Cannot open pipe client");
        }

        const DWORD now = GetTickCount();
        if (now >= deadline) {
            throw std::runtime_error("Timed out waiting for pipe server");
        }

        DWORD remain = deadline - now;
        if (remain > 200) {
            remain = 200;
        }
        WaitNamedPipeA(pipe_name.c_str(), remain);
        Sleep(25);
    }
}
#else
PipeChannel open_named_client(const std::string& name, int wait_ms = 5000) {
    const std::string pipe_name = "/tmp/sl_pipe_" + name;
    int waited = 0;
    while (waited <= wait_ms) {
        const int handle = ::open(pipe_name.c_str(), O_RDWR);
        if (handle >= 0) {
            return PipeChannel(handle, handle, name, true);
        }
        usleep(25000);
        waited += 25;
    }
    throw std::runtime_error("Timed out waiting for pipe server");
}
#endif

std::string as_string(const Value& value, const std::string& key, const std::string& fallback = "") {
    const Value* slot = value.find(key);
    if (slot == nullptr || !slot->is_string()) {
        return fallback;
    }
    return slot->as_string();
}

Value make_ok(const std::string& output, int exit_code) {
    return Value::map({
        MapEntry("ok", Value::boolean(true)),
        MapEntry("output", Value::string(output)),
        MapEntry("exit_code", Value::number(exit_code)),
    });
}

Value make_error(const std::string& error) {
    return Value::map({
        MapEntry("ok", Value::boolean(false)),
        MapEntry("error", Value::string(error)),
    });
}

#ifdef _WIN32

std::string quote_arg(const std::string& value) {
    if (value.empty()) {
        return "\"\"";
    }
    bool needs_quotes = false;
    for (char ch : value) {
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == '"') {
            needs_quotes = true;
            break;
        }
    }
    if (!needs_quotes) {
        return value;
    }

    std::string out = "\"";
    int backslashes = 0;
    for (char ch : value) {
        if (ch == '\\') {
            ++backslashes;
            continue;
        }
        if (ch == '"') {
            out.append(backslashes * 2 + 1, '\\');
            out.push_back('"');
            backslashes = 0;
            continue;
        }
        if (backslashes > 0) {
            out.append(backslashes, '\\');
            backslashes = 0;
        }
        out.push_back(ch);
    }
    if (backslashes > 0) {
        out.append(backslashes * 2, '\\');
    }
    out.push_back('"');
    return out;
}

std::string join_command_line(const std::vector<std::string>& argv) {
    std::ostringstream out;
    for (std::size_t i = 0; i < argv.size(); ++i) {
        if (i != 0) {
            out << ' ';
        }
        out << quote_arg(argv[i]);
    }
    return out.str();
}

std::string read_all_handle(HANDLE handle) {
    std::string out;
    std::array<char, 4096> buffer{};
    for (;;) {
        DWORD read = 0;
        const BOOL ok = ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr);
        if (!ok || read == 0) {
            break;
        }
        out.append(buffer.data(), buffer.data() + read);
    }
    return out;
}

Value run_process(const std::string& exe,
                  const std::string& script,
                  const std::string& cwd,
                  const std::string& input) {
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE stdout_read = nullptr;
    HANDLE stdout_write = nullptr;
    if (!CreatePipe(&stdout_read, &stdout_write, &sa, 0)) {
        throw std::runtime_error("CreatePipe stdout failed");
    }
    SetHandleInformation(stdout_read, HANDLE_FLAG_INHERIT, 0);

    HANDLE stdin_read = nullptr;
    HANDLE stdin_write = nullptr;
    if (!CreatePipe(&stdin_read, &stdin_write, &sa, 0)) {
        CloseHandle(stdout_read);
        CloseHandle(stdout_write);
        throw std::runtime_error("CreatePipe stdin failed");
    }
    SetHandleInformation(stdin_write, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = stdin_read;
    si.hStdOutput = stdout_write;
    si.hStdError = stdout_write;

    PROCESS_INFORMATION pi{};
    const std::vector<std::string> argv = {exe, script};
    std::string command_line = join_command_line(argv);

    BOOL ok = CreateProcessA(
        nullptr,
        command_line.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        cwd.empty() ? nullptr : cwd.c_str(),
        &si,
        &pi
    );

    CloseHandle(stdin_read);
    CloseHandle(stdout_write);

    if (!ok) {
        CloseHandle(stdin_write);
        CloseHandle(stdout_read);
        throw std::runtime_error("CreateProcess failed");
    }

    if (!input.empty()) {
        DWORD written = 0;
        WriteFile(stdin_write, input.data(), static_cast<DWORD>(input.size()), &written, nullptr);
    }
    CloseHandle(stdin_write);

    const std::string output = read_all_handle(stdout_read);
    CloseHandle(stdout_read);

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return make_ok(output, static_cast<int>(exit_code));
}

#else

std::string read_all_fd(int fd) {
    std::string out;
    std::array<char, 4096> buffer{};
    for (;;) {
        const ssize_t got = ::read(fd, buffer.data(), buffer.size());
        if (got <= 0) {
            break;
        }
        out.append(buffer.data(), static_cast<std::size_t>(got));
    }
    return out;
}

Value run_process(const std::string& exe,
                  const std::string& script,
                  const std::string& cwd,
                  const std::string& input) {
    int stdout_pipe[2];
    int stdin_pipe[2];
    if (::pipe(stdout_pipe) != 0 || ::pipe(stdin_pipe) != 0) {
        throw std::runtime_error("pipe() failed");
    }

    const pid_t pid = ::fork();
    if (pid < 0) {
        throw std::runtime_error("fork() failed");
    }

    if (pid == 0) {
        ::dup2(stdin_pipe[0], STDIN_FILENO);
        ::dup2(stdout_pipe[1], STDOUT_FILENO);
        ::dup2(stdout_pipe[1], STDERR_FILENO);
        ::close(stdin_pipe[0]);
        ::close(stdin_pipe[1]);
        ::close(stdout_pipe[0]);
        ::close(stdout_pipe[1]);
        if (!cwd.empty()) {
            ::chdir(cwd.c_str());
        }
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(exe.c_str()));
        argv.push_back(const_cast<char*>(script.c_str()));
        argv.push_back(nullptr);
        ::execvp(exe.c_str(), argv.data());
        std::fprintf(stderr, "execvp failed\n");
        std::_Exit(127);
    }

    ::close(stdin_pipe[0]);
    ::close(stdout_pipe[1]);

    if (!input.empty()) {
        ::write(stdin_pipe[1], input.data(), input.size());
    }
    ::close(stdin_pipe[1]);

    const std::string output = read_all_fd(stdout_pipe[0]);
    ::close(stdout_pipe[0]);

    int status = 0;
    ::waitpid(pid, &status, 0);
    int exit_code = 0;
    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        exit_code = 128 + WTERMSIG(status);
    }

    return make_ok(output, exit_code);
}

#endif

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: sl_runner_helper <pipe_name>\n";
        return 2;
    }

    try {
        PipeChannel channel = open_named_client(argv[1]);
        for (;;) {
            const Value request = channel.recv_net();
            const std::string cmd = as_string(request, "cmd");

            if (cmd == "run") {
                const std::string exe = as_string(request, "exe");
                const std::string script = as_string(request, "script");
                const std::string cwd = as_string(request, "cwd");
                const std::string input = as_string(request, "input");

                if (exe.empty()) {
                    channel.send_net(make_error("missing exe"));
                    continue;
                }
                if (script.empty()) {
                    channel.send_net(make_error("missing script"));
                    continue;
                }

                try {
                    channel.send_net(run_process(exe, script, cwd, input));
                } catch (const std::exception& e) {
                    channel.send_net(make_error(e.what()));
                }
                continue;
            }

            if (cmd == "stop") {
                channel.send_net(Value::map({MapEntry("ok", Value::boolean(true))}));
                return 0;
            }

            channel.send_net(make_error("unknown cmd: " + cmd));
        }
    } catch (const std::exception& e) {
        std::cerr << "sl_runner_helper failed: " << e.what() << "\n";
        return 1;
    }
}
