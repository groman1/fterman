#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

static DIR *srcTempDir;
static uint16_t nestedQty;
static char *srcParts[PATH_MAX/4];
static char *src;

uint8_t openDir(char *srcPath)
{
	src = srcPath;
	srcTempDir = opendir(srcPath);
	uint16_t startSrc = strlen(srcPath);
//	src[startSrc] = '/';
//	src[startSrc+1] = 0;
	srcParts[0] = src+startSrc+1;
	nestedQty = 0;
	return srcTempDir==0;
}

void closeDir()
{
	closedir(srcTempDir);
}

char *getSrcSuffix()
{
	return srcParts[0]-1;
}

struct dirent *getNextEntry(uint8_t type)
{
	struct dirent *file;

regen:

	file = readdir(srcTempDir);

	if (!file && !nestedQty) return 0;

	if (!file) // reached the end of the current directory
	{
		*(srcParts[nestedQty]-1) = 0;
		closedir(srcTempDir);
		srcTempDir = opendir(src);
		while ((file = readdir(srcTempDir))!=NULL)
		{
			if (!strcmp(file->d_name, ".")||!strcmp(file->d_name, "..")||file->d_type!=DT_DIR) continue;
			if (!strcmp(file->d_name, srcParts[nestedQty])) break;
		}
		--nestedQty;
		if (type) return file;
		goto regen;
	}
	if ((file->d_type==DT_DIR)&&(strcmp(file->d_name, "."))&&(strcmp(file->d_name, ".."))) // found a folder
	{
		++nestedQty;
		strcat(src, "/");
		srcParts[nestedQty] = src+strlen(src);
		strcat(src, file->d_name);		
		closedir(srcTempDir);
		srcTempDir = opendir(src);
		goto regen;
	}
		
	if (file->d_type!=DT_REG) goto regen; // the if statement above should deal with directories
	
	return file;
}
