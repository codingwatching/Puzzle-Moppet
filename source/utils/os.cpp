
#include "utils/os.h"

#if defined(_IRR_WINDOWS_API_)
#include <windows.h>
#include <direct.h> // _chdir
#include <shlobj.h> //for folder paths
#else
#include <unistd.h>
#include <cerrno>
#include <dirent.h>
#include <pwd.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#include <CoreServices/CoreServices.h>
#endif

namespace utils
{
namespace os
{
bool openWebpageCalled = false;

core::stringc newline()
{
#if defined(_IRR_WINDOWS_API_)
    return core::stringc("\r\n");
#else
    return core::stringc("\n");
#endif
}

io::path getcwd()
{
    io::path result;
    size_t size = 1024;

    while (true)
    {
        char *buf = new char[size];

#if defined(_IRR_WINDOWS_API_)
        if (_getcwd(buf, size) == buf)
#else
        if (::getcwd(buf, size) == buf)
#endif
        {
            result = buf;
            delete[] buf;
            return result;
        }

        delete[] buf;

        if (errno != ERANGE)
            return "";

        size *= 2;

        if (size >= 16384) // 1024*2*2*2*2
        {
            const char *adjectives[5] = {"stupidly", "ridiculously", "crazy",
                                         "absurdly", "oh my that's a"};
            WARN << adjectives[rand() % 5] << " long working directory";
        }
    }
}

bool chdir(const io::path &newPath)
{
    NOTE << "Changing directory to " << newPath;
#if defined(_IRR_WINDOWS_API_)
    return _chdir(newPath.c_str()) == 0;
#else
    // Specify default namespace to avoid recursion.
    return ::chdir(newPath.c_str()) == 0;
#endif
}

io::path getappdata()
{
#if defined(_IRR_WINDOWS_API_)
    // Windows
    char homePath[MAX_PATH];

    if (SHGetFolderPath(0, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0,
                        homePath) != S_OK)
    {
        WARN << "Could not locate Application Data directory.";
        return io::path();
    }
#else

#ifdef __APPLE__
    {
        FSRef fsRef;
        char homePath[PATH_MAX];

        if (FSFindFolder(kUserDomain, kApplicationSupportFolderType,
                         kCreateFolder, &fsRef) == noErr &&
            FSRefMakePath(&fsRef, (UInt8 *)homePath, sizeof(homePath)) == noErr)
        {
            return os::path::rtrim(io::path(homePath));
        }

        // If that fails, we use the unix method below.
        WARN << "Could not locate Mac application support folder, will fall "
                "back to user home.";
    }

#endif

    // Unix
    char *homePath = getenv("HOME");

    if (!homePath)
    {
        WARN << "Could not read HOME environment variable, will try getpwuid.";

        struct passwd *pwd = getpwuid(getuid());

        if (pwd)
            homePath = pwd->pw_dir;
        else
        {
            WARN << "Could not getpwuid either.";
            return io::path();
        }
    }

    char *xdgPath = getenv("XDG_DATA_HOME");
    if (xdgPath)
    {
        homePath = xdgPath;
    }
    else
    {
        return os::path::rtrim(os::path::concat(homePath, ".local/share"));
    }
#endif

    return os::path::rtrim(io::path(homePath));
}

io::path getcustomappdata(const io::path &dirName)
{
    io::path path = getappdata() + os::path::sepstr();

    path += dirName;

    os::path::ensure_dir(path);

    return path;
}

std::vector<io::path> listdir(const io::path &path)
{
    std::vector<io::path> items;

#if defined(_IRR_WINDOWS_API_)

    WIN32_FIND_DATA ffd;
    HANDLE hFind = FindFirstFile(os::path::concat(path, "*").c_str(), &ffd);

    if (hFind == INVALID_HANDLE_VALUE)
    {
        WARN << "Could not open dir (FindFirstFile): " << path;
        return items;
    }

    do
    {
        io::path itemName = ffd.cFileName;

        if (itemName != "." && itemName != "..")
            items.push_back(itemName);
    } while (FindNextFile(hFind, &ffd));

    if (GetLastError() != ERROR_NO_MORE_FILES)
        WARN << "Some kind of error when finding files: " << path;

    FindClose(hFind);

#else

    DIR *dp;

    if (!(dp = opendir(path.c_str())))
    {
        WARN << "Could not open dir (opendir): " << path;
        return items;
    }

    dirent *dirEntry;

    while ((dirEntry = readdir(dp)))
    {
        io::path itemName = dirEntry->d_name;

        if (itemName != "." && itemName != "..")
            items.push_back(itemName);
    }

    closedir(dp);

#endif

    return items;
}

std::vector<io::path> listfiles(const io::path &path)
{
    std::vector<io::path> all = listdir(path);
    std::vector<io::path> filesOnly;

    for (auto &elem : all)
    {
        if (path::is_file(path::concat(path, elem)))
            filesOnly.push_back(elem);
    }

    return filesOnly;
}

std::vector<io::path> listsubdirs(const io::path &path)
{
    std::vector<io::path> all = listdir(path);
    std::vector<io::path> dirsOnly;

    for (auto &elem : all)
    {
        if (path::is_dir(path::concat(path, elem)))
            dirsOnly.push_back(elem);
    }

    return dirsOnly;
}

io::path getexecutablepath()
{
#if defined(_IRR_WINDOWS_API_)

    io::path result = "";
    DWORD nSize = 1024;

    // This is not tested for very long filenames...

    while (true)
    {
        TCHAR *buf = new TCHAR[nSize];

        DWORD ret = GetModuleFileName(NULL, buf, nSize);

        if (ret == 0)
        {
            delete[] buf;
            WARN << "GetModuleFileName failed.";
            break;
        }
        else if (ret < nSize)
        {
            // success!
            result = buf;
            delete[] buf;
            break;
        }

        delete[] buf;

        nSize *= 2;

        if (nSize >= 16384)
            WARN << "Extremely long file path: " << s32(nSize);
    }

    return result;

#elif defined(__APPLE__)

    // This requires OS X 10.5+

    uint32_t nSize = PATH_MAX;
    char buf[nSize + 1];

    if (_NSGetExecutablePath(buf, &nSize) == 0)
        return io::path(buf);

    // failed, but nSize should now be updated with the correct size, so
    // next call should succeed.

    NOTE << "First call to _NSGetExecutablePath failed with PATH_MAX: "
         << PATH_MAX << ", will now try size " << nSize;

    char buf2[nSize + 1];

    if (_NSGetExecutablePath(buf2, &nSize) == 0)
        return io::path(buf2);

    WARN << "_NSGetExecutablePath failed completely.";

    return "";

#else

    // May not work on all Linux distributions...?
    // Only systems that have /proc/
    // Alternative is using argv.

    io::path exeLink = "/proc/";
    exeLink += getpid();
    exeLink += "/exe";

    char resolvedLink[PATH_MAX + 1];
    int len = readlink(exeLink.c_str(), resolvedLink, PATH_MAX);

    if (len == -1)
    {
        WARN << "Failed to read link: " << exeLink;
        return "";
    }

    resolvedLink[len] = 0;

    return resolvedLink;

#endif
}

// NOTE: The below is abandoned since I gave up trying to use SymFromAddr in
// MinGW. I was going to have FAIL and ASSERT display a backtrace before
// aborting.
/*

#if defined(_IRR_WINDOWS_API_)
#include <direct.h> // _chdir
#include <windows.h>
#include <specstrings.h> // for CaptureStackBackTrace function pointer
#include <imagehlp.h>
#endif

// This function should *NOT* output anything to the log system, since it is
// probably called from within the log system.
// Any errors can be returned as part of the backtrace.
std::vector<core::stringc> backtrace()
{
    std::vector<core::stringc> results;

#if defined(_IRR_WINDOWS_API_)

    const u32 DESIRED_FRAMES = 10;

    // The function we need...
    typedef USHORT (WINAPI *CaptureStackBackTraceType)(__in ULONG, __in ULONG,
__out PVOID *, __out_opt PULONG);

    HMODULE kernel32module = LoadLibrary("kernel32.dll");


    if (kernel32module)
    {
        CaptureStackBackTraceType CaptureStackBackTrace =
(CaptureStackBackTraceType)(GetProcAddress( kernel32module,
"RtlCaptureStackBackTrace"));

        if (CaptureStackBackTrace != NULL)
        {
            PVOID bt[DESIRED_FRAMES];
            USHORT numFrames = CaptureStackBackTrace(0,DESIRED_FRAMES,bt,NULL);

            for (u32 i = 0; i < numFrames; i ++)
            {
                // Need SymFromAddr or something...
            }
        }
        else
        {
            results.push_back("[No backtrace available, could not retrieve
CaptureStackBackTrace function]");
        }
    }
    else
    {
        results.push_back("[No backtrace available, could not load
kernel32.dll]");
    }

#else
    // Not implemented yet.
    // See http://www.linuxjournal.com/article/6391
#endif

    return results;
}
*/

bool run(const io::path &appPath, const std::vector<io::path> &args,
         u32 secondsBeforeStart)
{
    io::path fullAppPath;

    // Given appPath is a full path?
    if (os::path::contains_sep(appPath))
        fullAppPath = appPath;
    else
    {
        // Otherwise, we must search the PATH for it.
        fullAppPath = searchpath(appPath);

        if (fullAppPath.size() == 0)
        {
            WARN << "Failed to resolve to a full path: " << appPath;
            return false;
        }
    }

    // This check is mainly for Linux where it's hard to know if exec will
    // succeed. This is also the reason we need to get the full path above (and
    // so don't use execvp below).
    if (!os::path::is_executable(fullAppPath))
    {
        WARN << "Failed to run process, file is not executable: "
             << fullAppPath;
        return false;
    }

    // Debugging...

    NOTE << "Running process: " << fullAppPath;

    for (auto &arg : args)
        NOTE << "Arg: " << arg;

#if defined(_IRR_WINDOWS_API_)
    STARTUPINFO sinfo;
    FillMemory(&sinfo, sizeof(STARTUPINFO), 0);
    sinfo.cb = sizeof(STARTUPINFO);

    // Make the command line.
    // We wrap the exe path and each individual arg in quotes.

    io::path commandLine;
    commandLine += "\"";
    commandLine += fullAppPath;
    commandLine += "\"";

    for (u32 i = 0; i < args.size(); i++)
    {
        commandLine += " \"";
        commandLine += args[i];
        commandLine += "\"";
    }

    char *c_commandLine = new char[commandLine.size() + 1];

    if (c_commandLine)
    {
        strcpy(c_commandLine, commandLine.c_str());

        PROCESS_INFORMATION pinfo;

        if (CreateProcess(NULL, c_commandLine, NULL, NULL, FALSE,
                          NORMAL_PRIORITY_CLASS, NULL,
                          NULL, // current dir
                          &sinfo, &pinfo))
        {
            CloseHandle(pinfo.hProcess);
            CloseHandle(pinfo.hThread);
            delete[] c_commandLine;
            return true;
        }
        delete[] c_commandLine;
    }

    return false;
#else
    pid_t id = fork();

    if (id == -1)
    {
        WARN << "fork failed";
        return false;
    }
    else if (id == 0)
    {
        // Child process

        if (secondsBeforeStart > 0)
            sleep(secondsBeforeStart);

        char *argv[args.size() + 2];

        argv[0] = (char *)fullAppPath.c_str();

        for (u32 i = 0; i < args.size(); i++)
            argv[i + 1] = (char *)args[i].c_str();

        argv[args.size() + 1] = nullptr;

        execv(fullAppPath.c_str(), argv);

        // Will only reach here if exec fails.
        FAIL << "exec failed!";
    }

    // Parent process. execv has hopefully succeeded, since we previously
    // checked it was an executable. It's still possible it might not have
    // succeeded though...
    return true;
#endif
}

io::path searchpath(const io::path &appName)
{
    if (os::path::contains_sep(appName))
    {
        WARN << "app name should not be a full path " << appName;
        return "";
    }

    // Search current dir first
    io::path fullPath = os::path::concat(os::getcwd(), appName);

    if (os::path::is_file(fullPath))
    {
        NOTE << "Found " << appName << " with path: " << fullPath;
        return fullPath;
    }

    char *path = getenv("PATH");

    if (path)
    {
        // use : or ; for unix or windows.
#if defined(_IRR_WINDOWS_API_)
        std::vector<core::stringc> parts = str::explode(";", path);
#else
        std::vector<core::stringc> parts = str::explode(":", path);
#endif

        for (auto &part : parts)
        {
            fullPath = os::path::concat(part, appName);

            if (os::path::is_file(fullPath))
            {
                NOTE << "Found " << appName << " with path: " << fullPath;
                return fullPath;
            }
        }

        NOTE << appName << " not found in path.";
        return "";
    }
    else
        return "";
}

} // namespace os
} // namespace utils
