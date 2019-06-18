// See LICENSE for license details.

#pragma once

#ifdef _WIN32
typedef long long ssize_t;
#endif

struct file;
typedef std::shared_ptr<file> file_ptr;

struct file
{
    std::string sbuf;
    size_t sbufOffset;

    std::string path;
    std::string errmsg;
    int errcode;

    file(std::string path);
    virtual ~file() = default;

    std::string getPath();
    std::string getBasename();
    std::string getErrorMessage();
    int getErrorCode();
    bool copyToPath(std::string destpath);
    char* readLine(char *buf, size_t buflen);
    
    virtual bool open(int mode) = 0;
    virtual ssize_t read(void *buf, size_t buflen) = 0;
    virtual const void* getBuffer() = 0;
    virtual ssize_t getLength() = 0;
    virtual int asFileDescriptor(ssize_t *outStart, ssize_t *outLength) = 0;
    virtual ssize_t seek(ssize_t offset, int whence) = 0;
    virtual void close();

    static bool dirExists(std::string dname);
    static bool fileExists(std::string fname);
    static bool makeDir(std::string dname);

    static std::string dirName(std::string path);
    static std::string baseName(std::string path);

    static std::string getPath(std::string rsrc);
    static file_ptr getResource(std::string rsrc);
    static file_ptr getFile(std::string filename);
    static std::string getExecutablePath();
    static std::string getExecutableDirectory();
    static std::string getTempDir();
    static std::string getHomeDir();
    static std::string getTempFile(std::string filename, std::string suffix);
};

inline file::file(std::string path) :
    sbuf(""), sbufOffset(0), path(path), errmsg("unknown"), errcode(0) {}