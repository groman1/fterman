#include "rawtui.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "settings.h"

#define REGFILECOLOR 0
#define DIRECTORYCOLOR 1
#define SYMLINKCOLOR 2

#define ALPHABETICGROUP 0
#define SIZEGROUP 1
#define ACCESSEDTIMEGROUP 2
#define MODIFIEDTIMEGROUP 3

#define redrawentries if (entries) freeFileList(entries, qtyEntries);\
				entries = getFileList(&qtyEntries);\
				drawObjects(entries, offset, qtyEntries);\
				drawEntryCount(offset, currEntry, qtyEntries);\
				if (qtyEntries) highlightEntry(entries[currEntry], currEntry-offset);

int (*sortingfunction)(const struct dirent**, const struct dirent**);
uint16_t maxx, maxy, pwdlen;
int8_t keepoldFile, sortingmethod;
char *pwd, *filter, *savedpwd, *filecppwd;

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

// Frees up space for a character at *startingIndex* by pushing *string* forward, starting at *startingIndex* and finishing at *stringLen*
void strPushfwd(char *string, int startingIndex, int stringLen) // assume string is reallocated properly
{
	for (int i = stringLen; i>startingIndex; --i)
	{
		string[i] = string[i-1];
	}
	string[stringLen+1] = 0;
}

// Functions like strcat, but the result is stored in a malloc'd return value, not *string1*
char *strccat(char *string1, const char *string2)
{
	char *result = malloc(strlen(string1)+strlen(string2)+1);
	int x, i;
	for (x = 0; string1[x]; ++x)
	{
		result[x] = string1[x];
	}
	for (i = 0; string2[i]; ++i)
	{
		result[i+x] = string2[i];
	}
	result[i+x] = 0;
	return result;
}

// Prints filename *name* at offset *offset*, leaving space for file size with length *fileSizeLen*. currIndex is only used in editfname()
void printName(char *name, int fileSizeLen, int offset, int currIndex)
{
	move(1+offset, 0);
	int i = strlen(name);
	if (i+fileSizeLen+2>=maxx)
	{
		print("..");
		i -= maxx-fileSizeLen-4;
	}
	else i = 0;
	dprintf(STDOUT_FILENO, "%s", &name[i]);
	i = strlen(name);
	if (i+fileSizeLen+2<maxx)
	{
		wrattr(NORMAL);
		moveprint(1+offset, i, " ");
		move(1+offset, currIndex+1);
	}
	else
	{
		move(1+offset, maxx-(i-currIndex)-4);
	}
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
	else colorpair = REGFILECOLOR;
	clearline();
	wrattr(NORMAL|COLORPAIR(colorpair));
	printName(entry.name, getIntLen(entry.data.st_size), offset, 0);
	if (colorpair==REGFILECOLOR) printFileSize(entry.data.st_size, offset);
	wrattr(NORMAL);
}

// Highlights the entry with text *entry->name* at offset *offset* (applies REVERSE attribute)
void highlightEntry(entry_t entry, int offset)
{
	move(offset+1, 0);
	uint8_t colorpair;
	if (S_ISDIR(entry.data.st_mode)) colorpair = DIRECTORYCOLOR;
	else if (S_ISLNK(entry.data.st_mode)) colorpair = SYMLINKCOLOR;
	else colorpair = REGFILECOLOR;
	clearline();
	wrattr(REVERSE|COLORPAIR(colorpair));
	printName(entry.name, getIntLen(entry.data.st_size), offset, 0);
	if (colorpair==REGFILECOLOR) printFileSize(entry.data.st_size, offset);
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
	filecppwd = realloc(filecppwd, pwdlen+strlen(entry)+2);
	strcpy(filecppwd, pwd);
	strcat(filecppwd, "/");
	strcat(filecppwd, entry);
}

// Loads the previously saved path by savePWD() at *savedpwd*
void loadsavedPWD()
{
	pwdlen = strlen(savedpwd);
	pwd = realloc(pwd, pwdlen);
	strcpy(pwd, savedpwd);
}

// Copies (*keepoldFile* = 1) or cuts (*keepoldFile = 0) the file from *filecppwd* to *pwd*
void copycutFile(int keepoldFile)
{
	char *command = malloc(10+keepoldFile*3+strlen(filecppwd)+strlen(pwd)); // 9 = 3 (cp or mv) + 1 ( ) + 1 (/ for second fname) + 4 for quotes
	char cp[] = "cp -r \"";
	char mv[] = "mv \"";
	if (keepoldFile) strcpy(command, cp);
	else strcpy(command, mv);
	strcat(command, filecppwd);
	strcat(command, "\" \"");
	strcat(command, pwd);
	strcat(command, "/\"");
	system(command);
	free(command);
}

// Draws the top-right status line (current entry, offset, etc)
void drawEntryCount(int offset, int currentry, int qtyEntries)
{
	--qtyEntries;
	move(0, maxx-34);
	cleartoeol();
	char entrycntstring[46];
	sprintf(entrycntstring, "%d-(%d)-%d/%d", offset, currentry, offset+maxy-3>qtyEntries?qtyEntries:offset+maxy-3, qtyEntries);
	moveprint(0, maxx-strlen(entrycntstring), entrycntstring);
}

// Draws the path *pwd* at the top-left corner
void drawPath()
{
	move(0,0);
	clearline();
	if (pwdlen<maxx) dprintf(STDOUT_FILENO, "%s ", pwd);
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
	uint64_t size = statstruct.st_size;
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
	lstat(fullpath, &entrydata);
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
	lstat(fullpath, &entrydata);
	free(fullpath);
	if (!S_ISDIR(entrydata.st_mode)) return 1;
	return 0;
}

// Gets the file list using scandir(), returning them as return value and returning the quanitity of them as *qtyEntries*
entry_t *getFileList(int *qtyEntries)
{
	char *fileName;
	entry_t *fileList;
	int currEntry = 0, currLength, offset = 0; // offset is the number of "." and ".." currently found
	struct dirent **entries;
	int n;
	n = scandir(pwd, &entries, &dirfilter, sortingfunction);

	if (n==-1) return 0;
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

	if (n==-1) return 0;
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
		else currPair = REGFILECOLOR;
		clearline();
		wrattr(COLORPAIR(currPair));
		printName(entries[i].name, strlen(entries[i].name), i-offset, 0);
		if (currPair==REGFILECOLOR) printFileSize(entries[i].data.st_size, i-offset);
		move(i-offset+2, 0);
	}
	wrattr(NORMAL);
}

// Prints access denied message
void accessdenied()
{
	moveprint(1,0,"Access denied");
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
		if (entries) 
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
	else
	{
		char *editor = getenv("EDITOR");
		if (!editor) return entries; // EDITOR environment variable has to be set in order for this to work
		char *command = "";
		int size, currSize;
		command = malloc(strlen(editor)+1);
		for (size = 0; editor[size]; ++size)
		{
			command[size] = editor[size];
		}
		command[size++] = ' ';
		command = realloc(command, size+1+pwdlen);
		for (currSize = 0; pwd[currSize]; ++currSize)
		{
			command[size+currSize] = pwd[currSize];
		}
		size += currSize;
		command = realloc(command, size+1+strlen(entries[*entryID].name));
		for (currSize = 0; entries[*entryID].name[currSize]; ++currSize)
		{
			command[size+currSize] = entries[*entryID].name[currSize];
		}
		command[size+currSize] = 0;
		deinit();
		system(command);
		free(command);
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
	char *command = malloc(8+pwdlen+strlen(file));
	char initial[] = "rm -rf ";
	for (int i = 0; initial[i]; ++i)
	{
		command[i] = initial[i];
	}
	for (int i = 0; pwd[i]; ++i)
	{
		command[7+i] = pwd[i];
	}
	for (int i = 0; file[i]; ++i)
	{
		command[7+pwdlen+i] = file[i];
	}
	command[7+pwdlen+strlen(file)] = 0;
	system(command);
	free(command);
}

// Calls the function to edit the filename on line *offset*+1
void editfname(entry_t *entry, int offset)
{
	char *oldname = malloc(strlen(entry->name)+1);
	strcpy(oldname, entry->name);
	int ch, currIndex = strlen(entry->name)-1, filenameLen = currIndex+1, currPair = REGFILECOLOR;
	
	int currFileSizeLen = getIntLen(entry->data.st_size), initialFileStrLen = strlen(oldname)+1;
	
	if (S_ISDIR(entry->data.st_mode)) currPair = DIRECTORYCOLOR;
	else if (S_ISLNK(entry->data.st_mode)) currPair = SYMLINKCOLOR;
	deHighlightEntry(*entry, offset);

	setcursor(1);
	printName(entry->name, currFileSizeLen, offset, currIndex);
	while((ch=inesc())&&ch!=10&&ch!=13)
	{
		switch(ch)
		{
			case 'a'...'z': case 'A'...'Z': case '-': case '+': case '0'...'9': case '.': case '_':
			{	if (filenameLen<256) {entry->name = realloc(entry->name, filenameLen+2); if (filenameLen!=currIndex+1) { strPushfwd(entry->name, currIndex+1, filenameLen); }  ++filenameLen; entry->name[++currIndex] = ch; if (filenameLen==currIndex+1) { entry->name[currIndex+1] = 0; } } break;	}
			case 127:
			{	if (currIndex+1) {  strPushback(entry->name, currIndex--); if (!currIndex) { currIndex = 0; } entry->name = realloc(entry->name, filenameLen--); } break;	}
			case 3:
			{	wrattr(NORMAL); entry->name = realloc(entry->name, initialFileStrLen); strcpy(entry->name, oldname); setcursor(0); free(oldname); return;	}
			case 191:
			{	if (currIndex+1) {--currIndex; } break;	}
			case 190: 
			{	if (currIndex<filenameLen-1) { ++currIndex; } break; }
			default: break;
		}
		printName(entry->name, currFileSizeLen, offset, currIndex);
	}

	char *command = malloc(10+2*pwdlen+strlen(oldname)+strlen(entry->name)); // "mv "(3 bytes) + " + pwd + old filename + " + " " (1 byte) + " + pwd + new filename + ". As pwdlen includes the null terminator, subtract 2, but add 1 for null terminator
	char mv[] = "mv \"";
	for (int i = 0; mv[i]; ++i)
	{
		command[i] = mv[i];
	}
	for (int i = 0; pwd[i]; ++i)
	{
		command[4+i] = pwd[i];
	}
	for (int i = 0; oldname[i]; ++i)
	{
		command[4+pwdlen+i] = oldname[i];
	}
	command[4+pwdlen+strlen(oldname)] = '"';
	command[5+pwdlen+strlen(oldname)] = ' ';
	command[6+pwdlen+strlen(oldname)] = '"';
	for (int i = 0; pwd[i]; ++i)
	{
		command[7+pwdlen+strlen(oldname)+i] = pwd[i];
	}
	for (int i = 0; entry->name[i]; ++i)
	{
		command[7+2*pwdlen+strlen(oldname)+i] = entry->name[i];
	}
	command[7+2*pwdlen+strlen(oldname)+strlen(entry->name)] = '"';
	command[8+2*pwdlen+strlen(oldname)+strlen(entry->name)] = 0;
	system(command);

	setcursor(0);
	free(oldname);
	free(command);
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
void search(entry_t *entries, int *qtyEntries)
{
	moveprint(maxy, 0, ":");
	uint8_t filterlen = 0, keypressed;
	setcursor(1);
	while((keypressed=inesc()))
	{
		switch (keypressed)
		{
			case 'a'...'z': case 'A'...'Z': case '-': case '+': case '0'...'9': case '.': case '_':
			{ 
				if (filterlen<255)
				{
					filter = realloc(filter, ++filterlen+1); 
					filter[filterlen-1] = keypressed;
					filter[filterlen] = 0;
					clearline();
					moveprint(maxy, 0, ":");
					moveprint(maxy, 1, filter);
				}
				break;
			}
			case 127:
			{
				if (filterlen)
				{
					filter = realloc(filter, --filterlen+1); 
					filter[filterlen] = 0;
					clearline();
					moveprint(maxy, 0, ":");
					moveprint(maxy, 1, filter);
				}
				break;
			}
			case 13: case 10:
			{ 
				setcursor(0); 
				return; 
			}
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

int main()
{
	struct option_s config = loadConfig();
	sortingmethod = config.sortingmethod;
	setSortingFunction();
	initcolorpair(DIRECTORYCOLOR, BLUE, BLACK); // directory color
	initcolorpair(SYMLINKCOLOR, CYAN, BLACK); // symlink color
	initcolorpair(3, BLACK, GREEN);
	getTermXY(&maxy, &maxx);
	--maxy; // to fit the search bar

	uint8_t keypressed;
	char *temppwd = getenv("PWD");
	pwdlen = strlen(temppwd);
	pwd = malloc(pwdlen+2);
	strcpy(pwd, temppwd);
	pwd[pwdlen++] = '/';
	pwd[pwdlen] = 0;
	savedpwd = malloc(pwdlen+1);
	strcpy(savedpwd, pwd);
	filter = malloc(1);
	filter[0] = 0;

	int qtyEntries = 0, currEntry = 0, offset = 0;
	keepoldFile = -1;
	char *backpwd = NULL;
	entry_t *entries = getFileList(&qtyEntries);
	init();
	setcursor(0);
	clear();
	drawPath();
	drawEntryCount(0, 0, qtyEntries);
	drawObjects(entries, 0, qtyEntries);
	if (qtyEntries) highlightEntry(entries[0], 0);

	while (keypressed=inesc())
	{
		if (keypressed==config.quit)
		{	break;	}
		else if (keypressed==config.goFwd)
		{	
			if (entries) 
			{
				entries = enterObject(entries, &currEntry, &qtyEntries, &offset);
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
					if (offset>qtyEntries-maxy+1) offset -= maxy-1;
					else offset = qtyEntries-maxy+1>0?qtyEntries-maxy+1:0;
					currEntry = qtyEntries-1;
				}
				clear();
				drawPath();
				drawObjects(entries, offset, qtyEntries); 
				drawEntryCount(offset, currEntry, qtyEntries);
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
				drawObjects(entries, offset, qtyEntries); 
				drawEntryCount(offset, currEntry, qtyEntries);
				highlightEntry(entries[currEntry], currEntry-offset); 
			}
		}
		else if (keypressed==config.goBack)
		{	
			if (pwdlen>1) 
			{	
				backpwd = goback(backpwd);
				clear();
				drawPath();
				drawEntryCount(offset, currEntry, qtyEntries);
				if (entries) freeFileList(entries, qtyEntries);
				entries = getFileList(&qtyEntries);
				currEntry = findentry(backpwd, entries, qtyEntries); 
				offset = currEntry; // TEMPORARY
				drawObjects(entries, offset, qtyEntries);
				drawEntryCount(offset, currEntry, qtyEntries);
				if (qtyEntries) highlightEntry(entries[currEntry], currEntry-offset);
			}	
		}
		else if (keypressed==config.deletefile)
		{	
			deleteFile(entries[currEntry].name); 
			deHighlightEntry(entries[currEntry], currEntry-offset); 
			--qtyEntries; 
			clear();
			drawPath();
			if (currEntry==qtyEntries) 
			{ 
				--currEntry; 
				if (offset) --offset; 
			} 
			redrawentries;
		}
		else if (keypressed==config.editfile)
		{
			editfname(&entries[currEntry], currEntry-offset); 
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
			sortingmethod = config.sortingmethod;
			setSortingFunction();
			currEntry = offset = 0; 
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
			savecpPWD(entries[currEntry].name);
		}
		else if (keypressed==config.paste)
		{
			if (keepoldFile!=-1)
			{
				copycutFile(keepoldFile);
				redrawentries;
			}
			if (keepoldFile==0) keepoldFile = -1;
		}
		else if (keypressed==config.search)
		{
			currEntry = 0;
			offset = 0;
			search(entries, &qtyEntries);
			clear();
			drawPath();
			redrawentries;
		}
		else if (keypressed==config.cancelsearch)
		{
			currEntry = 0;
			offset = 0;
			filter = realloc(filter, 1);
			filter[0] = 0;
			clear();
			drawPath();
			redrawentries;
		}
		getTermXY(&maxy, &maxx);
	}

	freeFileList(entries, qtyEntries);
	free(savedpwd);
	free(pwd);
	free(backpwd);
	free(filter);
	free(filecppwd);
	freeConfig();
	setcursor(1);
	deinit();
	return 0;
}
