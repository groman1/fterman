#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "rawtui.h"
#include "settings.h"

#define pwd pwdarr[currentWindow]
#define filter filterarr[currentWindow]
#define pwdlen pwdlenarr[currentWindow]

#define REGFILECOLOR 1
#define EXECCOLOR 2
#define DIRECTORYCOLOR 3
#define SYMLINKCOLOR 4
#define BROKENSYMLINKCOLOR 5

#define ALPHABETICGROUP 0
#define SIZEGROUP 1
#define ACCESSEDTIMEGROUP 2
#define MODIFIEDTIMEGROUP 3

#define UNUSED -1

#define regenerateentries\
				if (qtyEntries) freeFileList(entries, qtyEntries);\
				entries = getFileList(&qtyEntries);\

#define redrawentries\
				drawObjects(entries, offset, qtyEntries);\
				drawEntryCount(offset, currEntry, qtyEntries);\
				if (qtyEntries) highlightEntry(entries[currEntry], currEntry-offset);\
				else accessdenied();\
				if (filter[0])\
				{\
					moveprint(maxy, 0, ":");\
					moveprint(maxy, 1, filter);\
				}\
				char workspacestring[2] = "W";\
				workspacestring[1] = currentWindow+49;\
				moveprintsize(maxy, maxx-2, workspacestring, 2);

int (*sortingfunction)(const struct dirent**, const struct dirent**);
uint16_t maxx, maxy, pwdlenarr[4];
int8_t keepoldFile, sortingmethod, showsize, searchtype, currentWindow;
char *pwdarr[4], *filterarr[4], *savedpwd, *filecppwd;

typedef struct
{
	char *name;
	struct stat data;
} entry_t;

// Returns the amount of characters required to store *input* in a string
unsigned char getIntLen(long long input)
{
	unsigned char currFileSizeLen = 1;
	unsigned long long multiplier = 10;
	while (input/multiplier)
	{
		++currFileSizeLen;
		multiplier *= 10;
	}
	return currFileSizeLen;
}

// Pushes back *string* starting at *startingIndex* by one byte, clearing letter at *startingIndex*
void strPushback(char *string, int startingIndex)
{
	for (; string[startingIndex+1]; ++startingIndex)
	{
		string[startingIndex] = string[startingIndex+1];
	}
	string[startingIndex] = 0;
}

// Pushes back *entries* starting at *startingIndex*, deleting the entry with index *startingIndex* and shortening the *entries* by 1
entry_t *entriesPushback(entry_t *entries, int startingIndex, int qtyEntries)
{
	free(entries[startingIndex].name);
	for (; startingIndex<qtyEntries-1; ++startingIndex)
	{
		entries[startingIndex] = entries[startingIndex+1];
	}
	--qtyEntries;
	entries = realloc(entries, sizeof(entry_t)*qtyEntries);
	return entries;
}

// Frees up space for a character at *startingIndex* by pushing *string* forward, starting at *startingIndex* and finishing at *stringLen*
void strPushfwd(char *string, int startingIndex, int stringLen) // assume string is reallocated properly
{
	for (int i = stringLen; i>startingIndex; --i)
	{
		string[i] = string[i-1];
	}
	string[stringLen+1] = 0;
}

// Prints filename *name* at offset *offset*, leaving space for file size with length *fileSizeLen*. currIndex is only used in editfname()
void printName(char *name, int fileSizeLen, int offset, int currIndex, uint8_t isasymlink, uint8_t prefixlen)
{
	move(1+offset, prefixlen);
	int i = strlen(name);
	char *linkpath;
	if (!showsize) fileSizeLen = 0;
	if (isasymlink)
	{
		char *fullpath = strccat(pwd, name);
		linkpath = realpath(fullpath, NULL);
		free(fullpath);
		if (linkpath==NULL)
		{
			wrcolorpair(BROKENSYMLINKCOLOR);
			isasymlink = 0;
		}
	}

	if (i+fileSizeLen+1>=maxx)
	{
		print("..");
		i -= maxx-fileSizeLen-4;
	}
	else i = 0;

	print(&name[i]);
	i = strlen(name);

	if (i+fileSizeLen+1<maxx)
	{
		if (isasymlink)
		{
			if (i+fileSizeLen+4+strlen(linkpath)+prefixlen<maxx)
			{
				moveprint(1+offset, i+prefixlen, " => ");
				moveprint(1+offset, i+prefixlen+4, linkpath);
			}
		}
		wrattr(NORMAL);
		move(1+offset, currIndex+prefixlen);
	}
	else
	{
		move(1+offset, maxx-(i-currIndex)-fileSizeLen-1);
		wrattr(NORMAL);
	}

	if (isasymlink) free(linkpath);
}

// Prints file size *size* at offset *offset*
void printFileSize(long long size, int offset)
{
	move(1+offset, maxx-getIntLen(size));
	dprintf(STDOUT_FILENO, "%lld", size);
}

// Reverts the effects of highlightEntry() (applies NORMAL attribute)
void deHighlightEntry(entry_t entry, int offset)
{
	move(offset+1, 0);
	uint8_t colorpair;
	if (S_ISDIR(entry.data.st_mode)) colorpair = DIRECTORYCOLOR;
	else if (S_ISLNK(entry.data.st_mode)) colorpair = SYMLINKCOLOR;
	else if (S_IXUSR&entry.data.st_mode) colorpair = EXECCOLOR;
	else colorpair = REGFILECOLOR;
	clearline();
	wrcolorpair(colorpair);
	printName(entry.name, getIntLen(entry.data.st_size), offset, UNUSED, colorpair==SYMLINKCOLOR, 0);
	if (colorpair<=EXECCOLOR&&showsize) printFileSize(entry.data.st_size, offset);
	wrattr(NORMAL);
}

// Highlights the entry with text *entry->name* at offset *offset* (applies REVERSE attribute)
void highlightEntry(entry_t entry, int offset)
{
	move(offset+1, 0);
	uint8_t colorpair;
	if (S_ISDIR(entry.data.st_mode)) colorpair = DIRECTORYCOLOR;
	else if (S_ISLNK(entry.data.st_mode)) colorpair = SYMLINKCOLOR;
	else if (S_IXUSR&entry.data.st_mode) colorpair = EXECCOLOR;
	else colorpair = REGFILECOLOR;
	clearline();
	wrattr(REVERSE);
	wrcolorpair(colorpair);
	printName(entry.name, getIntLen(entry.data.st_size), offset, UNUSED, colorpair==SYMLINKCOLOR, 0);
	if (colorpair<=EXECCOLOR&&showsize) printFileSize(entry.data.st_size, offset);
	wrattr(NORMAL);
}

// Saves the current path to *savedpwd*
void savePWD()
{
	free(savedpwd);
	savedpwd = malloc(pwdlen+1);
	strcpy(savedpwd, pwd);
}

// Saves the path of an entry to copy/cut to *filecppwd*
void savecpPWD(char *entry)
{
	free(filecppwd);
	filecppwd = strccat(pwd, entry);
}

// Loads the previously saved path by savePWD() at *savedpwd*
void loadsavedPWD()
{
	pwdlen = strlen(savedpwd);
	pwd = realloc(pwd, pwdlen);
	strcpy(pwd, savedpwd);
}

// Copies (*keepoldFile* = 1) or cuts (*keepoldFile* = 0) the file from *filecppwd* to *pwd*
void copycutFile(int keepoldFile)
{
	pid_t pid = fork();

	if (keepoldFile&&pid==0) exit(execlp("cp", "cp", "-r", filecppwd, pwd, (char*)0));
	else if (!keepoldFile&&pid==0) exit(execlp("mv", "mv", filecppwd, pwd, (char*)0));

	while(wait(0)!=-1);
}

// Draws the top-right status line (current entry, offset, etc)
void drawEntryCount(int offset, int currentry, int qtyEntries)
{
	move(0, maxx-34);
	cleartoeol();
	char entrycntstring[46];
	sprintf(entrycntstring, "%d-(%d)-%d/%d", offset+1, currentry+1, offset+maxy-3>qtyEntries?qtyEntries:offset+maxy-3, qtyEntries);
	moveprint(0, maxx-strlen(entrycntstring), entrycntstring);
}

// Draws the path *pwd* at the top-left corner
void drawPath()
{
	move(0,0);
	clearline();
	if (pwdlen<maxx) print(pwd);
	else 
	{
		printsize(pwd, maxx-2);
	}
}

// Converts *ch* into a lowercase letter if it isn't already lowercase
char toLower(char ch)
{
	if (ch>='A'&&ch<='Z') return ch+32;
	return ch;
}

// The next four functions use sortingmethod's lowest byte to determine whether to invert the result or not.

// Sorts entries alphabetically, used as compar() by scandir()
int alphabeticsort(const struct dirent **dirent1, const struct dirent **dirent2)
{
	uint8_t len1 = strlen((*dirent1)->d_name), len2 = strlen((*dirent2)->d_name);
	for (int i = 0; i<(len1>len2?len2:len1); ++i)
	{
		if (toLower((*dirent1)->d_name[i])!=toLower((*dirent2)->d_name[i]))
		{
			return toLower((*dirent1)->d_name[i])<toLower((*dirent2)->d_name[i])?1*(-1+(sortingmethod&1)*2):-1*(-1+(sortingmethod&1)*2);
		}
	}
	for (int i = 0; i<(len1>len2?len2:len1); ++i)
	{
		if ((*dirent1)->d_name[i]!=(*dirent2)->d_name[i])
		{
			return (*dirent1)->d_name[i]<(*dirent2)->d_name[i]?1*(-1+(sortingmethod&1)*2):-1*(-1+(sortingmethod&1)*2);
		}
	}
	return len1>len2?1:-1;
}

// Sorts entries by size, used as compar() by scandir()
int sizesort(const struct dirent **dirent1, const struct dirent **dirent2)
{
	struct stat statstruct;
	char *fullpath = strccat(pwd, (*dirent1)->d_name);
	lstat(fullpath, &statstruct);
	uint32_t size = statstruct.st_size;
	free(fullpath);
	fullpath = strccat(pwd, (*dirent2)->d_name);
	lstat(fullpath, &statstruct);
	free(fullpath);
	return size>statstruct.st_size?1-(sortingmethod&1)*2:-1+(sortingmethod&1)*2;
}

// Sorts entries by time last accessed, used as compar() by scandir()
int lastaccessedsort(const struct dirent **dirent1, const struct dirent **dirent2)
{
	struct stat statstruct;
	char *fullpath = strccat(pwd, (*dirent1)->d_name);
	lstat(fullpath, &statstruct);
	uint32_t accessedtime = statstruct.st_atime;
	free(fullpath);
	fullpath = strccat(pwd, (*dirent2)->d_name);
	lstat(fullpath, &statstruct);
	free(fullpath);
	return accessedtime>statstruct.st_atime?1-(sortingmethod&1)*2:-1+(sortingmethod&1)*2;
}

// Sorts entries by time last modified, used as compar() by scandir()
int lastmodifiedsort(const struct dirent **dirent1, const struct dirent **dirent2)
{
	struct stat statstruct;
	char *fullpath = strccat(pwd, (*dirent1)->d_name);
	lstat(fullpath, &statstruct);
	uint32_t modifiedtime = statstruct.st_mtime;
	free(fullpath);
	fullpath = strccat(pwd, (*dirent2)->d_name);
	lstat(fullpath, &statstruct);
	free(fullpath);
	return modifiedtime>statstruct.st_mtime?1-(sortingmethod&1)*2:-1+(sortingmethod&1)*2;
}

// Returns 1 if the entry *entry* is a directory, used as filter() by scandir()
int dirfilter(const struct dirent *entry)
{
	if (entry->d_name[0]=='.'&&(entry->d_name[1]==0||(entry->d_name[1]=='.'&&entry->d_name[2]==0))) return 0;

	if (!strcasestr(entry->d_name, filter)) return 0;
	char *fullpath;
	fullpath = strccat(pwd, entry->d_name);
	struct stat entrydata;
	stat(fullpath, &entrydata);
	free(fullpath);
	if (S_ISDIR(entrydata.st_mode)) return 1;
	return 0;
}

// Returns 1 if the entry *entry* is not a directory, used as filter() by scandir()
int filefilter(const struct dirent *entry)
{
	if (!strcasestr(entry->d_name, filter)) return 0;
	char *fullpath;
	fullpath = strccat(pwd, entry->d_name);
	struct stat entrydata;
	stat(fullpath, &entrydata);
	free(fullpath);
	if (!S_ISDIR(entrydata.st_mode)) return 1;
	return 0;
}

// Gets the file list using scandir(), returning them as return value and returning the quanitity of them as *qtyEntries*
entry_t *getFileList(int *qtyEntries)
{
	char *fileName;
	entry_t *fileList;
	int currLength, offset = 0; // offset is the number of "." and ".." currently found
	struct dirent **entries;
	int n;
	n = scandir(pwd, &entries, &dirfilter, sortingfunction);

	if (n==-1) 
	{ *qtyEntries = 0; return 0; }
	fileList = malloc(sizeof(entry_t)*n);
	for (int i = 0; i<n; ++i)
	{
		fileName = strccat(pwd, entries[i]->d_name);
		lstat(fileName, &fileList[i-offset].data);
		free(fileName);
		currLength = strlen(entries[i]->d_name);
		fileList[i-offset].name = malloc(currLength+1);
		for (int x = 0; x<=currLength; fileList[i-offset].name[x] = entries[i]->d_name[x], ++x);
		free(entries[i]);
	}
	free(entries);
	*qtyEntries = n;
	n = scandir(pwd, &entries, &filefilter, sortingfunction);

	if (n==-1) 
	{ *qtyEntries = 0; return 0; }
	fileList = realloc(fileList, (*qtyEntries+n)*sizeof(entry_t));
	offset = 0;

	for (int i = 0; i<n; ++i)
	{
		fileName = strccat(pwd, entries[i]->d_name);
		lstat(fileName, &fileList[i-offset+*qtyEntries].data);
		free(fileName);
		currLength = strlen(entries[i]->d_name);
		fileList[i-offset+*qtyEntries].name = malloc(currLength+1);
		for (int x = 0; x<=currLength; fileList[i-offset+*qtyEntries].name[x] = entries[i]->d_name[x], ++x);
		free(entries[i]);
	}
	*qtyEntries += n;
	free(entries);
	return fileList;
}

// Frees the file list allocated by getFileList() function
void freeFileList(entry_t *fileList, int qtyEntries)
{
	for (int i = 0; i<qtyEntries; ++i)
	{
		free(fileList[i].name);
		fileList[i].name = 0;
	}
	free(fileList);
}

// Draws entries starting from entries[offset] until entries[offset+maxy] or entries[qtyEntries] (whichever is lower)
void drawObjects(entry_t *entries, int offset, int qtyEntries)
{
	move(1,0);
	int currPair;
	for (int i = offset; i<qtyEntries&&i-offset<maxy-2; ++i)
	{
		if (S_ISDIR(entries[i].data.st_mode)) currPair = DIRECTORYCOLOR;
		else if (S_ISLNK(entries[i].data.st_mode)) currPair = SYMLINKCOLOR;
		else if (S_IXUSR&entries[i].data.st_mode) currPair = EXECCOLOR;
		else currPair = REGFILECOLOR;
		clearline();
		wrcolorpair(currPair);
		printName(entries[i].name, getIntLen(entries[i].data.st_size), i-offset, UNUSED, currPair==SYMLINKCOLOR, 0);
		if (currPair<=EXECCOLOR&&showsize) printFileSize(entries[i].data.st_size, i-offset);
		move(i-offset+2, 0);
	}
	wrattr(NORMAL);
}

// Prints access denied message
void accessdenied()
{
	moveprint(1,0,"No files found");
}

// Removes the last directory from *pwd* and copies the removed string to *backpath*
char *goback(char *backpath)
{
	filter = realloc(filter, 1);
	filter[0] = 0;
	int i;
	for (i = pwdlen-2; pwd[i]!='/'; --i);
	backpath = realloc(backpath, pwdlen-++i);
	for (int x = 0; x<pwdlen-i-1; ++x)
	{
		backpath[x] = pwd[i+x];
		pwd[i+x] = 0;
	}
	backpath[pwdlen-i-1] = 0;
	pwdlen = i;
	pwd = realloc(pwd, pwdlen+1);
	return backpath;
}

// Checks if the entry ( *entries[*entryID]* ) is a file, link or directory. If it is a link, determines whether the link is pointing to a file or a directory. In both cases, if entry is a directory, opens it and gets the file list there. If entry is a file, opens it in a file editor determined by environment variable EDITOR
entry_t *enterObject(entry_t *entries, int *entryID, int *qtyEntries, int *offset)
{
	filter = realloc(filter, 1);
	filter[0] = 0;
	struct stat tempstat;
	if (S_ISLNK(entries[*entryID].data.st_mode))
	{
		char *fullpwd;
		fullpwd = strccat(pwd, entries[*entryID].name);
		stat(fullpwd, &tempstat);
		free(fullpwd);
	}
	else
	{
		tempstat = entries[*entryID].data;
	}
	if (!*qtyEntries) return entries;
	if (S_ISDIR(tempstat.st_mode))
	{
		pwd = realloc(pwd, pwdlen+strlen(entries[*entryID].name)+2);
		for (int i = 0; entries[*entryID].name[i]; ++i)
		{
			pwd[pwdlen++] = entries[*entryID].name[i];
		}
		pwd[pwdlen++] = '/';
		pwd[pwdlen] = 0;
		freeFileList(entries, *qtyEntries);
		entries = getFileList(qtyEntries);	
		if (*qtyEntries) 
		{
			clear();
			drawPath();
			drawObjects(entries, 0, *qtyEntries);
			drawEntryCount(0, 0, *qtyEntries);
			if (*qtyEntries) highlightEntry(entries[0], 0);
		}
		else
		{
			clear();
			drawPath();
			accessdenied();
		}
		*entryID = 0;
		*offset = 0;
	}
	else if (tempstat.st_mode&S_IXUSR)
	{
		char *fullpath = strccat(pwd, entries[*entryID].name);
		deinit();
		pid_t pid = fork();

		if (pid==0)
		{
			exit(execl(fullpath, fullpath, (char*)0));
		}
		free(fullpath);
		while(wait(0)!=-1);
		init();
		setcursor(0);
		clear();
		drawPath();
		drawEntryCount(*offset, *entryID, *qtyEntries);
		drawObjects(entries, *offset, *qtyEntries);
		highlightEntry(entries[*entryID], *entryID-*offset);
	}
	else
	{
		char *editor = getenv("EDITOR");
		if (!editor) return entries; // EDITOR environment variable has to be set in order for this to work
		deinit();
		char *args = strccat(pwd, entries[*entryID].name);
		deinit();
		pid_t pid = fork();

		if (pid==0) exit(execlp(editor, editor, args, (char*)0));
		free(args);
		while(wait(0)!=-1);
		init();
		setcursor(0);
		clear();
		drawPath();
		drawEntryCount(*offset, *entryID, *qtyEntries);
		drawObjects(entries, *offset, *qtyEntries);
		highlightEntry(entries[*entryID], *entryID-*offset);
	}
	return entries;
}

// Deletes a file at *pwd* + *file*
void deleteFile(char *file)
{
	char *fullpath = strccat(pwd, file);
	pid_t pid = fork();
	if (pid==0)
	{
		exit(execlp("rm", "rm", "-rf", fullpath, (char*)0));
	}
	while(wait(0)!=-1);
	free(fullpath);
}

// Provides inline text editing with inline right/left movement for functions *editfname* and *createEntry*
char *inlineedit(uint16_t offset, char *initialtext, uint16_t prefixlen, uint8_t colorpair)
{
	char *text;
	uint8_t length, currIndex, ch;
	if (initialtext)
	{
		text = malloc(strlen(initialtext)+1);
		strcpy(text, initialtext);
		length = strlen(initialtext);
		currIndex = length;
	}
	else
	{
		text = malloc(1);
		length = 0;
		currIndex = 0;
	}

	setcursor(1);
	move(offset+1, prefixlen);
	cleartoeol();
	char workspacestring[2] = "W";
	workspacestring[1] = currentWindow+49;

	saveCursorPos();
	moveprintsize(maxy, maxx-2, workspacestring, 2);
	loadCursorPos();
	wrcolorpair(colorpair);
	if (initialtext) printName(initialtext, 0, offset, currIndex, 0, prefixlen);

	while((ch=inesc())&&ch!=10&&ch!=13)
	{
		switch(ch)
		{
			case 'a'...'z': case 'A'...'Z': case '-': case '+': case '0'...'9': case '.': case '_':
			{
				if (length<255)
				{
					text = realloc(text, length+2);
					if (length!=currIndex) strPushfwd(text, currIndex, length);
					++length;
					text[currIndex++] = ch;
					if (length==currIndex) text[currIndex] = 0;
				}
				break;
			}
			case 127:
			{
				if (currIndex)
				{
					strPushback(text, --currIndex);
					text = realloc(text, length--);
				}
				break;
			}
			case 183:
			{
				if (currIndex!=length)
				{
					strPushback(text, currIndex);
					text = realloc(text, length--);
				}
				break;
			}
			case 3:
			{
				setcursor(0);
				free(text);
				return 0;
			}
			case 191:
			{
				if (currIndex) --currIndex;
				break;
			}
			case 190: 
			{
				if (currIndex<length) ++currIndex;
				break;
			}
			default: break;
		}

		move(offset+1, prefixlen);
		cleartoeol();

		wrattr(NORMAL);
		saveCursorPos();
		moveprintsize(maxy, maxx-2, workspacestring, 2);
		loadCursorPos();

		wrcolorpair(colorpair);
		printName(text, 0, offset, currIndex, 0, prefixlen);
	}
	
	setcursor(0);
	return text;
}

// Calls the function to edit the filename on line *offset*+1
void editfname(entry_t *entry, int offset)
{
	char *oldname = malloc(strlen(entry->name)+1);
	strcpy(oldname, entry->name);

	uint8_t colorpair;
	if (S_ISDIR(entry->data.st_mode)) colorpair = DIRECTORYCOLOR;
	else if (S_ISLNK(entry->data.st_mode)) colorpair = SYMLINKCOLOR;
	else if (S_IXUSR&entry->data.st_mode) colorpair = EXECCOLOR;
	else colorpair = REGFILECOLOR;

	free(entry->name);
	entry->name = inlineedit(offset, oldname, 0, colorpair);
	if (!entry->name)
	{
		entry->name = malloc(strlen(oldname)+1);
		strcpy(entry->name, oldname);
		free(oldname);
		return;
	}

	char *oldpath = strccat(pwd, oldname);
	char *newpath = strccat(pwd, entry->name);

	pid_t pid = fork();
	if (pid==0) exit(execlp("mv", "mv", oldpath, newpath, (char*)0));

	while(wait(0)!=-1);

	free(oldname);
	free(oldpath);
	free(newpath);
}

// Attempts to find entry with name *entryname* in *entries* with length *qtyEntries*
int findentry(char *entryname, entry_t *entries, int qtyEntries)
{
	for (int i = 0; i<qtyEntries; ++i)
	{
		if (!strcmp(entries[i].name, entryname)) return i;
	}
	return -1;
}

// Opens search menu, sets *filter* to the phrase entered and regenerates file list
void search(int qtyEntries)
{
	entry_t *entries = getFileList(&qtyEntries);
	uint8_t filterlen = strlen(filter), keypressed;
	if (searchtype) drawEntryCount(0, 0, qtyEntries);
	char workspacestring[2] = "W";
	workspacestring[1] = currentWindow+49;
	moveprintsize(maxy, maxx-2, workspacestring, 2);
	moveprint(maxy, 0, ":");
	moveprint(maxy, 1, filter);
	qtyEntries = 0;
	setcursor(1);
	while((keypressed=inesc()))
	{
		switch (keypressed)
		{
			case 'a'...'z': case 'A'...'Z': case '-': case '+': case '0'...'9': case '.': case '_':
			{ 
				if (filterlen<maxx-2)
				{
					filter = realloc(filter, ++filterlen+1); 
					filter[filterlen-1] = keypressed;
					filter[filterlen] = 0;
					move(maxy, 1);
					cleartoeol();
				}
				break;
			}
			case 127:
			{
				if (filterlen)
				{
					filter = realloc(filter, --filterlen+1); 
					filter[filterlen] = 0;
					move(maxy, 0);
					clearline();
					moveprint(maxy, 0, ":");
					moveprint(maxy, 1, filter);
				}
				break;
			}
			case 13: case 10:
			{
				if (qtyEntries&&searchtype) freeFileList(entries, qtyEntries);
				setcursor(0); 
				return;
			}
		}

		if (searchtype)
		{
			if (qtyEntries) freeFileList(entries, qtyEntries);
			entries = getFileList(&qtyEntries);
			drawEntryCount(0, 0, qtyEntries);
			setcursor(0);
			move(1,0);
			cleartobot();
			drawObjects(entries, 0, qtyEntries);
			moveprint(maxy, 0, ":");
			moveprint(maxy, 1, filter);
			
			saveCursorPos();
			moveprintsize(maxy, maxx-2, workspacestring, 2);
			loadCursorPos();
			setcursor(1);
		}
	}
}

// Sets sorting function depending on *sortingmethod*'s value
void setSortingFunction()
{
	if (sortingmethod>>1==ALPHABETICGROUP) sortingfunction = &alphabeticsort;
	else if (sortingmethod>>1==SIZEGROUP) sortingfunction = &sizesort;
	else if (sortingmethod>>1==MODIFIEDTIMEGROUP) sortingfunction = &lastmodifiedsort;
	else if (sortingmethod>>1==ACCESSEDTIMEGROUP) sortingfunction = &lastaccessedsort;
}

uint8_t createEntry(entry_t *entries, uint32_t qtyEntries, uint8_t isdir)
{
	char *fname;

	if (isdir)
	{
		moveprint(maxy, 0, "D: ");
		fname = inlineedit(maxy-1, NULL, 2, DIRECTORYCOLOR);
	}
	else	
	{
		moveprint(maxy, 0, "F: ");
		fname = inlineedit(maxy-1, NULL, 2, REGFILECOLOR);
	}

	if (!fname||fname[0]==0)
	{
		return 1;
	}

	move(maxy, 0);
	clearline();

	for (uint32_t i = 0; i<qtyEntries; ++i)
	{
		if (!strcmp(entries[i].name, fname))
		{
			moveprint(0, 0, "The file/directory with this name already exists");
			in();
			return 1;
		}
	}

	pid_t pid = fork();
	char *fullpath = strccat(pwd, fname);
	if (pid==0)
	{
		if (isdir)
			exit(execlp("mkdir", "mkdir", fullpath, (char*)0));
		else
			exit(execlp("touch", "touch", fullpath, (char*)0));
	}

	while(wait(0)==-1);
	return 0;
}

// These macros will break everything if placed at the top
#define currEntry currEntryarr[currentWindow]
#define offset offsetarr[currentWindow]
#define backpwd backpwdarr[currentWindow]
#define qtyEntries qtyEntriesarr[currentWindow]
#define entries entriesarr[currentWindow]


int main()
{
	struct option_s config = loadConfig();

	currentWindow = 0;
	sortingmethod = config.sortingmethod;
	showsize = config.showsize;
	searchtype = config.searchtype;
	setSortingFunction();

	initcolorpair(7, BLACK, GREEN);
	getTermXY(&maxy, &maxx);

	if (maxx<40||maxy<25)
	{
		printf("The terminal window is too small\n");
		return 1;
	}
	--maxy; // to fit the search bar

	uint8_t keypressed, windowsInitialised = 1, windowstatus = 0, cutfromwindow = 0;
	char *temppwd = getenv("PWD");
	pwdlen = strlen(temppwd);
	pwd = malloc(pwdlen+2);
	strcpy(pwd, temppwd);
	pwd[pwdlen++] = '/';
	pwd[pwdlen] = 0;
	savedpwd = malloc(pwdlen+1);
	strcpy(savedpwd, pwd);

	int qtyEntriesarr[4], currEntryarr[4], offsetarr[4];
	char *backpwdarr[4];
	for (int i = 0; i<4; ++i)
	{
		filterarr[i] = malloc(1);
		filterarr[i][0] = 0;
		backpwdarr[i] = NULL;
		currEntryarr[i] = 0;
		offsetarr[i] = 0;
		qtyEntriesarr[i] = 0;
	}

	keepoldFile = -1;
	entry_t *entriesarr[4];
	entries = getFileList(&qtyEntries);
	init();
	setcursor(0);
	clear();
	drawPath();
	drawEntryCount(0, 0, qtyEntries);
	drawObjects(entries, 0, qtyEntries);
	if (qtyEntries) highlightEntry(entries[0], 0);

	char workspacestring[2] = "W";
	workspacestring[1] = currentWindow+49;
	moveprintsize(maxy, maxx-2, workspacestring, 2);

	while ((keypressed=inesc()))
	{
		if (keypressed==config.quit)
		{	break;	}
		else if (keypressed==config.goFwd)
		{	
			if (qtyEntries) 
			{
				entries = enterObject(entries, &currEntry, &qtyEntries, &offset);
				moveprintsize(maxy, maxx-2, workspacestring, 2);
			}	
		}
		else if (keypressed==config.goDown)
		{	
			if (currEntry<qtyEntries-1) 
			{ 
				deHighlightEntry(entries[currEntry], currEntry-offset); 
				++currEntry; 
				if (currEntry-offset>maxy-4&&currEntry<qtyEntries-1)
				{
					++offset;
					drawObjects(entries, offset, qtyEntries);
				}
				drawEntryCount(offset, currEntry, qtyEntries);
				highlightEntry(entries[currEntry], currEntry-offset); 
			}	
		}
		else if (keypressed==config.goUp)
		{	
			if (currEntry>0) 
			{ 
				deHighlightEntry(entries[currEntry], currEntry-offset); 
				--currEntry; 
				if (currEntry-offset-1<0&&currEntry>0)
				{	
					--offset; 
					drawObjects(entries, offset, qtyEntries); 
				}
				drawEntryCount(offset, currEntry, qtyEntries);
				highlightEntry(entries[currEntry], currEntry-offset); 
			}	
		}
		else if (keypressed==config.goDownLong)
		{
			if (currEntry<qtyEntries-1) 
			{ 
				deHighlightEntry(entries[currEntry], currEntry-offset); 
				offset += maxy-1;
				currEntry += maxy-1;
				if (currEntry>qtyEntries-1)
				{
					if (offset>qtyEntries-1) offset -= maxy-1;
					else offset = qtyEntries-maxy+1>0?qtyEntries-maxy+2:0;
					currEntry = qtyEntries-1;
				}
				clear();
				drawPath();
				drawObjects(entries, offset, qtyEntries); 
				drawEntryCount(offset, currEntry, qtyEntries);
				moveprintsize(maxy, maxx-2, workspacestring, 2);
				highlightEntry(entries[currEntry], currEntry-offset); 
			}
		}
		else if (keypressed==config.goUpLong)
		{
			if (currEntry>0) 
			{ 
				deHighlightEntry(entries[currEntry], currEntry-offset); 
				offset -= maxy-1; 
				currEntry -= maxy-1;
				if (offset<0)
				{
					offset = 0;
					currEntry = 0;
				}
				clear();
				drawPath();
				redrawentries;
			}
		}
		else if (keypressed==config.goBack)
		{	
			if (pwdlen>1)
			{
				backpwd = goback(backpwd);
				clear();
				drawPath();
				regenerateentries;
				currEntry = findentry(backpwd, entries, qtyEntries);
				offset = currEntry?currEntry-1:currEntry;
				redrawentries;
			}	
		}
		else if (keypressed==config.deletefile)
		{	
			deleteFile(entries[currEntry].name); 
			deHighlightEntry(entries[currEntry], currEntry-offset); 
			entries = entriesPushback(entries, currEntry, qtyEntries);
			clear();
			drawPath();
			if (currEntry==qtyEntries-1)
			{ 
				--currEntry; 
				if (offset) --offset; 
			}
			--qtyEntries;
			if (qtyEntries) 
			{
				redrawentries;
			}
			else 
			{
				accessdenied();
			}
		}
		else if (keypressed==config.editfile)
		{
			editfname(&entries[currEntry], currEntry-offset); 
			regenerateentries;
			redrawentries;
		}
		else if (keypressed==config.savedir) savePWD();
		else if (keypressed==config.loaddir)
		{	
			loadsavedPWD(); 
			currEntry = offset = 0; 
			redrawentries;
		}
		else if (keypressed==15)
		{	
			config = drawSettings(); 
			if (sortingmethod!=config.sortingmethod) 
			{
				currEntry = offset = 0; 
				sortingmethod = config.sortingmethod;
				setSortingFunction();
				regenerateentries;
			}
			showsize = config.showsize;
			searchtype = config.searchtype;
			clear();
			drawPath();
			redrawentries;
		}
		else if (keypressed==config.copy)
		{
			keepoldFile = 1;
			savecpPWD(entries[currEntry].name);
		}
		else if (keypressed==config.cut)
		{
			keepoldFile = 0;
			cutfromwindow = currentWindow;
			savecpPWD(entries[currEntry].name);
		}
		else if (keypressed==config.paste)
		{
			if (keepoldFile!=-1)
			{
				copycutFile(keepoldFile);
				regenerateentries;
				redrawentries;
			}
			if (keepoldFile==0) 
			{
				keepoldFile = -1;
				windowstatus |= 1<<cutfromwindow;
			}
		}
		else if (keypressed==config.search)
		{
			currEntry = 0;
			offset = 0;
			search(qtyEntries);
			if (searchtype)
			{
				if (qtyEntries) freeFileList(entries, qtyEntries);
				entries = getFileList(&qtyEntries);
				if (qtyEntries) highlightEntry(entries[0], 0);
			}
			else
			{
				move(1,0);
				cleartobot();
				regenerateentries;
				redrawentries;
			}
		}
		else if (keypressed==config.cancelsearch)
		{
			currEntry = 0;
			offset = 0;
			filter = realloc(filter, 1);
			filter[0] = 0;
			clear();
			drawPath();
			regenerateentries;
			redrawentries;
		}
		else if (keypressed==config.createdir)
		{
			createEntry(entries, qtyEntries, 1);
			clear();
			drawPath();
			windowstatus = 15-(1<<currentWindow);
			regenerateentries;
			redrawentries;
		}
		else if (keypressed==config.createfile)
		{
			createEntry(entries, qtyEntries, 0);
			clear();
			drawPath();
			windowstatus = 15-(1<<currentWindow);
			regenerateentries;
			redrawentries;
		}
		else if (keypressed>='1'&&keypressed<='4')
		{
			uint8_t lastwindow = currentWindow;
			currentWindow = keypressed-49;
			if (!((windowsInitialised>>currentWindow)&1))
			{
				pwd = malloc(strlen(pwdarr[lastwindow])+1);
				strcpy(pwd, pwdarr[lastwindow]);
				pwdlen = pwdlenarr[lastwindow];
				windowsInitialised|=1<<currentWindow;
				regenerateentries;
			}
			workspacestring[1] = currentWindow+49;
			clear();
			drawPath();
			if (windowstatus>>currentWindow&1) 
			{ regenerateentries; windowstatus ^= 1<<currentWindow; }
			redrawentries;
		}
		getTermXY(&maxy, &maxx);
	}

	for (int i = 0; i<4; ++i)
	{
		if ((windowsInitialised>>i)&1)
		{
			free(pwdarr[i]);
			if (backpwdarr[i]) free(backpwdarr[i]);
			freeFileList(entriesarr[i], qtyEntriesarr[i]);
		}
		free(filterarr[i]);
	}
	free(savedpwd);
	free(filecppwd);
	freeConfig();
	setcursor(1);
	deinit();
	return 0;
}
