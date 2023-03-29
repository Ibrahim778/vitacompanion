#include "zip.h"
#include "print.h"

#include <minizip/unzip.h>
#include <paf.h>
#include <kernel.h>

#define dir_delimter '/'
#define MAX_FILENAME 512
#define READ_SIZE 2048

paf::string dirnameOf(const paf::string& fname) {
    for(int i = fname.length() - 1; i > 0; i--)
    {
        if(fname.c_str()[i] == '\\' || fname.c_str()[i] == '/')
        {
            return paf::string(fname.data(), i);
        }
    }
    return paf::string("");
}

Zipfile::Zipfile(const paf::string zip_path) {
	
	zipfile_ = unzOpen(zip_path.c_str());
	if (!zipfile_)
    {
		print("Cannot open zip");
        return;
    }

	if (unzGetGlobalInfo(zipfile_, &global_info_) != UNZ_OK)
	{
        print("Cannot read zip info");
        return;
    }
}

Zipfile::~Zipfile() {
	if (zipfile_)
		unzClose(zipfile_);
}

int Zipfile::Unzip(const paf::string outpath, void (*progcb)(SceUInt, SceUInt)) {
	
	if (uncompressed_size_ == 0)
		UncompressedSize();

	char read_buffer[READ_SIZE];

	uLong i;
	if (unzGoToFirstFile(zipfile_) != UNZ_OK)
	{
        print("Error going to first file");
        return -1;
    }
	for (i = 0; i < global_info_.number_entry; ++i) {
		
		unz_file_info file_info;
		char filename[MAX_FILENAME];
		char fullfilepath[MAX_FILENAME];
		if (unzGetCurrentFileInfo(zipfile_, &file_info, filename, MAX_FILENAME, nullptr, 0, nullptr, 0) != UNZ_OK)
		{
            print("Error reading zip file info");
            return -2;
        }
        print("Got current file\n");
        sce_paf_snprintf(fullfilepath, sizeof(fullfilepath), "%s%s", outpath.c_str(), filename);
        print("sprintf done! %s\n", fullfilepath);
		// Check if this entry is a directory or file.
		const size_t filename_length = sce_paf_strlen(fullfilepath);
		if (fullfilepath[filename_length - 1] == dir_delimter) {
			paf::Dir::CreateRecursive(fullfilepath);
		} else {
			// Create the dir where the file will be placed
			paf::string destdir = dirnameOf(paf::string(fullfilepath));
			paf::Dir::CreateRecursive(destdir.c_str());
			// Entry is a file, so extract it.
			if (unzOpenCurrentFile(zipfile_) != UNZ_OK)
			{
                print("Cannot open file from zip");
                return -3;
            }
			// Open a file to write out the data.
			FILE* out = sce_paf_fopen(fullfilepath, "wb");
			if (out == nullptr) {
				unzCloseCurrentFile(zipfile_);
				print("Cannot open destination file");
                return -4;
            }

			int error;
			do {
				error = unzReadCurrentFile(zipfile_, read_buffer, READ_SIZE);
				if (error < 0) {
					unzCloseCurrentFile(zipfile_);
					print("Cannot read current zip file");
                    return -5;
                }

				if (error > 0)
					sce_paf_fwrite(read_buffer, error, 1, out); // TODO check fwrite return
				
			} while (error > 0);

			sce_paf_fclose(out);
		}

		unzCloseCurrentFile(zipfile_);

		// Go the the next entry listed in the zip file.
		if ((i + 1) < global_info_.number_entry)
			if (unzGoToNextFile(zipfile_) != UNZ_OK)
			{
                print("Error getting next zip file");
                return -6;
            }
        
        if(progcb)
            progcb(i, global_info_.number_entry);
        
	}

	return 0;
}

int Zipfile::UncompressedSize() {
	uncompressed_size_ = 0;

	if (unzGoToFirstFile(zipfile_) != UNZ_OK)
	{
        print("Error going to first file");
        return -1;
    }

	uLong i;
	for (i = 0; i < global_info_.number_entry; ++i) {
		
		unz_file_info file_info;
		char filename[MAX_FILENAME];
		if (unzGetCurrentFileInfo(zipfile_, &file_info, filename, MAX_FILENAME, nullptr, 0, nullptr, 0) != UNZ_OK)
		{
            print("Error reading zip file info");
            return -2;
        }

		uncompressed_size_ += file_info.uncompressed_size;

		// Go the the next entry listed in the zip file.
		if ((i + 1) < global_info_.number_entry)
			if (unzGoToNextFile(zipfile_) != UNZ_OK)
			{
                print("Error calculating zip size");
                return -3;
            }
    }

	return 0;
}
