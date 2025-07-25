#include <ncurses.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include "settings.h"

#define REGFILECOLOR 0
#define DIRECTORYCOLOR 1
#define SYMLINKCOLOR 2

int color = 0, maxx, maxy, pwdlen, keepoldFile;
char *pwd, *savedpwd, *filecppwd;

typedef struct
{
	char *name;
	struct stat data;
} entry_t;

unsigned char fnamelen(char *fname)
{
	for (int i = 0; i<256; ++i)
	{
		if (fname[i]==0)
		{
		return i;
		}
	}
	return 0;
}

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

void pushback(entry_t *entries, int startingIndex, int qtyEntries)
{
	free(entries[startingIndex].name);
	for (; startingIndex<qtyEntries-1; ++startingIndex)
	{
		entries[startingIndex] = entries[startingIndex+1];
	}
}

void strPushback(char *string, int startingIndex)
{
	for (; string[startingIndex+1]; ++startingIndex)
	{
		string[startingIndex] = string[startingIndex+1];
	}
	string[startingIndex] = 0;
}

void strPushfwd(char *string, int startingIndex, int stringLen) // assume string is reallocated properly
{
	for (int i = stringLen; i>startingIndex; --i)
	{
		string[i] = string[i-1];
	}
	string[stringLen+1] = 0;
}


#ifdef ALPHABETIC
char toLower(char ch)
{
	if (ch>='A'&&ch<='Z') ch += 32;
	return ch;
}

char fnamecmp(char *fname1, char *fname2)
{
	int i;
	for (i = 0; fname1[i]&&fname2[i]; ++i)
	{
		if (toLower(fname1[i])>toLower(fname2[i]))
		{
			return 1;
		}
		else if (toLower(fname2[i])>toLower(fname1[i]))
		{
			return 0;
		}
	}
	if (fname1[i])
	{
		return 0;
	}
	else
	{
		return 1;
	}
}
#endif

char *strccat(char *string1, char *string2)
{
	char *result = malloc(1);
	int x, i;
	for (x = 0; string1[x]; ++x)
	{
		result[x] = string1[x];
		result = realloc(result, x+2);
	}
	for (i = 0; string2[i]; ++i)
	{
		result[i+x] = string2[i];
		result = realloc(result, i+x+2);
	}
	result[i+x] = 0;
	return result;
}

void deHighlightEntry(entry_t entry, int offset)
{
	move(offset+1, 0);
	if (S_ISDIR(entry.data.st_mode))
	{
		chgat(-1, A_NORMAL, DIRECTORYCOLOR, NULL);
	}
	else if (S_ISLNK(entry.data.st_mode))
	{
		chgat(-1, A_NORMAL, SYMLINKCOLOR, NULL);
	}
	else
	{
		chgat(-1, A_NORMAL, REGFILECOLOR, NULL);
	}
}

void highlightEntry(entry_t entry, int offset)
{
	move(offset+1, 0);
	if (S_ISDIR(entry.data.st_mode))
	{
		chgat(-1, A_REVERSE, DIRECTORYCOLOR, NULL);
	}
	else if (S_ISLNK(entry.data.st_mode))
	{
		chgat(-1, A_REVERSE, SYMLINKCOLOR, NULL);
	}
	else
	{
		chgat(-1, A_REVERSE, REGFILECOLOR, NULL);
	}
}

void savePWD()
{
	free(savedpwd);
	savedpwd = malloc(pwdlen+1);
	strcpy(savedpwd, pwd);
}

void savecpPWD(char *entry)
{
	filecppwd = realloc(filecppwd, pwdlen+fnamelen(entry)+1);
	strcpy(filecppwd, pwd);
	strcat(filecppwd, "/");
	strcat(filecppwd, entry);
}

void loadsavedPWD()
{
	free(pwd);
	pwdlen = strlen(savedpwd);
	pwd = malloc(pwdlen);
	strcpy(pwd, savedpwd);
}

void copycutFile(int keepoldFile)
{
	char *command = malloc(9+keepoldFile*3+strlen(filecppwd)+strlen(pwd)); // 9 = 3 (cp or mv) + 1 ( ) + 1 (/ for second fname) + 4 for quotes
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

void drawPath()
{
	clear();
	move(0,0);
	if (pwdlen<maxx) printw("%s ", pwd);
	else 
	{
	
		for (int i = 0; i<maxx-2; ++i)
		{
			printw("%c", pwd[i]);
		}
	}
}

void sortEntries(entry_t *entries, int qtyEntries)
{
	int sorted = 0;
	entry_t tempentry;

#ifdef ALPHABETIC
	while (!sorted)
	{
		sorted = 1;
		for (int i = 0; i<qtyEntries-1; ++i)
		{
			if (fnamecmp(entries[i].name, entries[i+1].name))
			{
				tempentry = entries[i+1];
				entries[i+1] = entries[i];
				entries[i] = tempentry;
				sorted = 0;
				break;
			}
		}
	}	
#elifdef LASTACCESED
	while (!sorted)
	{
		sorted = 1;
		for (int i = 0; i<qtyEntries-2; ++i)
		{
			if (entries[i].data.st_atime>entries[i+1].data.st_atime)
			{
				sorted = 0;
				tempentry = entries[i+1];
				entries[i+1] = entries[i];
				entries[i] = tempentry;
			}
		}
	}
#elifdef LASTMODIFIED
	while (!sorted)
	{
		sorted = 1;
		for (int i = 0; i<qtyEntries-2; ++i)
		{
			if (entries[i].data.st_mtime>entries[i+1].data.st_mtime)
			{
				sorted = 0;
				tempentry = entries[i+1];
				entries[i+1] = entries[i];
				entries[i] = tempentry;
			}
		}
	}
#elifdef SIZE
	while (!sorted)
	{
		sorted = 1;
		for (int i = 0; i<qtyEntries-2; ++i)
		{
			if (entries[i].data.st_size>entries[i+1].data.st_size)
			{
				sorted = 0;
				tempentry = entries[i+1];
				entries[i+1] = entries[i];
				entries[i] = tempentry;
			}
		}
	}
#else 
#error "SORTINGMETHOD not set properly"
#endif
}

entry_t *getFileList(int *qtyEntries)
{
	struct dirent *entry = malloc(sizeof(struct dirent));
	DIR *dir;
	char *fileName;
	entry_t *fileList = malloc(sizeof(entry_t));
	int currEntry = 0, currLength;

	if (!(dir = opendir(pwd)))
	{ 
		return 0;
	}
	while (entry=readdir(dir))
	{
		if (!strcmp(entry->d_name, "..")) continue;
		if (!strcmp(entry->d_name, ".")) continue;
		fileName = strccat(pwd, entry->d_name);
		stat(fileName, &fileList[currEntry].data);
		free(fileName);
		currLength = fnamelen(entry->d_name);
		fileList[currEntry].name = malloc(currLength+1);
		for (int i = 0; i<=currLength; fileList[currEntry].name[i] = entry->d_name[i], ++i);
		fileList = realloc(fileList, (++currEntry+1)*sizeof(entry_t));
	}
	free(entry);
	free(dir);
	*qtyEntries = currEntry;
	sortEntries(fileList, currEntry);
	return fileList;
}

void freeFileList(entry_t *fileList, int qtyEntries)
{
	for (int i = 0; i<qtyEntries; ++i)
	{
		free(fileList[i].name);
		fileList[i].name = 0;
	}
	free(fileList);
}

void drawObjects(entry_t *entries, int offset, int qtyEntries)
{
	move(1,0);
	int currPair, currLen;
	char *sizestr = malloc(20);
	for (int i = offset; i<qtyEntries; ++i)
	{
		if (S_ISDIR(entries[i].data.st_mode)) currPair = DIRECTORYCOLOR;
		else if (S_ISLNK(entries[i].data.st_mode)) currPair = SYMLINKCOLOR;
		else currPair = REGFILECOLOR;
		attron(COLOR_PAIR(currPair));
		for (currLen = 0; currLen<maxx-4-getIntLen(entries[i].data.st_size)&&entries[i].name[currLen]; ++currLen)
		{
			printw("%c", entries[i].name[currLen]);
		}
		if (entries[i].name[currLen]) 
		{	
			printw("..");
			currLen+=2;
		}

		sprintf(sizestr, "%lu", entries[i].data.st_size);
		for (int x = 0; x<maxx-currLen-fnamelen(sizestr)-1; ++x)
		{
			printw(" ");
		}
		printw("%s", sizestr);
		attroff(COLOR_PAIR(currPair));

		move(i-offset+2, 0);
	}
}

void accessdenied()
{
	mvprintw(1,0,"Access denied");
}

char *goback(char *backpath)
{
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

entry_t *enterObject(entry_t *entries, int *entryID, int *qtyEntries)
{
	if (!*qtyEntries) return entries;
	if (S_ISDIR(entries[*entryID].data.st_mode))
	{
		pwd = realloc(pwd, pwdlen+fnamelen(entries[*entryID].name)+2);
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
			drawPath();
			sortEntries(entries, *qtyEntries);
			drawObjects(entries, 0, *qtyEntries);
			highlightEntry(entries[0], 0);
		}
		else
		{
			drawPath();
			accessdenied();
		}
		*entryID = 0;
		return entries;
	}
	else
	{
		char *editor = getenv("EDITOR");
		if (!editor) return entries; // EDITOR environment variable has to be set in order for this to work
		char *command = "";
		int size, currSize;
		command = malloc(fnamelen(editor)+1);
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
		command = realloc(command, size+1+fnamelen(entries[*entryID].name));
		for (currSize = 0; entries[*entryID].name[currSize]; ++currSize)
		{
			command[size+currSize] = entries[*entryID].name[currSize];
		}
		command[size+currSize] = 0;
		endwin();
		system(command);
		initscr();
		drawPath();
		int offset = *entryID>maxy/2?*entryID-maxy/2:0;
		drawObjects(entries, offset, *qtyEntries);
		highlightEntry(entries[*entryID], *entryID-offset);
		return entries;
	}
}

void deleteFile(char *file)
{
	char *command = malloc(7+pwdlen+fnamelen(file));
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
	command[7+pwdlen+fnamelen(file)] = 0;
	system(command);
	free(command);
}

void printName(char *name, int fileSizeLen, int offset, int currIndex)
{
	move(1+offset, 0);
	int i = fnamelen(name);
	if (i+fileSizeLen+2>=maxx)
	{
		printw("..");
		i -= maxx-fileSizeLen-4;
	}
	else i = 0;
	printw("%s", &name[i]);
	i = fnamelen(name);
	if (i+fileSizeLen+2<maxx)
	{
		mvprintw(1+offset, i, " ");
		move(1+offset, currIndex+1);
	}
	else
	{
		move(1+offset, maxx-(i-currIndex)-4);
	}
}

void editfname(entry_t *entry, int offset)
{
	char *oldname = malloc(fnamelen(entry->name)+1);
	strcpy(oldname, entry->name);
	int ch, currIndex = fnamelen(entry->name)-1, filenameLen = currIndex+1, currPair = REGFILECOLOR;
	
	int currFileSizeLen = getIntLen(entry->data.st_size), initialFileStrLen = fnamelen(oldname)+1;
	
	if (S_ISDIR(entry->data.st_mode)) currPair = DIRECTORYCOLOR;
	else if (S_ISLNK(entry->data.st_mode)) currPair = SYMLINKCOLOR;
	attron(A_REVERSE|COLOR_PAIR(currPair));

	curs_set(1);
	printName(entry->name, currFileSizeLen, offset, currIndex);
	while((ch=getch())&&ch!=10)
	{
		switch(ch)
		{
			case 'a'...'z': case 'A'...'Z': case '-': case '+': case '0'...'9': case '.': case '_':
			{	if (filenameLen<256) {entry->name = realloc(entry->name, filenameLen+2); if (filenameLen!=currIndex+1) { strPushfwd(entry->name, currIndex+1, filenameLen); }  ++filenameLen; entry->name[++currIndex] = ch; if (filenameLen==currIndex+1) { entry->name[currIndex+1] = 0; } } break;	}
			case 263:
			{	if (currIndex+1) {  strPushback(entry->name, currIndex--); if (!currIndex) { currIndex = 0; } entry->name = realloc(entry->name, filenameLen--); } break;	}
			case 27:
			{	attroff(A_REVERSE|COLOR_PAIR(currPair)); entry->name = realloc(entry->name, initialFileStrLen); strcpy(entry->name, oldname); curs_set(0); free(oldname); return;	}
			case 260:
			{	if (currIndex+1) {--currIndex; } break;	}
			case 261: 
			{	if (currIndex<filenameLen-1) { ++currIndex; } break; }
			default: break;
		}
		printName(entry->name, currFileSizeLen, offset, currIndex);
	}
	attroff(A_REVERSE|COLOR_PAIR(currPair));


	char *command = malloc(5+2*pwdlen+fnamelen(oldname)+fnamelen(entry->name)); // "mv "(3 bytes) + pwd + old filename + " " (1 byte) + pwd + new filename. As pwdlen includes the null terminator, subtract 2, but add 1 for null terminator
	char mv[] = "mv ";
	for (int i = 0; mv[i]; ++i)
	{
		command[i] = mv[i];
	}
	for (int i = 0; pwd[i]; ++i)
	{
		command[3+i] = pwd[i];
	}
	for (int i = 0; oldname[i]; ++i)
	{
		command[3+pwdlen+i] = oldname[i];
	}
	command[3+pwdlen+fnamelen(oldname)] = ' ';
	for (int i = 0; pwd[i]; ++i)
	{
		command[4+pwdlen+fnamelen(oldname)+i] = pwd[i];
	}
	for (int i = 0; entry->name[i]; ++i)
	{
		command[4+2*pwdlen+fnamelen(oldname)+i] = entry->name[i];
	}
	command[5+2*pwdlen+fnamelen(oldname)+fnamelen(entry->name)] = 0;
	system(command);

	curs_set(0);
	free(oldname);
	free(command);
}

int findentry(char *entryname, entry_t *entries, int qtyEntries)
{
	for (int i = 0; i<qtyEntries; ++i)
	{
		if (!strcmp(entries[i].name, entryname)) return i;
	}
	return -1;
}

int main()
{
	struct keybind_s keybinds = loadKeybinds();
	initscr();
#ifndef NORAW // so you can ctrl+c out of the program, conflicts with default copy keybind
	raw();
#endif
	curs_set(0);
	if (has_colors())
	{
		color = 1;
		start_color();
		init_pair(DIRECTORYCOLOR, COLOR_BLUE, COLOR_BLACK); // directory color
		init_pair(SYMLINKCOLOR, COLOR_CYAN, COLOR_BLACK); // symlink color
	}
	noecho();
	keypad(stdscr, 1);
	getmaxyx(stdscr, maxy, maxx);

	keybind_t keypressed;
	char *temppwd = getenv("PWD");
	pwdlen = strlen(temppwd);
	pwd = malloc(pwdlen+2);
	strcpy(pwd, temppwd);
	pwd[pwdlen++] = '/';
	pwd[pwdlen] = 0;
	savedpwd = malloc(pwdlen+1);
	strcpy(savedpwd, pwd);

	int qtyEntries = 0, currEntry = 0, offset = 0;
	keepoldFile = -1;
	char *backpwd = NULL;
	entry_t *entries = getFileList(&qtyEntries);
	drawPath();
	drawObjects(entries, 0, qtyEntries);
	highlightEntry(entries[0], 0);

	while (keypressed=getch())
	{
		if (keypressed==keybinds.quit)
		{	break;	}
		else if (keypressed==keybinds.goFwd)
		{	
			if (entries) 
			{
				entries = enterObject(entries, &currEntry, &qtyEntries);
				offset = currEntry>maxy/2?currEntry-maxy/2:0;
			}	
		}
		else if (keypressed==keybinds.goDown)
		{	
			if (currEntry<qtyEntries-1) 
			{ 
				deHighlightEntry(entries[currEntry], currEntry-offset); 
				++currEntry; 
				if (currEntry-offset>maxy-2)
				{
					++offset; 
					drawPath(); 
					drawObjects(entries, offset, qtyEntries); 
				}
				highlightEntry(entries[currEntry], currEntry-offset); 
			}	
		}
		else if (keypressed==keybinds.goUp)
		{	
			if (currEntry>0) 
			{ 
				deHighlightEntry(entries[currEntry], currEntry-offset); 
				--currEntry; 
				if (currEntry-offset<0)
				{	
					--offset; 
					drawPath(); 
					drawObjects(entries, offset, qtyEntries); 
				}
				highlightEntry(entries[currEntry], currEntry-offset); 
			}	
		}
		else if (keypressed==keybinds.goBack)
		{	
			if (pwdlen>1) 
			{	
				backpwd = goback(backpwd);
				drawPath();  
				if (entries) freeFileList(entries, qtyEntries); 
				entries = getFileList(&qtyEntries); 
				currEntry = findentry(backpwd, entries, qtyEntries); 
				offset = currEntry>maxy/2?currEntry-maxy/2:0;  
				drawObjects(entries, offset, qtyEntries);
				highlightEntry(entries[0], currEntry-offset); 
			}	
		}
		else if (keypressed==keybinds.deletefile)
		{	
			deleteFile(entries[currEntry].name); 
			deHighlightEntry(entries[currEntry], currEntry-offset); 
			pushback(entries, currEntry, qtyEntries); 
			--qtyEntries; 
			drawPath(); 
			drawObjects(entries, offset, qtyEntries); 
			if (currEntry==qtyEntries-1) 
			{ 
				--currEntry; 
				if (offset) --offset; 
			} 
			highlightEntry(entries[currEntry], currEntry-offset); }
		else if (keypressed==keybinds.editfile)
		{	
			editfname(&entries[currEntry], currEntry-offset); 
			drawPath(); 
			sortEntries(entries, qtyEntries); 
			drawObjects(entries, offset, qtyEntries); 
			highlightEntry(entries[currEntry], currEntry-offset);	
		}
		else if (keypressed==keybinds.savedir) savePWD();
		else if (keypressed==keybinds.loaddir)
		{	
			loadsavedPWD(); 
			if (entries) freeFileList(entries, qtyEntries); 
			entries = getFileList(&qtyEntries); 
			sortEntries(entries, qtyEntries); 
			drawPath(); 
			currEntry = offset = 0; 
			drawObjects(entries, offset, qtyEntries); 
			highlightEntry(entries[0], currEntry-offset); 
		}
		else if (keypressed==15)
		{	
			drawSettings(&keybinds); 
			drawPath(); 
			drawObjects(entries, offset, qtyEntries); 
			highlightEntry(entries[currEntry], currEntry-offset);	
		}
		else if (keypressed==keybinds.copy)
		{
			keepoldFile = 1;
			savecpPWD(entries[currEntry].name);
		}
		else if (keypressed==keybinds.cut)
		{
			keepoldFile = 0;
			savecpPWD(entries[currEntry].name);
		}
		else if (keypressed==keybinds.paste)
		{
			if (keepoldFile!=-1)
			{
				copycutFile(keepoldFile);
				drawPath();
				entries = getFileList(&qtyEntries);
				sortEntries(entries, qtyEntries);
				drawObjects(entries, offset, qtyEntries);
				highlightEntry(entries[currEntry], currEntry-offset);
			}
			if (keepoldFile==0) keepoldFile = -1;
		}
		getmaxyx(stdscr, maxy, maxx);
	}
	endwin();
	return 0;
}
