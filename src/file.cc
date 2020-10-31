// See LICENSE for license details.

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <cerrno>
#include <cctype>

#include <string>
#include <vector>
#include <memory>
#include <algorithm>

#include "logger.h"
#include "file.h"

#include <array>

#include <fcntl.h>
#include <sys/stat.h>

#ifndef _WIN32
#include <dirent.h>
#include <sys/types.h>
#endif

#ifdef _WIN32
#include <io.h>
#include <Windows.h>
#include <Shlobj.h>
#include <KnownFolders.h>
#pragma comment(lib, "Shell32.lib")
#define stat _stat
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/mman.h>
#endif

#if defined (__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>
#endif

#if !defined (DIR_SEPARATOR)
#ifdef _WIN32
#define DIR_SEPARATOR  "\\"
#else
#define DIR_SEPARATOR  "/"
#endif
#endif

#ifdef _WIN32

/* file_win32_file */

struct file_win32_file : public file
{
private:
    HANDLE fh;
    HANDLE fmaph;
    ssize_t length;
    void *mapaddr;

public:
    file_win32_file(std::string path);
    ~file_win32_file();

    bool open(int mode);
    ssize_t read(void *buf, size_t buflen);
    const void* getBuffer();
    ssize_t getLength();
    int asFileDescriptor(ssize_t *outStart, ssize_t *outLength);
    ssize_t seek(ssize_t offset, int whence);
    void close();
};

file_win32_file::file_win32_file(std::string path) :
    file(path), fh(INVALID_HANDLE_VALUE), fmaph(INVALID_HANDLE_VALUE),
    length(-1), mapaddr(NULL) {}

file_win32_file::~file_win32_file()
{
    close();
}

bool file_win32_file::open(int mode)
{
    if (fh != INVALID_HANDLE_VALUE) {
        return false;
    }
    if ((fh = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
        errcode = GetLastError();
        Error("%s: CreateFile: errcode=%d: %s\n", __func__,
            errcode, path.c_str());
        return true;
    }
    return false;
}

ssize_t file_win32_file::read(void *buf, size_t buflen)
{
    if (open(0)) {
        return -1;
    }
    DWORD bytesRead;
    if (!ReadFile(fh, buf, (DWORD)buflen, &bytesRead, NULL)) {
        errcode = GetLastError();
        Error("%s: ReadFile: errcode=%d\n", __func__, errcode);
        return -1;
    }
    return bytesRead;
}

const void* file_win32_file::getBuffer()
{
    if (mapaddr) {
        return mapaddr;
    }
    if (open(0)) {
        return NULL;
    }
    if (getLength() <= 0) {
        return NULL;
    }
    if ((fmaph = CreateFileMapping(fh, NULL, PAGE_READONLY, 0, 0, NULL)) ==
        INVALID_HANDLE_VALUE) {
        errcode = GetLastError();
        Error("%s: CreateFileMapping: errcode=%d\n", __func__, errcode);
        return NULL;
    }
    void *addr;
    if ((addr = MapViewOfFile(fmaph, FILE_MAP_READ, 0, 0, 0)) == NULL) {
        errcode = GetLastError();
        Error("%s: MapViewOfFile: errcode=%d\n", __func__, errcode);
        return NULL;
    } else {
        Debug("%s MapViewOfFile: file=%s addr=%p\n", __func__,
            getBasename().c_str(), addr);
        return (mapaddr = addr);
    }
}

ssize_t file_win32_file::getLength()
{
    if (length != -1) {
        return length;
    }

    if (open(0)) {
        return -1;
    }

    struct _FILE_STANDARD_INFO finfo;
    if (!GetFileInformationByHandleEx(fh, FileStandardInfo, &finfo,
            sizeof(finfo))) {
        errcode = GetLastError();
        return -1;
    }

    return (length = finfo.EndOfFile.LowPart);
}

int file_win32_file::asFileDescriptor(ssize_t *outStart, ssize_t *outLength)
{
    if (open(0)) {
        return -1;
    }
    if (outStart) {
        *outStart = 0;
    }
    if (outLength) {
        *outLength = getLength();
    }
    return _open_osfhandle((intptr_t)fh, _O_RDONLY);
}

ssize_t file_win32_file::seek(ssize_t offset, int whence)
{
    if (open(0)) {
        return -1;
    }
    int moveMethod;
    switch(whence) {
        case 0: moveMethod = FILE_BEGIN;   break; /* SEEK_SET */
        case 1: moveMethod = FILE_CURRENT; break; /* SEEK_CUR */
        case 2: moveMethod = FILE_END;     break; /* SEEK_END */
    }

    ssize_t off = SetFilePointer(fh, (LONG)offset, 0, moveMethod);
    if (off == INVALID_SET_FILE_POINTER) {
        errcode = GetLastError();
        return -1;
    }
    return off;
}

void file_win32_file::close()
{
    file::close();

    if (mapaddr) {
        if (!UnmapViewOfFile(mapaddr)) {
            errcode = GetLastError();
            Error("%s: UnmapViewOfFile: errcode=%d\n", __func__, errcode);
        } else {
            Debug("%s: UnmapViewOfFile: file=%s\n", __func__,
                getBasename().c_str());
        }
        mapaddr = NULL;
    }

    if (fmaph != INVALID_HANDLE_VALUE) {
        CloseHandle(fmaph);
        fmaph = INVALID_HANDLE_VALUE;
    }
    
    if (fh != INVALID_HANDLE_VALUE) {
        CloseHandle(fh);
        fh = INVALID_HANDLE_VALUE;
        length = -1;
    }
}

#else

/* file_posix_file */

struct file_posix_file : public file
{
private:
    int fd;
    ssize_t length;
    void *mapaddr;

public:
    file_posix_file(std::string path);
    ~file_posix_file();

    bool open(int mode);
    ssize_t read(void *buf, size_t buflen);
    const void* getBuffer();
    ssize_t getLength();
    int asFileDescriptor(ssize_t *outStart, ssize_t *outLength);
    ssize_t seek(ssize_t offset, int whence);
    void close();
};

file_posix_file::file_posix_file(std::string path) :
    file(path), fd(-1), length(-1), mapaddr(NULL) {}

file_posix_file::~file_posix_file()
{
    close();
}

bool file_posix_file::open(int mode)
{
    if (fd != -1) {
        return false;
    }
    if ((fd = ::open(path.c_str(), O_RDONLY)) < 0) {
        errcode = errno;
        errmsg = strerror(errno);
        return true;
    }
    return false;
}

ssize_t file_posix_file::read(void *buf, size_t buflen)
{
    if (open(0)) {
        return -1;
    }
    ssize_t bytesRead = ::read(fd, buf, buflen);
    if (bytesRead < 0) {
        errcode = errno;
        errmsg = strerror(errno);
    }
    return bytesRead;
}

const void* file_posix_file::getBuffer()
{
    if (mapaddr) {
        return mapaddr;
    }
    if (open(0)) {
        return NULL;
    }
    if (getLength() <= 0) {
        return NULL;
    }
    void *addr;
    if ((addr = mmap(NULL, length, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
        Error("%s: mmap: %s\n", __func__, strerror(errno));
        return NULL;
    } else {
        Debug("%s: mmap: file=%s addr=%p\n", __func__,
            getBasename().c_str(), addr);
        return (mapaddr = addr);
    }
}

ssize_t file_posix_file::getLength()
{
    if (length != -1) {
        return length;
    }

    if (open(0)) {
        return -1;
    }

    struct stat statbuf;
    if (fstat(fd, &statbuf) < 0) {
        return -1;
    }

    return (length = statbuf.st_size);
}

int file_posix_file::asFileDescriptor(ssize_t *outStart, ssize_t *outLength)
{
    if (open(0)) {
        return -1;
    }
    if (outStart) {
        *outStart = 0;
    }
    if (outLength) {
        *outLength = getLength();
    }
    return fd;
}

ssize_t file_posix_file::seek(ssize_t offset, int whence)
{
    if (open(0)) {
        return -1;
    }
    ssize_t off = lseek(fd, offset, whence);
    if (off < 0) {
        errcode = errno;
        errmsg = strerror(errno);
    }
    return off;
}

void file_posix_file::close()
{
    file::close();

    if (mapaddr) {
        if (munmap(mapaddr, length) < 0) {
            Error("%s: munmap %s\n", __func__, strerror(errno));
        } else {
            Debug("%s: munmap: file=%s\n", __func__, getBasename().c_str());
        }
        mapaddr = NULL;
    }
    
    if (fd >= 0) {
        ::close(fd);
        fd = -1;
        length = -1;
    }
}

#endif


/* file */

#ifdef _WIN32
std::vector<std::string> file::list(std::string dirname)
{
    WIN32_FIND_DATA ffd;
    HANDLE hFind;
    char path[PATH_MAX];
    std::vector<std::string> list;

    std::string findname = dirname + "/*";
    hFind = FindFirstFileA(findname.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        Error("%s: FindFirstFileA error: %s: %d\n", __func__,
            dirname.c_str(), GetLastError());
        goto out;
    }

    do {
        snprintf(path, sizeof(path), "%s/%s",
                dirname.size() == 0 ? "." : dirname.c_str(), ffd.cFileName);
        list.push_back(path);
    } while (FindNextFileA(hFind, &ffd) != 0);

    FindClose(hFind);
out:
    return list;
}
#else
std::vector<std::string> file::list(std::string dirname)
{
    DIR *dirp;
    struct dirent *r;
    char path[PATH_MAX];
    std::vector<std::string> list;

    if (!(dirp = opendir(dirname.c_str()))) {
        Error("%s: opendir error: %s: %s", __func__,
            dirname.c_str(), strerror(errno));
        goto out;
    }

    while ((r = readdir(dirp))) {
        if (strcmp(r->d_name, ".") == 0 || strcmp(r->d_name, "..") == 0) {
            continue;
        }
        snprintf(path, sizeof(path), "%s/%s",
            dirname.size() == 0 ? "." : dirname.c_str(), r->d_name);
        list.push_back(path);
    }

    closedir(dirp);

out:
    return list;
}
#endif

std::string file::getPath()
{
    return path;
}

std::string file::getBasename()
{
#ifdef _WIN32
    size_t slashoffset = path.find_last_of('\\');
#else
    size_t slashoffset = path.find_last_of('/');
#endif
    if (slashoffset == std::string::npos) {
        return path;
    } else {
        return path.substr(slashoffset + 1);
    }
    return path;
}

std::string file::getErrorMessage()
{
    return errmsg;
}

int file::getErrorCode()
{
    return errcode;
}

#ifdef _WIN32
#define open _open
#define close _close
#define write _write
#endif

bool file::copyToPath(std::string destpath)
{
    int destfd;
    ssize_t bytesWritten = 0;
    ssize_t len;
    uint8_t buf[4096];

    ssize_t fileLength = getLength();
    if (fileLength == -1) {
        Error("%s: error opening file for copy: %s\n", __func__,
            getPath().c_str());
        return true;
    }

    if ((destfd = ::open(destpath.c_str(), O_RDWR | O_CREAT, 0600)) < 0) {
        errcode = errno;
        errmsg = strerror(errno);
        Error("%s: error opening file copy destination %s: %s\n", __func__,
            destpath.c_str(), errmsg.c_str());
        return true;
    }

    while ((len = read(buf, sizeof(buf))) > 0) {
        if (::write(destfd, buf, (unsigned)len) != len) {
            errcode = errno;
            errmsg = strerror(errno);
            ::close(destfd);
            remove(destpath.c_str());
            Error("%s: short write during copy: %s -> %s\n",
                    __func__, path.c_str(), destpath.c_str());
            return true;
        }
        bytesWritten += len;
    }
    ::close(destfd);

    if (bytesWritten != fileLength) {
        errcode = EIO;
        errmsg = "incomplete copy";
        Error("%s: incomplete copy: (length=%d written=%d) %s -> %s\n", __func__,
            fileLength, bytesWritten, path.c_str(), destpath.c_str());
        return true;
    }

    return false;
}

#ifdef _WIN32
#undef open
#undef close
#undef write
#endif

char* file::readLine(char *buf, size_t buflen)
{
    if (buflen == 0) return NULL;

    // periodically refill buffer
    if ((sbuf.size() - sbufOffset) < buflen) {
        ssize_t len;
        char rdbuf[4096];
        while((len = read(rdbuf, sizeof(rdbuf))) > 0) {
            sbuf.append(rdbuf, len);
            if (sbuf.size() - sbufOffset >= buflen) break;
        }
    }

    // return EOF if there is no data remaining
    if (sbufOffset >= sbuf.size()) {
        sbuf.resize(0);
        sbufOffset = 0;
        return NULL;
    }

    // read a line from the buffer
    size_t nextOffset, sizeToCopy = 0;
    if ((nextOffset = sbuf.find_first_of("\r\n", sbufOffset)) != std::string::npos) {
        sizeToCopy = nextOffset - sbufOffset;
    } else {
        sizeToCopy = sbuf.size() - sbufOffset;
    }
    if (sizeToCopy > buflen - 1) {
        sizeToCopy = buflen - 1;
    }
    if (sizeToCopy > 0) {
        memcpy(buf, sbuf.data() + sbufOffset, sizeToCopy);
    }
    buf[sizeToCopy] = '\0';

    // increment buffer position
    if (nextOffset == std::string::npos) {
        sbufOffset += sizeToCopy;
    } else if (nextOffset + 1 < sbuf.size() && sbuf[nextOffset] == '\r' &&
        sbuf[nextOffset + 1] == '\n') {
        sbufOffset += (sizeToCopy + 2);
    } else {
        sbufOffset += (sizeToCopy + 1);
    }

    // periodically trim buffer
    if (sbufOffset >= 4096 && sbufOffset <= sbuf.size()) {
        sbuf.erase(0, sbufOffset);
        sbufOffset = 0;
    }

    return buf;
}

void file::close()
{
    sbuf.resize(0);
    sbufOffset = 0;
}

bool file::dirExists(std::string dname)
{
    struct stat buf;
    if (stat(dname.c_str(), &buf)) {
        return false;
    }
    return ((buf.st_mode & S_IFDIR) != 0);
}

bool file::fileExists(std::string fname)
{
    struct stat buf;
    if (stat(fname.c_str(), &buf)) {
        return false;
    }
    return ((buf.st_mode & S_IFREG) != 0);
}

bool file::makeDir(std::string dname)
{
    if (dirExists(dname)) {
        return true;
    }
#ifdef _WIN32
    if (!CreateDirectoryA(dname.c_str(), NULL)) {
        Error("%s: *** Error: failed to make directory: %s\n", __func__,
            dname.c_str());
        return false;
    }
#else
    if (mkdir(dname.c_str(), 0777) < 0) {
        Error("%s: *** Error: failed to make directory: %s\n", __func__,
            dname.c_str());
        return false;
    }
#endif
    return true;
}

file_ptr file::getFile(std::string filename)
{
#ifdef _WIN32
    std::replace(filename.begin(), filename.end(), '/', '\\');
    return file_ptr(new file_win32_file(filename));
#else
    return file_ptr(new file_posix_file(filename));
#endif
}

#if defined (__APPLE__)

std::string file::getPath(std::string rsrcpath)
{    
    // Seperate the file path into path components
    CFStringRef rsrcStringRef = CFStringCreateWithCString
        (kCFAllocatorDefault, rsrcpath.c_str(), kCFStringEncodingMacRoman);
    CFArrayRef components = CFStringCreateArrayBySeparatingStrings
        (kCFAllocatorDefault, rsrcStringRef, CFSTR("/"));
    
    // See if we have a bundle
    CFStringRef bundleName = NULL;
    CFMutableStringRef bundlePath = NULL;
    CFStringRef lastComponent = NULL;
    for (int i=0; i < CFArrayGetCount(components); i++) {
        CFStringRef pathcomp = (CFStringRef)CFArrayGetValueAtIndex(components, i);
        if (bundleName) {
            // Accumulate path after bundle
            CFStringAppend(bundlePath, CFSTR("/"));
            CFStringAppend(bundlePath, pathcomp);
        } else {
            // See if we have a bundle in our path
            if (CFStringHasSuffix(pathcomp, CFSTR(".bundle"))) {
                bundleName = pathcomp;
                bundlePath = CFStringCreateMutable(kCFAllocatorDefault, 0);
            } else if (i == CFArrayGetCount(components) - 1) {
                // otherwise stash lastComponent
                lastComponent = pathcomp;
            }
        }
    }
    
    // Get name and type of file
    CFBundleRef mainBundle;
    CFURLRef fsURL;
    CFStringRef nameStringRef, typeStringRef, fsPathStringRef;
    
    if (bundleName) {
        CFRange result =
            CFStringFind(bundleName, CFSTR("."), kCFCompareBackwards);
        nameStringRef =
            CFStringCreateWithSubstring(kCFAllocatorDefault, bundleName,
                CFRangeMake(0, result.location));
        typeStringRef =
            CFStringCreateWithSubstring(kCFAllocatorDefault, bundleName,
                CFRangeMake(result.location + 1, CFStringGetLength(bundleName) -
                result.location - 1));        
    } else {
        CFRange result = CFStringFind(lastComponent, CFSTR("."),
            kCFCompareBackwards);
        nameStringRef =
            CFStringCreateWithSubstring(kCFAllocatorDefault, lastComponent,
                CFRangeMake(0, result.location));
        typeStringRef =
            CFStringCreateWithSubstring(kCFAllocatorDefault, lastComponent,
                CFRangeMake(result.location + 1,
                CFStringGetLength(lastComponent) - result.location - 1));        
    }

    // Create path to file
    std::string filepath;
    mainBundle = CFBundleGetMainBundle();
    fsURL = CFBundleCopyfileURL(mainBundle, nameStringRef, typeStringRef,
        NULL);
    if (fsURL) {
        fsPathStringRef = CFURLCopyFileSystemPath(fsURL, kCFURLPOSIXPathStyle);
        filepath = std::string(CFStringGetCStringPtr(fsPathStringRef,
            CFStringGetFastestEncoding(fsPathStringRef)));
        if (bundleName) {
            filepath += std::string(CFStringGetCStringPtr(bundlePath,
                CFStringGetFastestEncoding(bundlePath)));
        }
        CFRelease(fsURL);
        CFRelease(fsPathStringRef);
    }
    if (bundlePath) {
        CFRelease(bundlePath);
    }
    CFRelease(nameStringRef);
    CFRelease(typeStringRef);
    CFRelease(rsrcStringRef);
    CFRelease(components);
    
    return filepath;
}

file_ptr file::getResource(std::string rsrc)
{
    return file_ptr(new file_posix_file(getPath(rsrc)));
}

std::string file::getExecutablePath()
{
    std::string result;

    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);

    std::vector<char> buffer;
    buffer.resize(size + 1);

    _NSGetExecutablePath(buffer.data(), &size);
    buffer[size] = '\0';

    if (!strrchr(buffer.data(), '/'))
    {
        return "";
    }
    return buffer.data();
}

std::string file::getExecutableDirectory()
{
    std::string executablePath = getExecutablePath();
    size_t lastPathSepLoc      = executablePath.find_last_of("/");
    return (lastPathSepLoc != std::string::npos) ?
        executablePath.substr(0, lastPathSepLoc) : "";
}

std::string file::getTempDir()
{
    return std::string("/tmp");
}

std::string file::getHomeDir()
{
    return std::string(getenv("HOME"));
}

typedef Boolean (*funcptr_CFURLSetfilePropertyForKey)(CFURLRef url,
    CFStringRef key, CFTypeRef propertyValue, CFErrorRef *error);

#elif defined (_WIN32)

std::string file::getPath(std::string rsrcpath)
{
    char path[MAX_PATH];
    static std::string topDir;

    GetModuleFileNameA(NULL, path, MAX_PATH);

    // locate top directory by searching upwards from exe dir for presence
    // of the 'files' directory
    if (!topDir.length()) {
        std::string moduleDir = path;
        size_t last = moduleDir.length();
        while (last >= 0) {
            size_t offset;
            if ((offset = moduleDir.find_last_of("/\\", last)) ==
                std::string::npos) {
                break;
            }
            std::string dir = moduleDir.substr(0, offset);
            if (dirExists(dir + std::string(DIR_SEPARATOR) +
                std::string("files"))) {
                Debug("%s found files directory in : %s\n", __func__,
                    dir.c_str());
                topDir = dir;
                break;
            }
            Debug("%s searching for files directory in: %s\n", __func__,
                dir.c_str());
            last = offset - 1;
        }
    }
    std::string finalPath = topDir + std::string(DIR_SEPARATOR) + rsrcpath;
    std::replace(finalPath.begin(), finalPath.end(), '/', '\\');
    return finalPath;
}

file_ptr file::getResource(std::string rsrc)
{
    return file_ptr(new file_win32_file(getPath(rsrc)));
}

std::string file::getExecutablePath()
{
    std::array<char, MAX_PATH> executableFileBuf;
    DWORD executablePathLen = GetModuleFileNameA(nullptr,
        executableFileBuf.data(), static_cast<DWORD>(executableFileBuf.size()));
    return (executablePathLen > 0 ?
        std::string(executableFileBuf.data()) : "");
}

std::string file::getExecutableDirectory()
{
    std::string executablePath = getExecutablePath();
    size_t lastPathSepLoc      = executablePath.find_last_of("\\/");
    return (lastPathSepLoc != std::string::npos) ?
        executablePath.substr(0, lastPathSepLoc) : "";
}

std::string file::getTempDir()
{
    wchar_t wpath[MAX_PATH] = { 0 };
    char path[MAX_PATH];
    
    GetTempPathW(MAX_PATH, wpath);
    if (!WideCharToMultiByte(CP_UTF8, 0, wpath, -1, path, MAX_PATH,
        NULL, NULL)) {
        Error("%s: WideCharToMultiByte failed\n", __func__);
        return std::string();
    }
    return path;
}

std::string file::getHomeDir()
{
    PWSTR wpath;
    char path[MAX_PATH*2];
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, KF_FLAG_CREATE, NULL,
        &wpath) != S_OK) {
        Error("%s: SHGetKnownFolderPath failed\n", __func__);
        return std::string();
    }
    if (!WideCharToMultiByte(CP_UTF8, 0, wpath, -1, path, MAX_PATH*2, NULL,
        NULL)) {
        Error("%s: WideCharToMultiByte failed\n", __func__);
        return std::string();
    }
    return std::string(path);
}

#else

std::string file::getExecutablePath()
{
    // We can't use lstat to get the size of /proc/self/exe as it returns 0
    // so we just use a big buffer and hope the path fits in it.
    char path[4096];

    ssize_t result = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (result < 0 || static_cast<size_t>(result) >= sizeof(path) - 1) {
        return "";
    }

    path[result] = '\0';
    return path;
}

std::string file::getExecutableDirectory()
{
    std::string executablePath = getExecutablePath();
    size_t lastPathSepLoc      = executablePath.find_last_of("/");
    return (lastPathSepLoc != std::string::npos) ?
        executablePath.substr(0, lastPathSepLoc) : "";
}

std::string file::getPath(std::string rsrcpath)
{
    return std::string("./") + rsrcpath;
}

file_ptr file::getResource(std::string rsrc)
{
    return file_ptr(new file_posix_file(getPath(rsrc)));
}

std::string file::getTempDir()
{
    return std::string("/tmp");
}

std::string file::getHomeDir()
{
    return std::string(getenv("HOME"));
}
        
#endif
          
std::string file::dirName(std::string path)
{
    size_t offset;
    if ((offset = path.find_last_of("/\\")) == std::string::npos) {
        return ".";
    } else {
        return path.substr(0, offset);
    }
}

std::string file::baseName(std::string path)
{
    size_t offset;
    if ((offset = path.find_last_of("/\\")) == std::string::npos) {
        return path;
    } else {
        return path.substr(offset + 1);
    }
}
             
std::string file::getTempFile(std::string filename, std::string suffix)
{
    size_t npos;
    std::string tmpfilename;
    if ((npos = filename.find_last_of("/")) != std::string::npos) {
        tmpfilename = file::getTempDir() + filename.substr(npos) + suffix;
    } else if ((npos = filename.find_last_of("\\")) != std::string::npos) {
        tmpfilename = file::getTempDir() + filename.substr(npos) + suffix;
    } else {
        tmpfilename = file::getTempDir() + std::string(DIR_SEPARATOR) +
            filename + suffix;
    }
    return tmpfilename;
}    
