#include "audio-process.hpp"

#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>

namespace wf_audio
{
namespace detail
{

static ProcessHooks g_hooks;

ProcessHooks& process_hooks()
{
    return g_hooks;
}

void reset_process_hooks()
{
    g_hooks = ProcessHooks{};
}

bool path_exists_real(const std::string& path)
{
    return access(path.c_str(), F_OK) == 0;
}

std::string read_text_file_real(const std::string& path)
{
    std::ifstream in(path);
    if (!in)
    {
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool run_capture_real(const std::vector<std::string>& argv, std::string& out, int& exit_code)
{
    out.clear();
    exit_code = -1;
    if (argv.empty())
    {
        return false;
    }

    int pipefd[2];
    if (pipe(pipefd) != 0)
    {
        return false;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0)
    {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);
        std::vector<std::string> args = argv;
        std::vector<char*> cargv;
        cargv.reserve(args.size() + 1);
        for (auto& s : args)
        {
            cargv.push_back(s.data());
        }
        cargv.push_back(nullptr);
        execvp(cargv[0], cargv.data());
        _exit(127);
    }

    close(pipefd[1]);
    std::array<char, 4096> buf{};
    ssize_t n;
    while ((n = read(pipefd[0], buf.data(), buf.size())) > 0)
    {
        out.append(buf.data(), static_cast<size_t>(n));
    }
    close(pipefd[0]);

    int status = 0;
    if (waitpid(pid, &status, 0) < 0)
    {
        return false;
    }
    if (WIFEXITED(status))
    {
        exit_code = WEXITSTATUS(status);
    }
    return true;
}

bool run_capture(const std::vector<std::string>& argv, std::string& out, int& exit_code)
{
    if (g_hooks.run_capture)
    {
        return g_hooks.run_capture(argv, out, exit_code);
    }
    return run_capture_real(argv, out, exit_code);
}

bool path_exists(const std::string& path)
{
    if (g_hooks.path_exists)
    {
        return g_hooks.path_exists(path);
    }
    return path_exists_real(path);
}

std::string read_text_file(const std::string& path)
{
    if (g_hooks.read_text_file)
    {
        return g_hooks.read_text_file(path);
    }
    return read_text_file_real(path);
}

std::vector<std::string> split_lines(const std::string& s)
{
    std::vector<std::string> lines;
    std::string cur;
    for (char c : s)
    {
        if (c == '\n')
        {
            if (!cur.empty() && cur.back() == '\r')
            {
                cur.pop_back();
            }
            lines.push_back(cur);
            cur.clear();
        } else
        {
            cur.push_back(c);
        }
    }
    if (!cur.empty())
    {
        if (cur.back() == '\r')
        {
            cur.pop_back();
        }
        lines.push_back(cur);
    }
    return lines;
}

} // namespace detail
} // namespace wf_audio
