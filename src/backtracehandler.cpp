#include "backtracehandler.h"


#include <bfd.h>
#include <cstring>
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fstream>
#include <iostream>
#include <link.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sstream>


backtraceHandler::backtraceHandler(bool withSourceData)
:
m_withSourceData(withSourceData)

{

}

// based on https://oroboro.com/printing-stack-traces-file-line/
json backtraceHandler::getStacktraceJSON(size_t skip_front, size_t skip_back)
{
    void* callstack[128];
    const int nMaxFrames = sizeof(callstack) / sizeof(callstack[0]);
    //char buf[1024];
    int nFrames = backtrace(callstack, nMaxFrames);
    //char** symbols = backtrace_symbols(callstack, nFrames);
    size_t additionalLines = 4;

    json outBacktrace = createBacktraceSymbols(callstack, nFrames, skip_front, skip_back);

    if (m_withSourceData)
    {
        for (auto& frame : outBacktrace)
        {
            json contextLinesInfo = getContextLines(frame["abs_path"], frame["lineno"], additionalLines);

            for (auto& line : contextLinesInfo.items())
            {
                frame[line.key()]= line.value();
            }
         }
     }
    return outBacktrace;
}

json backtraceHandler::getContextLines(const std::string& filePath, size_t lineNo, size_t deltaLines)
{
    std::string contextLne;
    std::vector<std::string> preContext;
    std::vector<std::string> postContext;

    std::ifstream file;
    std::string currentLine;
    file.open(filePath);

    if (file.is_open())
    {
        for(size_t i=1; i<=lineNo+deltaLines; i++)
        {
            if (file.eof())
                break;
            getline(file, currentLine);
            if (i >= (lineNo-deltaLines) && i < lineNo)
            {
                preContext.push_back(currentLine);
            }
            else if (i == lineNo)
            {
                contextLne = currentLine;
            }
            else if ((i>lineNo) && (i<=lineNo+deltaLines))
            {
                postContext.push_back(currentLine);
            }
            else
            {
                // do nothing
            }
        }
    }

    json output;
    output["context_line"] = contextLne;
    output["pre_context"] = preContext;
    output["post_context"] = postContext;

    return output;
}

json backtraceHandler::createBacktraceSymbols(void* const* addrList, int nFrames, size_t skip_front, size_t skip_back)
{
    size_t numberOfFramesInOutput = static_cast<size_t>(nFrames) - skip_back - skip_front;
    json output;
    // initialize the bfd library
    bfd_init();

    int total = 0;
    for (size_t i = numberOfFramesInOutput+skip_front-1; i >= skip_front;  --i )
    {
        char** location = reinterpret_cast<char **>(alloca(sizeof(char**)));

       // find which executable, or library the symbol is from
       FileMatch match( addrList[i] );
       dl_iterate_phdr( findMatchingFile, &match );

       // adjust the address in the global space of your binary to an
       // offset in the relevant library
       bfd_vma addr  = reinterpret_cast<bfd_vma>( addrList[i]);
               addr -= reinterpret_cast<bfd_vma>( match.mBase );

       // lookup the symbol
       if ( match.mFile && strlen( match.mFile ))
          location = processFile(match.mFile, &addr, 1);
       else
           location = processFile("/proc/self/exe", &addr, 1 );

       total += strlen(location[0]) + 1;
       FrameInfo frameInfo(location[0]);

       output.push_back({{"function", frameInfo.functionName},
                         {"lineno", frameInfo.lineNumber},
                         {"filename", frameInfo.fileName},
                         {"abs_path", frameInfo.absFilePath},
                         {"in_app", frameInfo.functionName[0]!='_'},
                         {"package", "package"},
                         //{"instruction_addr", match.mAddress}
                        });
       free(location);
    }

    return output;
}

int backtraceHandler::findMatchingFile(struct dl_phdr_info* info, size_t size, void* data)
{
    FileMatch* match = reinterpret_cast<FileMatch*>(data);
    for (uint32_t i=0; i < info->dlpi_phnum; i++)
    {
        const ElfW(Phdr)& phdr = info->dlpi_phdr[i];

        if (phdr.p_type == PT_LOAD)
        {
            ElfW(Addr) vaddr = phdr.p_vaddr + info->dlpi_addr;
            ElfW(Addr) maddr = ElfW(Addr)(match->mAddress);
            if ((maddr >= vaddr) && (maddr < vaddr + phdr.p_memsz))
            {
                match->mFile = info->dlpi_name;
                match->mBase = reinterpret_cast<void*>(info->dlpi_addr);
                return 1;
            }
        }
    }
    return 0;
}

asymbol** backtraceHandler::kstSlurpSymtab(bfd* abfd, const char* fileName)
{
   if ( !( bfd_get_file_flags( abfd ) & HAS_SYMS ))
   {
      printf( "Error bfd file \"%s\" flagged as having no symbols.\n", fileName );
      return nullptr;
   }

   asymbol** syms;
   unsigned int size;

   long symcount = bfd_read_minisymbols( abfd, false, reinterpret_cast<void**>(&syms), &size );
   if ( symcount == 0 )
        symcount = bfd_read_minisymbols( abfd, true,  reinterpret_cast<void**>(&syms), &size );

   if ( symcount < 0 )
   {
      printf( "Error bfd file \"%s\", found no symbols.\n", fileName );
      return nullptr;
   }

   return syms;
}

char** backtraceHandler::translateAddressesBuf(bfd* abfd, bfd_vma* addr, uint32_t numAddr, asymbol** syms)
{
    char** ret_buf = nullptr;
    uint32_t total = 0;

    char   b;
    char*  buf = &b;
    uint32_t len = 0;

   for ( size_t state = 0; state < 2; state++ )
   {
      if ( state == 1 )
      {
         ret_buf = reinterpret_cast<char**>(malloc( total + ( sizeof(char*) * numAddr )));
         buf = reinterpret_cast<char*>(ret_buf + numAddr);
         len = total;
      }

      for ( uint32_t i = 0; i < numAddr; i++ )
      {
         FileLineDesc desc( syms, addr[i] );

         if ( state == 1 )
            ret_buf[i] = buf;

         bfd_map_over_sections( abfd, FindAddressInSection, reinterpret_cast<void*>(&desc) );

         if ( !desc.mFound )
         {
            #if __WORDSIZE == 32
            total += static_cast<uint32_t>(snprintf( buf, len, "[0x%llx] \?\? \?\?:0", static_cast<uint64_t>(addr[i]) ) + 1);
            #else
             total += static_cast<uint32_t>(snprintf( buf, len, "[0x%lx] \?\? \?\?:0", static_cast<uint64_t>(addr[i]) ) + 1);
            #endif

         } else {

            int status = -1;
            const char* name = desc.mFunctionname;
            if ( name == nullptr || *name == '\0' )
               name = "??";

            char* demangled = nullptr;
            if (name[0] == '_')
            {
                 demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
            }
            if (status == 0)
            {
                name = demangled;
            }

            total += static_cast<uint32_t>(snprintf( buf, len, "%s:%u %s %lu", desc.mFilename ? desc.mFilename : "??", desc.mLine, name, static_cast<uint64_t>(addr[i])) + 1);

            free(demangled);
         }
      }

      if ( state == 1 )
      {
         buf = buf + total + 1;
      }
   }

   return ret_buf;
}

char** backtraceHandler::processFile(const char* fileName, bfd_vma* addr, uint32_t naddr)
{
    bfd* abfd = bfd_openr(fileName, nullptr);
    if (!abfd)
    {
      printf( "Error opening bfd file \"%s\"\n", fileName );
      return nullptr;
    }
    if(bfd_check_format(abfd, bfd_archive))
    {
        printf( "Cannot get addresses from archive \"%s\"\n", fileName );
        bfd_close( abfd );
        return nullptr;
    }
    char** matching;
    if (!bfd_check_format_matches(abfd, bfd_object, &matching))
    {
        printf( "Format does not match for archive \"%s\"\n", fileName );
        bfd_close( abfd );
        return nullptr;
    }
       asymbol** syms = kstSlurpSymtab( abfd, fileName );
       if ( !syms )
       {
          printf( "Failed to read symbol table for archive \"%s\"\n", fileName );
          bfd_close( abfd );
          return nullptr;
       }

       char** retBuf = translateAddressesBuf( abfd, addr, naddr, syms );

       free( syms );

       bfd_close( abfd );
       return retBuf;

}

void backtraceHandler::FileLineDesc::findAddressInSection( bfd* abfd, asection* section )
{
   if ( mFound )
      return;

   if (( bfd_get_section_flags( abfd, section ) & SEC_ALLOC ) == 0 )
      return;

   bfd_vma vma = bfd_get_section_vma( abfd, section );
   if ( mPc < vma )
      return;

   bfd_size_type size = bfd_section_size( abfd, section );
   if ( mPc >= ( vma + size ))
      return;

   mFound = bfd_find_nearest_line( abfd, section, mSyms, ( mPc - vma ),
                                   const_cast<const char**>(&mFilename), const_cast<const char**>(&mFunctionname), &mLine );
}

void backtraceHandler::FindAddressInSection( bfd* abfd, asection* section, void* data )
{
   FileLineDesc* desc = reinterpret_cast<FileLineDesc*>(data);
   //assert( desc );
   return desc->findAddressInSection( abfd, section );
}
