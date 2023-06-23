#include <paf.h>

#include "compressed_file.h"
#include "print.h"
#include "zip.h"

using namespace paf;

CompressedFile::CompressedFile()
{
    uncompressedSize = 0;
    error = 0;
}

CompressedFile::~CompressedFile()
{

}

paf::common::SharedPtr<CompressedFile> CompressedFile::Create(paf::string path)
{
    auto ext = string(sce_paf_strchr(path.c_str(), '.'));
    
    if(ext == ".zip" || ext == ".vpk")
        return common::SharedPtr<CompressedFile>(new Zipfile(path));
}