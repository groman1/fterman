#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "rawtui.h"
#include "settings.h"
#include "futils.h"

#define pwd pwdarr[currentWindow]
#define filter filterarr[currentWindow]
#define pwdlen pwdlenarr[currentWindow]
#define pathIds pathIdsarr[currentWindow]
#define pathDepth pathDeptharr[currentWindow]

#define REGFILECOLOR 1
#define EXECCOLOR 2
#define DIRECTORYCOLOR 3
#define SYMLINKCOLOR 4
#define BROKENSYMLINKCOLOR 5

#define ALPHABETICGROUP 0
#define SIZEGROUP 1
#define ACCESSEDTIMEGROUP 2
#define MODIFIEDTIMEGROUP 3

#define UNUSED 0

#define WINDOWQTY 4 // custom quantities are not implemented, using more than 4 will result in more memory used

#define MINX 30
#define MINY 5

#define regenerateentries\
				if (qtyEntries) freeFileList(entries, qtyEntries);\
				entries = getFileList(&qtyEntries);\

#define redrawentries\
				drawObjects(entries, offset, qtyEntries);\
				drawEntryCount(offset, currEntry, qtyEntries);\
				if (qtyEntries) highlightEntry(entries[currEntry], currEntry-offset);\
				else accessdenied();\
				drawBottomLine();

int (*sortingfunction)(const struct dirent**, const struct dirent**);
uint16_t maxx, maxy, pwdlenarr[WINDOWQTY];
int8_t keepoldFile, sortingmethod, showsize, searchtype, currentWindow;
char pwdarr[WINDOWQTY][PATH_MAX+1], filterarr[WINDOWQTY][256], savedpwd[PATH_MAX+1], filecppwd[PATH_MAX+1];
char *pathIdsarr[WINDOWQTY][PATH_MAX/4];
uint16_t pathDeptharr[WINDOWQTY];
char workspacestring[2] = "W";

typedef struct
{
	char *name;
	struct stat data;
} entry_t;

// Returns the amount of characters required to store *input* in a string
uint8_t getIntLen(long long input)
{
	uint8_t currFileSizeLen = 1;
	uint64_t multiplier = 10;
	while (input/multiplier)
	{
		++currFileSizeLen;
		multiplier *= 10;
	}
	return currFileSizeLen;
}

// Converts *ch* into a lowercase letter if it isn't already lowercase
char toLower(char ch)
{
	if (ch>='A'&&ch<='Z') return ch+32;
	return ch;
}

// Pushes back *string* starting at *startingIndex* by one byte, clearing letter at *startingIndex*
void strPushback(char *string, int startingIndex)
{
	for (; string[startingIndex+1]; ++startingIndex)
		string[startingIndex] = string[startingIndex+1];

	string[startingIndex] = 0;
}

// Pushes back *entries* starting at *startingIndex*, deleting the entry with index *startingIndex* and shortening the *entries* by 1
entry_t *entriesPushback(entry_t *entries, int startingIndex, int qtyEntries)
{
	free(entries[startingIndex].name);
	for (; startingIndex<qtyEntries-1; ++startingIndex)
		entries[startingIndex] = entries[startingIndex+1];

	--qtyEntries;
	entries = realloc(entries, sizeof(entry_t)*qtyEntries);
	return entries;
}

// Makes space for an entry at *index* by pushing forward *entries* starting at *index*
entry_t *entriesPushForward(entry_t *entries, int index, int qtyEntries)
{
	++qtyEntries;
	entries = realloc(entries, sizeof(entry_t)*qtyEntries);
	for (int i = qtyEntries-2; i>=index; --i)
		entries[i+1] = entries[i];

	return entries;
}

// Combine the *pwd* and *fname* to get a path to a file
void constructPath(const char *fname, char *ret)
{
	strcpy(ret, pwd);
	ret[pwdlen] = '/';
	ret[pwdlen+1] = 0;
	strcat(ret, fname);
}

// Frees up space for a character at *startingIndex* by pushing *string* forward, starting at *startingIndex* and finishing at *stringLen*
void strPushfwd(char *string, uint16_t startingIndex, uint16_t stringLen) // assume string is reallocated properly
{
	for (int i = stringLen; i>startingIndex; --i)
		string[i] = string[i-1];

	string[stringLen+1] = 0;
}

// Fills some data used by the goback/enterObject dirs
void fillPwdData()
{
	pathDepth = 0;
	for (uint16_t i = 0; pwd[i]; ++i)
	{
		if (pwd[i]=='/')
		{
			// TODO check if this ever triggers
			if (!pwd[i+1])
			{
				--pwdlen;
				pwd[i] = 0;
				break;
			}
			pathIds[pathDepth] = pwd+i+1;
			++pathDepth;
		}
	}
}

// Compares two strings but never returns 0
int8_t strccmp(const char *string1, const char *string2)
{
	uint8_t len1 = strlen(string1), len2 = strlen(string2);

	for (int i = 0; i<(len1>len2?len2:len1); ++i)
		if (toLower(string1[i])!=toLower(string2[i]))
			return toLower(string1[i])<toLower(string2[i])?1*(-1+(sortingmethod&1)*2):-1*(-1+(sortingmethod&1)*2);

	for (int i = 0; i<(len1>len2?len2:len1); ++i)
		if (string1[i]!=string2[i])
			return string1[i]<string2[i]?1*(-1+(sortingmethod&1)*2):-1*(-1+(sortingmethod&1)*2);

	return 2*(len1>len2)-1;
}

// Print the (not always) overflowing search text from search() function. Returns the offset at which the cursor has been placed
uint8_t overflowPrint(char *text, uint8_t len, uint8_t maxlen, uint16_t offset, uint8_t xoffset, uint8_t currIndex)
{
	move(offset+1, xoffset);
	uint8_t hasTrailing = 0, hasLeading = 0;

	if (currIndex>=maxlen-2)
	{
		// trailing
		hasTrailing = 1;
		print("..");
		maxlen -= 2;
		len -= maxlen;
		currIndex -= maxlen;
	}
	// whether to draw leading .. or not
	if (currIndex/(maxlen-2)<len/(maxlen-2))
	{
		maxlen -= 2;
		hasLeading = 1;
	}

	printsize(text+((currIndex/maxlen+hasTrailing)*maxlen), maxlen);

	// leading
	if (hasLeading)
		print("..");

	move(offset+1, xoffset+(currIndex+(maxlen+2)*hasTrailing)%maxlen);

	return (currIndex+(maxlen+2)*hasTrailing)%maxlen;
}

// Prints filename *name* at offset *offset*, leaving space for file size with length *fileSizeLen*. currIndex is only used in editfname()
void printName(char *name, uint8_t fileSizeLen, uint16_t offset, uint16_t currIndex, uint8_t isasymlink, uint8_t prefixlen)
{
	move(1+offset, prefixlen);
	uint8_t namelen = strlen(name);
	if (currIndex==UNUSED) currIndex = namelen;
	char *linkpath = 0;
	if (!showsize) fileSizeLen = 0;
	if (isasymlink)
	{
		char fullpath[PATH_MAX];
		constructPath(name, fullpath);
		linkpath = realpath(fullpath, NULL);
		if (linkpath==NULL)
		{
			wrcolorpair(BROKENSYMLINKCOLOR);
			isasymlink = 0;
		}
	}

	uint8_t xoff = overflowPrint(name, namelen, maxx-fileSizeLen-(fileSizeLen!=0), offset, prefixlen, currIndex);

	if (isasymlink)
	{
		if (xoff+fileSizeLen+(fileSizeLen!=0)+4+strlen(linkpath)+prefixlen<maxx)
		{
			print(" => ");
			print(linkpath);
		}
		free(linkpath);
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
	strcpy(savedpwd, pwd);
}

uint16_t entryoffset;

// Saves the path of an entry to copy/cut to *filecppwd*
void savecpPWD(char *entry)
{
	strcpy(filecppwd, pwd);
	entryoffset = pwdlen+1;
	filecppwd[pwdlen] = '/';
	filecppwd[pwdlen+1] = 0;
	strcat(filecppwd, entry);
}

// Loads the previously saved path by savePWD() at *savedpwd*
void loadsavedPWD()
{
	strcpy(pwd, savedpwd);
}

// Copies (*keepoldFile* = 1) or cuts (*keepoldFile* = 0) the file from *filecppwd* to *pwd*
void copycutFile()
{
	if (copymove(filecppwd, pwd, filecppwd+entryoffset, !keepoldFile))// TODO handle errors
		raise(SIGTRAP); // temp for debugging
}

// Draws the top-right status line (current entry, offset, etc)
void drawEntryCount(int offset, int currentry, int qtyEntries)
{
	char entrycntstring[46];
	sprintf(entrycntstring, "%d-(%d)-%d/%d", offset+1, currentry+1, offset+maxy-3>qtyEntries?qtyEntries:offset+maxy-3, qtyEntries);
	move(0, maxx-strlen(entrycntstring)-2);
	cleartoeol();
	moveprint(0, maxx-strlen(entrycntstring), entrycntstring);
}

// Draws the path *pwd* at the top-left corner
void drawPath()
{
	move(0,0);
	clearline();
	if (pwdlen==1) printc('/');
	if (pwdlen-1<maxx) print(pwd+1);
	else printsize(pwd+1, maxx-2);
}

// Draws *text* in the centre of the screen
void drawCentered(char *text)
{
	moveprint(maxy/2, maxx/2-strlen(text)/2, text);
}

// Draws the bottom bar (search and workspace string)
void drawBottomLine()
{
	move(maxy, 0);
	clearline();
	if (*filter)
	{
		print("/");
		overflowPrint(filter, strlen(filter), maxx-4, maxy-1, 1, strlen(filter));
	}
	moveprint(maxy, maxx-2, workspacestring);
}

// The next four functions use sortingmethod's lowest byte to determine whether to invert the result or not.

// Sorts entries alphabetically, used as compar() by scandir()
int alphabeticsort(const struct dirent **dirent1, const struct dirent **dirent2)
{
	return strccmp((*dirent1)->d_name, (*dirent2)->d_name);
}

// Sorts entries by size, used as compar() by scandir()
int sizesort(const struct dirent **dirent1, const struct dirent **dirent2)
{
	struct stat statstruct;
	char fullpath[PATH_MAX+1];
	constructPath((*dirent1)->d_name, fullpath);
	lstat(fullpath, &statstruct);
	uint32_t size = statstruct.st_size;
	constructPath((*dirent2)->d_name, fullpath);
	lstat(fullpath, &statstruct);
	return size>statstruct.st_size?1-(sortingmethod&1)*2:-1+(sortingmethod&1)*2;
}

// Sorts entries by time last accessed, used as compar() by scandir()
int lastaccessedsort(const struct dirent **dirent1, const struct dirent **dirent2)
{
	struct stat statstruct;
	char fullpath[PATH_MAX+1];
	constructPath((*dirent1)->d_name, fullpath);
	lstat(fullpath, &statstruct);
	uint32_t accessedtime = statstruct.st_atime;
	constructPath((*dirent2)->d_name, fullpath);
	lstat(fullpath, &statstruct);
	return accessedtime>statstruct.st_atime?1-(sortingmethod&1)*2:-1+(sortingmethod&1)*2;
}

// Sorts entries by time last modified, used as compar() by scandir()
int lastmodifiedsort(const struct dirent **dirent1, const struct dirent **dirent2)
{
	struct stat statstruct;
	char fullpath[PATH_MAX+1];
	constructPath((*dirent1)->d_name, fullpath);
	lstat(fullpath, &statstruct);
	uint32_t modifiedtime = statstruct.st_mtime;
	constructPath((*dirent2)->d_name, fullpath);
	lstat(fullpath, &statstruct);
	return modifiedtime>statstruct.st_mtime?1-(sortingmethod&1)*2:-1+(sortingmethod&1)*2;
}

// Returns 1 if the entry *entry* is a directory, used as filter() by scandir()
int dirfilter(const struct dirent *entry)
{
	if (entry->d_name[0]=='.'&&(entry->d_name[1]==0||(entry->d_name[1]=='.'&&entry->d_name[2]==0))) return 0;

	if (!strcasestr(entry->d_name, filter)) return 0;
	char fullpath[PATH_MAX+1];
	constructPath(entry->d_name, fullpath);
	struct stat entrydata;
	stat(fullpath, &entrydata);
	if (S_ISDIR(entrydata.st_mode)) return 1;
	return 0;
}

// Returns 1 if the entry *entry* is not a directory, used as filter() by scandir()
int filefilter(const struct dirent *entry)
{
	if (!strcasestr(entry->d_name, filter)) return 0;
	char fullpath[PATH_MAX+1];
	constructPath(entry->d_name, fullpath);
	struct stat entrydata;
	stat(fullpath, &entrydata);
	if (!S_ISDIR(entrydata.st_mode)) return 1;
	return 0;
}

// Gets the file list using scandir(), returning them as return value and returning the quanitity of them as *qtyEntries*
entry_t *getFileList(int *qtyEntries)
{
	char fileName[PATH_MAX+1];
	entry_t *fileList = 0;
	int currLength, offset = 0; // offset is the number of "." and ".." currently found
	struct dirent **entries;
	int n;
	n = scandir(pwd, &entries, &dirfilter, sortingfunction);
	if (n==0) goto filescan;

	if (n==-1)
	{ *qtyEntries = 0; return 0; }
	fileList = malloc(sizeof(entry_t)*n);
	for (int i = 0; i<n; ++i)
	{
		constructPath(entries[i]->d_name, fileName);
		lstat(fileName, &fileList[i-offset].data);
		currLength = strlen(entries[i]->d_name);
		fileList[i-offset].name = malloc(currLength+1);
		for (int x = 0; x<=currLength; fileList[i-offset].name[x] = entries[i]->d_name[x], ++x);
		free(entries[i]);
	}
	free(entries);

filescan:
	*qtyEntries = n;
	n = scandir(pwd, &entries, &filefilter, sortingfunction);

	if (*qtyEntries+n<=0)
	{ *qtyEntries = 0; return 0; }
	fileList = realloc(fileList, (*qtyEntries+n)*sizeof(entry_t));
	offset = 0;

	for (int i = 0; i<n; ++i)
	{
		constructPath(entries[i]->d_name, fileName);
		lstat(fileName, &fileList[i-offset+*qtyEntries].data);
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
	for (int i = offset; i<qtyEntries&&i-offset<maxy-1; ++i)
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

// Prints access denied message (changed into no files found)
void accessdenied()
{
	moveprint(1, 0, "No files found");
}

uint8_t invalidate = 1;

// Removes the last directory from *pwd* and copies the removed string to *backpath*
char *goback()
{
	filter[0] = 0;
	invalidate = 0;
	--pathDepth;
	pwdlen -= strlen(pathIds[pathDepth])+1;
	*(pathIds[pathDepth]-1) = 0;
	if (!pwd[1])
	{
		*pwd = '/';
		*(pwd+1) = 0;
	}
	return pathIds[pathDepth];
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

// Checks if the entry ( *entries[*entryID]* ) is a file, link or directory. If it is a link, determines whether the link is pointing to a file or a directory. In both cases, if entry is a directory, opens it and gets the file list there. If entry is a file, opens it in a file editor determined by environment variable EDITOR
entry_t *enterObject(entry_t *entries, int *entryID, int *qtyEntries, int *offset)
{
	struct stat tempstat;
	if (S_ISLNK(entries[*entryID].data.st_mode))
	{
		char fullpwd[PATH_MAX];
		constructPath(entries[*entryID].name, fullpwd);
		stat(fullpwd, &tempstat);
	}
	else
		tempstat = entries[*entryID].data;

	if (S_ISDIR(tempstat.st_mode))
	{
		*filter = 0;
		pwd[pwdlen] = '/';

		int newQtyEntries;
		entry_t *newentries = 0;

		if (pathIds[pathDepth]&&!strcmp(pathIds[pathDepth], entries[*entryID].name)&&!invalidate)
		{
			pwdlen += strlen(entries[*entryID].name)+1;
			newentries = getFileList(&newQtyEntries);
			if (pathIds[pathDepth+1])
			{
				*entryID = findentry(pathIds[pathDepth+1], newentries, newQtyEntries);
				if (*entryID==-1) ++*entryID;
				// TESTING
				*offset = *entryID?((*entryID-*entryID%(maxy-1)>newQtyEntries-(maxy-1)&&*entryID>maxy)?newQtyEntries-(maxy-1):*entryID-1):*entryID;
			}
			else
			{
				invalidate = 1;
				*entryID = 0;
				*offset = 0;
			}
		}
		else
		{
			pathIds[pathDepth] = pwd+pwdlen+1;
			pathIds[pathDepth+1] = 0;
			strcpy(pathIds[pathDepth], entries[*entryID].name);
			pwdlen += strlen(entries[*entryID].name)+1;
			invalidate = 1;
			*entryID = 0;
			*offset = 0;
			newentries = getFileList(&newQtyEntries);
		}
		++pathDepth;

		freeFileList(entries, *qtyEntries);
		entries = newentries;
		*qtyEntries = newQtyEntries;

		if (*qtyEntries)
		{
			clear();
			drawPath();
			drawObjects(entries, *offset, *qtyEntries);
			drawEntryCount(*offset, *entryID, *qtyEntries);
			if (*qtyEntries) highlightEntry(entries[*entryID], *entryID-*offset);
		}
		else
		{
			clear();
			drawPath();
			accessdenied();
		}
	}
	else if (tempstat.st_mode&S_IXUSR)
	{
		char fullpath[PATH_MAX+1];
		constructPath(entries[*entryID].name, fullpath);
		deinit();
		pid_t pid = fork();

		if (pid==0)
		{
			exit(execl(fullpath, fullpath, (char*)0));
		}
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
		char args[PATH_MAX+1];
		constructPath(entries[*entryID].name, args);
		deinit();
		pid_t pid = fork();

		if (pid==0) exit(execlp(editor, editor, args, (char*)0));
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
	char fullpath[PATH_MAX+1];
	constructPath(file, fullpath);
	if (removeEntry(fullpath)) // TODO error handling
		raise(SIGTRAP);
}

// Provides inline text editing with inline right/left movement for functions *editfname* and *createEntry*
// TODO implement an argument for returning static array
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
		text[0] = 0;
		length = 0;
		currIndex = 0;
	}

	setcursor(1);
	move(offset+1, prefixlen);
	cleartoeol();
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
	
	wrattr(NORMAL);
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

	char oldpath[PATH_MAX+1], newpath[PATH_MAX+1];
	constructPath(oldname, oldpath);
	constructPath(entry->name, newpath);

	rename(oldpath, newpath);

	free(oldname);
}

// Opens search menu, sets *filter* to the phrase entered
void search()
{
	int qtyEntries = 0;
	entry_t *entries = 0;
	uint8_t filterlen = strlen(filter), keypressed, currIndex;
	currIndex = filterlen;

	moveprintsize(maxy, maxx-2, workspacestring, 2);
	moveprint(maxy, 0, "/");
	overflowPrint(filter, filterlen, maxx-4, maxy-1, 1, currIndex);
	setcursor(1);
	while((keypressed = inesc()))
	{
		switch (keypressed)
		{
			case 'a'...'z': case 'A'...'Z': case '-': case '+': case '0'...'9': case '.': case '_':
			{ 
				if (filterlen<255)
				{
					if (filterlen!=currIndex) strPushfwd(filter, currIndex, filterlen);
					++filterlen;
					filter[currIndex++] = keypressed;
					filter[filterlen] = 0;
					if (currIndex==filterlen) filter[currIndex] = 0;
					move(maxy, 1);
					clearline();
					moveprint(maxy, 0, "/");
					moveprintsize(maxy, maxx-2, workspacestring, 2);
					overflowPrint(filter, filterlen, maxx-4, maxy-1, 1, currIndex);
				}
				break;
			}
			case 127:
			{
				if (currIndex)
				{
					strPushback(filter, --currIndex);
					move(maxy, 0);
					clearline();
					moveprint(maxy, 0, "/");
					moveprintsize(maxy, maxx-2, workspacestring, 2);
					overflowPrint(filter, filterlen, maxx-4, maxy-1, 1, currIndex);
				}
				break;
			}
			case 13: case 10:
			{
				if (qtyEntries&&searchtype) freeFileList(entries, qtyEntries);
				setcursor(0); 
				return;
			}
			case 3:
			{
				if (qtyEntries&&searchtype) freeFileList(entries, qtyEntries);
				*filter = 0;
				setcursor(0);
				return;
			}
			case 191:
			{
				if (currIndex>0) --currIndex;
				break;
			}
			case 190:
			{
				if (currIndex<filterlen) ++currIndex;
				break;
			}
		}

		if (searchtype)
		{
			regenerateentries;
			drawEntryCount(0, 0, qtyEntries);
			setcursor(0);
			move(1,0);
			cleartobot();
			if (!qtyEntries) accessdenied();
			drawObjects(entries, 0, qtyEntries);
			moveprintsize(maxy, maxx-2, workspacestring, 2);
			moveprint(maxy, 0, "/");
			overflowPrint(filter, filterlen, maxx-4, maxy-1, 1, currIndex);
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

// Creates an entry, directory or file, depending on *isdir*, and returns the entries array
entry_t *createEntry(entry_t *entries, uint32_t qtyEntries, uint8_t isdir, uint32_t *result)
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

	wrcolorpair(1);
	move(maxy, 0);
	clearline();

	if (!fname||fname[0]==0)
	{
		*result = (uint32_t)-1;
		return 0;
	}

	for (uint32_t i = 0; i<qtyEntries; ++i)
	{
		if (!strcmp(entries[i].name, fname))
		{
			moveprint(maxy, 0, "The file/directory with this name already exists");
			in();
			*result = i;
			return 0;
		}
	}

	char fullpath[PATH_MAX+1];
	constructPath(fname, fullpath);
	if (isdir) // TODO? error checking
		mkdir(fullpath, 0755);
	else
		close(creat(fullpath, 0644));

	uint32_t index = 0;

	if (sortingmethod>>1==ALPHABETICGROUP)
	{
		if (!S_ISREG(entries[0].data.st_mode))
		{
			for (; index<qtyEntries; ++index)
			{
				if (strccmp(fname, entries[index].name)==-1+2*(sortingmethod&1)&&(S_ISDIR(entries[index].data.st_mode)-!isdir)) break;
			}
		}
	}
	else
	{
		switch (sortingmethod>>1)
		{
			case ACCESSEDTIMEGROUP:
			case MODIFIEDTIMEGROUP:
			{
				if (!isdir&&(sortingmethod&1)==0)
				{
					index = qtyEntries;
					break;
				}
				else if (isdir&&(sortingmethod&1))
				{
					index = 0;
					break;
				}
				for (; index<qtyEntries; ++index)
				{
					if (!S_ISDIR(entries[index].data.st_mode)) break;
				}
				break;
			}
			case SIZEGROUP:
			{
				if (isdir&&(sortingmethod&1)==0)
				{
					index = 0;
					break;
				}
				else if (!isdir&&(sortingmethod&1))
				{
					index = qtyEntries;
					break;
				}
				for (; index<qtyEntries; ++index)
				{
					if (!S_ISDIR(entries[index].data.st_mode)) break;
				}
				break;
			}
		}
	}

	entries = entriesPushForward(entries, index, qtyEntries);
	entries[index].name = malloc(strlen(fname)+1);
	strcpy(entries[index].name, fname);
	stat(fullpath, &entries[index].data);

	free(fname);

	*result = index;

	return entries;
}

// These macros will break everything if placed at the top
#define currEntry currEntryarr[currentWindow]
#define offset offsetarr[currentWindow]
#define backpwd backpwdarr[currentWindow]
#define qtyEntries qtyEntriesarr[currentWindow]
#define entries entriesarr[currentWindow]

int qtyEntriesarr[WINDOWQTY], currEntryarr[WINDOWQTY], offsetarr[WINDOWQTY];
entry_t *entriesarr[WINDOWQTY];

uint8_t ignoreinput = 0, isInSettings = 0;

// Handles resize operations
void resizehandler(int)
{
	clear();
	getTermXY(&maxy, &maxx);
	--maxy;

	if (maxx<MINX||maxy<MINY)
	{
		ignoreinput = 1;
		drawCentered("The terminal window is too small");
		return;
	}
	ignoreinput = 0;
	if (!isInSettings)
	{
		drawPath();
		redrawentries;
	}
	else
		drawSettings();
}

int main(int argc, char **argv)
{
	struct config_s config = loadConfig();

	currentWindow = 0;
	sortingmethod = config.sortingmethod;
	showsize = config.showsize;
	searchtype = config.searchtype;
	setSortingFunction();

	pwd[0] = '/';

	if (argc>1)
	{
		struct stat tempstat;
		realpath(argv[1], pwd+1);
		if (!stat(pwd, &tempstat)&&S_ISDIR(tempstat.st_mode))
			pwdlen = strlen(pwd);
		else
		{
			print("Invalid path\n");
			return 1;
		}
	}
	else
	{
		getcwd(pwd+1, PATH_MAX);
		pwdlen = strlen(pwd);
	}

	fillPwdData();

	initcolorpair(7, BLACK, GREEN);
	getTermXY(&maxy, &maxx);

	--maxy; // to fit the search bar

	uint8_t keypressed, windowsInitialised = 1, windowstatus = 0, cutfromwindow = 0;
	strcpy(savedpwd, pwd);

	char *backpwdarr[WINDOWQTY];
	for (int i = 0; i<WINDOWQTY; ++i)
	{
		filterarr[i][0] = 0;
		currEntryarr[i] = 0;
		offsetarr[i] = 0;
		qtyEntriesarr[i] = 0;
	}

	keepoldFile = -1;
	entries = getFileList(&qtyEntries);
	init();
	setcursor(0);
	clear();
	drawPath();
	drawEntryCount(0, 0, qtyEntries);
	drawObjects(entries, 0, qtyEntries);
	if (qtyEntries) highlightEntry(entries[0], 0);
	else accessdenied();

	workspacestring[1] = currentWindow+49;
	moveprintsize(maxy, maxx-2, workspacestring, 2);

	if (maxx<MINX||maxy<MINY) // capped by settings window
	{
		ignoreinput = 1;
		clear();
		drawCentered("The terminal window is too small");
	}


	const struct sigaction sa = {.sa_handler = resizehandler, .sa_flags = SA_RESTART};

	sigaction(SIGWINCH, &sa, NULL);

	while ((keypressed = inesc()))
	{
		if (ignoreinput&&keypressed!=config.quit) continue;
		if (keypressed==config.quit) break;
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
				if (currEntry-offset>maxy-3&&currEntry<qtyEntries-1)
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
				drawBottomLine();
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
			if (pathDepth)
			{
				backpwd = goback();
				clear();
				drawPath();
				regenerateentries;
				currEntry = findentry(backpwd, entries, qtyEntries);
				if (currEntry==-1) currEntry = 0;
				offset = currEntry?(((currEntry-currEntry%(maxy-1)>qtyEntries-(maxy-1))&&currEntry>maxy)?qtyEntries-(maxy-1):currEntry-1):currEntry;
				redrawentries;
			}	
		}
		else if (keypressed==config.deletefile)
		{
			if (qtyEntries)
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
				redrawentries;
			}
		}
		else if (keypressed==config.editfile)
		{
			if (qtyEntries)
			{
				editfname(&entries[currEntry], currEntry-offset); 
				regenerateentries;
				redrawentries;
			}
		}
		else if (keypressed==config.savedir) savePWD();
		else if (keypressed==config.loaddir)
		{	
			loadsavedPWD();
			currEntry = offset = 0;
			redrawentries;
		}
		else if (keypressed==15) // ctrl-o
		{
			isInSettings = 1;
			config = openSettings();
			isInSettings = 0;
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
			if (qtyEntries)
			{
				keepoldFile = 1;
				savecpPWD(entries[currEntry].name);
			}
		}
		else if (keypressed==config.cut)
		{
			if (qtyEntries)
			{
				keepoldFile = 0;
				cutfromwindow = currentWindow;
				savecpPWD(entries[currEntry].name);
			}
		}
		else if (keypressed==config.paste)
		{
			if (keepoldFile!=-1)
			{
				copycutFile();
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
			search();
			move(1,0);
			cleartobot();
			regenerateentries;
			redrawentries;
		}
		else if (keypressed==config.cancelsearch)
		{
			currEntry = 0;
			offset = 0;
			filter[0] = 0;
			clear();
			drawPath();
			regenerateentries;
			redrawentries;
		}
		else if (keypressed==config.createdir)
		{
			uint32_t result;
			entry_t *tempentries = createEntry(entries, qtyEntries, 1, &result);
			if (tempentries)
			{
				entries = tempentries;
				currEntry = result;
				offset = currEntry?(((currEntry-currEntry%(maxy-1)>qtyEntries-(maxy-1))&&currEntry>maxy)?qtyEntries-(maxy-1):currEntry-1):currEntry;
				++qtyEntries;
				
				clear();
				drawPath();
				windowstatus = 15-(1<<currentWindow);
				redrawentries;
			}
			else
			{
				drawPath();
				if (result!=(uint32_t)-1)
				{
					currEntry = result;
					offset = currEntry?(((currEntry-currEntry%(maxy-1)>qtyEntries-(maxy-1))&&currEntry>maxy)?qtyEntries-(maxy-1):currEntry-1):currEntry;
				}
				move(1,0);
				cleartobot();
				redrawentries;
			}
		}
		else if (keypressed==config.createfile)
		{
			uint32_t result;
			entry_t *tempentries = createEntry(entries, qtyEntries, 0, &result);
			if (tempentries)
			{
				entries = tempentries;
				currEntry = result;
				offset = currEntry?(((currEntry-currEntry%(maxy-1)>qtyEntries-(maxy-1))&&currEntry>maxy)?qtyEntries-(maxy-1):currEntry-1):currEntry;
				++qtyEntries;
				
				clear();
				drawPath();
				windowstatus = 15-(1<<currentWindow);
				redrawentries;
			}
			else
			{
				drawPath();
				if (result!=(uint32_t)-1)
				{
					currEntry = result;
					offset = currEntry?(((currEntry-currEntry%(maxy-1)>qtyEntries-(maxy-1))&&currEntry>maxy)?qtyEntries-(maxy-1):currEntry-1):currEntry;
				}
				move(1,0);
				cleartobot();
				redrawentries;
			}
		}
		else if (keypressed>='1'&&keypressed<='4')
		{
			uint8_t lastwindow = currentWindow;
			currentWindow = keypressed-49;
			if (!((windowsInitialised>>currentWindow)&1))
			{
				strcpy(pwd, pwdarr[lastwindow]);
				pwdlen = pwdlenarr[lastwindow];
				fillPwdData();
				windowsInitialised|=1<<currentWindow;
				regenerateentries;
			}
			workspacestring[1] = currentWindow+49;
			clear();
			drawPath();
			if (windowstatus>>currentWindow&1)
			{
				regenerateentries;
				windowstatus ^= 1<<currentWindow;
			}
			redrawentries;
		}
	}

	for (int i = 0; i<WINDOWQTY; ++i)
	{
		if ((windowsInitialised>>i)&1)
			freeFileList(entriesarr[i], qtyEntriesarr[i]);
	}
	freeConfig();
	setcursor(1);
	deinit();
	return 0;
}
