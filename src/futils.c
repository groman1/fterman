#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "direnum.h"

uint8_t copyFile(char *src, char *dest, char *fname)
{
	int srcLen = strlen(src), destLen = strlen(dest);
	src[srcLen] = '/';
	dest[destLen] = '/';
	strcpy(src+srcLen+1, fname);
	strcpy(dest+destLen+1, fname);

	int srcFd = open(src, O_RDONLY, 0);

	int destFd = open(dest, O_CREAT|O_WRONLY, 0644);
	src[srcLen] = 0;
	dest[destLen] = 0;
	if (destFd<0) return 1;

	char buf[4096];
	uint16_t lengthRead;
	while ((lengthRead = read(srcFd, buf, 4096)))
		write(destFd, buf, lengthRead);

	close(srcFd);
	close(destFd);
	return 0;
}

uint8_t moveFile(char *src, char *dest, char *fname)
{
	int srcLen = strlen(src), destLen = strlen(dest);
	src[srcLen] = '/';
	dest[destLen] = '/';
	strcpy(src+srcLen+1, fname);
	strcpy(dest+destLen+1, fname);

	if (!link(src, dest))
	{
		unlink(src);
		return 0;
	}
	if (copyFile(src, dest, "")) return 1;
	unlink(src);

	src[srcLen] = 0;
	dest[destLen] = 0;
	
	return 0;
}

uint8_t createDirs(char *path, uint16_t start)
{
	while (path[start])
	{
		path[start] = 0;
		if (mkdir(path, 0755)&&errno!=EEXIST) return 1;
		path[start] = '/';
		for (++start;path[start]&&path[start]!='/'; ++start);
	}
	if (mkdir(path, 0755)&&errno!=EEXIST) return 1;
	return 0;
}

uint8_t rmDirs(char *path, uint16_t end)
{
	uint16_t len = strlen(path), start = len;

	while (start<end)
	{
		path[start] = 0;
		if (rmdir(path)) return 1;
		path[start] = '/';
		while (path[--start]!='/'&&start<end);
	}
	path[len] = 0;
	return 0;
}

uint8_t removeEntry(char *path)
{
	uint16_t pathLen = strlen(path), tmpLen;
	struct stat entryData;
	lstat(path, &entryData);
	
	if (S_ISREG(entryData.st_mode)||S_ISLNK(entryData.st_mode))
	{
		unlink(path);
		return 0;
	}
	else if (S_ISDIR(entryData.st_mode))
	{
		openDir(path);
		struct dirent *file;
		while ((file = getNextEntry(1)))
		{
			tmpLen = strlen(path);
			strcat(path, "/");
			strcat(path, file->d_name);
			if (file->d_type==DT_REG)
			{
				if (unlink(path))
					goto errorremove;
			}
			else if (file->d_type==DT_DIR)
			{
				if (rmdir(path))
					goto errorremove;
			}
			path[tmpLen] = 0;
		}
		path[pathLen] = 0;
		if (rmdir(path))
			goto errorremove;
		closeDir();
	}
	return 0;
errorremove:
	path[pathLen] = 0;
	closeDir();
	return 1;
}

uint8_t copymove(char *src, char *dest, char *fname, uint8_t move)
{
	struct stat entryData;
	lstat(src, &entryData);
	char destPath[PATH_MAX+1];
	strcpy(destPath, dest);
	strcat(destPath, "/");
	strcat(destPath, fname);
	uint16_t srcLen = strlen(src), destLen = strlen(dest), destPathLen = strlen(destPath);

	if (S_ISREG(entryData.st_mode))
	{
		if (move)
		{
			if (moveFile(src, destPath, "")) return 1;
				return 1;
		}
		else
		{
			if (copyFile(src, destPath, "")) return 1;
				return 1;
		}
	}
	else if (S_ISLNK(entryData.st_mode))
	{
		char *path = realpath(src, NULL);
		if (symlink(path, destPath)) return 1;
		if (move)
			if (unlink(path))
				return 1;
		free(path);
	}
	else if (S_ISDIR(entryData.st_mode))
	{
		openDir(src);
		struct dirent *file;
		if (createDirs(destPath, destLen))
			goto errordir;
		while ((file = getNextEntry(0)))
		{
			strcat(destPath, getSrcSuffix());
			if (createDirs(destPath, destLen))
				goto errordir;

			if (move)
				if (rmDirs(src, srcLen))
					goto errordir;

			if (!move)
			{
				if (copyFile(src, destPath, file->d_name)&&errno!=EEXIST)
					goto errordir;
			}
			else
				if (moveFile(src, destPath, file->d_name)&&errno!=EEXIST)
					goto errordir;
			destPath[destPathLen] = 0;
		}
		src[srcLen] = 0;
		closeDir();
		if (move)
			if (rmdir(src))
				return 1;
	}
	else return 1;
	return 0;

errordir:
	src[srcLen] = 0;
	closeDir();
	return 1;
}
