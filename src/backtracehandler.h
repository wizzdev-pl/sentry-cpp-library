#ifndef SENTRY_BACKTRACEHANDLER_H
#define SENTRY_BACKTRACEHANDLER_H

#include "json.h"

#include <bfd.h>
#include <link.h>
#include <stdlib.h>


using json = nlohmann::json;

class backtraceHandler
{
public:
    backtraceHandler(bool withSourceData);
    json getStacktraceJSON(size_t skip_front=0, size_t skip_back=0);

private:
    static int findMatchingFile(struct dl_phdr_info* info, size_t size, void* data);
    json createBacktraceSymbols(void* const* addrList, int nFrames, size_t skip_front=0, size_t skip_back=0);
    json getContextLines(const std::string& filePath, size_t lineNo, size_t deltaLines);
    static asymbol** kstSlurpSymtab(bfd* abfd, const char* fileName);
    static char** translateAddressesBuf(bfd* abfd, bfd_vma* addr, uint32_t numAddr, asymbol** syms);
    static char** processFile(const char* fileName, bfd_vma* addr, uint32_t naddr);
    static void FindAddressInSection( bfd* abfd, asection* section, void* data );

    bool m_withSourceData;

struct FrameInfo
{
    std::string absFilePath;
    std::string fileName;
    uint64_t lineNumber;
    std::string functionName;
    std::string address;

    FrameInfo(char* inputLine)
        :
          absFilePath(),
          fileName(),
          lineNumber(),
          functionName(),
          address()
    {
        std::string line(inputLine);
        std::size_t filePathEnd = line.find(':');
        absFilePath = line.substr(0, filePathEnd);

        // TODO filename: The path to the source file relative to the project root directory.
        size_t fileNameStart = absFilePath.rfind('/');
        fileName = absFilePath.substr(fileNameStart+1);

        std::size_t linenumberEnd = line.find(' ');
        std::string lineNumberStr = line.substr(filePathEnd+1, linenumberEnd-(filePathEnd+1));
        lineNumber = std::stoul(lineNumberStr, nullptr, 0);

        std::size_t nameEnd = line.rfind(' ');

        functionName = line.substr(linenumberEnd+1, nameEnd-(linenumberEnd+1));

        address = line.substr(nameEnd+1);
        uint64_t addrInt = std::stoul(address, nullptr, 0);

        std::stringstream ss;
        ss << std::hex << addrInt;
        address = std::string(ss.str());
    }

};

 class FileMatch
    {
    public:
        FileMatch(void* addr)
        :
        mAddress(addr),
        mFile(),
        mBase()
        {}
        void* mAddress;
        const char* mFile;
        void* mBase;
    };

 class FileLineDesc
 {
 public:
    FileLineDesc( asymbol** syms, bfd_vma pc ) : mPc( pc ), mFound( false ), mSyms( syms ) {}

    void findAddressInSection( bfd* abfd, asection* section );

    bfd_vma      mPc;
    char*        mFilename;
    char*        mFunctionname;
    unsigned int mLine;
    int          mFound;
    asymbol**    mSyms;
 };};

#endif // SENTRY_BACKTRACEHANDLER_H

