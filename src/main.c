#include <ncurses.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>


// ENTER YOUR SORTING METHOD HERE, ONE OF ENUM SORTTYPE VALUES
#define SORTINGMETHOD LASTMODIFIED


int color = 0, maxx, maxy, pwdlen;
char *pwd;

typedef struct
{
	char *name;
	struct stat data;
} entry_t;

enum sorttype 
{
	ALPHABETIC,
	LASTMODIFIED,
	LASTACCESED,
	SIZE
} sorttype;

unsigned char fnamelen(char *fname)
{
	for (int i = 1; i<256; ++i)
	{
		if (fname[i]==0)
		{
			return i;
		}
	}
	return 0;
}

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
		chgat(-1, A_NORMAL, 1, NULL);
	}
	else if (S_ISLNK(entry.data.st_mode))
	{
		chgat(-1, A_NORMAL, 2, NULL);
	}
	else
	{
		chgat(-1, A_NORMAL, 0, NULL);
	}
}

void highlightEntry(entry_t entry, int offset)
{
	move(offset+1, 0);
	if (S_ISDIR(entry.data.st_mode))
	{
		chgat(-1, A_REVERSE, 1, NULL);
	}
	else if (S_ISLNK(entry.data.st_mode))
	{
		chgat(-1, A_REVERSE, 2, NULL);
	}
	else
	{
		chgat(-1, A_REVERSE, 0, NULL);
	}
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

void sortEntries(enum sorttype sorttype, entry_t *entries, int qtyEntries)
{
	int sorted = 0;
	entry_t tempentry;
	switch (sorttype) 
	{
		case ALPHABETIC:
		{
			while (!sorted)
			{
				sorted = 1;
				for (int i = 0; i<qtyEntries-1; ++i)
				{
					if (strcmp(entries[i].name, entries[i+1].name)>0)
					{
						tempentry = entries[i+1];
						entries[i+1] = entries[i];
						entries[i] = tempentry;
						sorted = 0;
						break;
					}
				}
			}	
			break;
		}
		case LASTACCESED:
		{
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
			break;
		}
		case LASTMODIFIED:
		{
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
			break;
		}
		case SIZE:
		{
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
			break;
		}
	}
}

entry_t *getFileList(int *qtyEntries)
{
	struct stat *fileData;
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
	sortEntries(SORTINGMETHOD, fileList, currEntry);
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
	int currPair, overflown, currLen;
	char *sizestr = malloc(20);
	for (int i = offset; i<qtyEntries; ++i)
	{
		overflown = 0;
		if (S_ISDIR(entries[i].data.st_mode)) currPair = 1;
		else if (S_ISLNK(entries[i].data.st_mode)) currPair = 2;
		else currPair = 0;
		attron(COLOR_PAIR(currPair));
		for (currLen = 0; currLen<maxx-2&&entries[i].name[currLen]; ++currLen)
		{
			printw("%c", entries[i].name[currLen]);
		}
		if (entries[i].name[currLen]) 
		{	
			printw("..");
			overflown = 1;
		}
		attroff(COLOR_PAIR(currPair));

		sprintf(sizestr, "%ld", entries[i].data.st_size);
		if (!overflown&&fnamelen(sizestr)+currLen<maxx)	
		{
			for (int x = 0; x<maxx-currLen-fnamelen(sizestr); ++x)
			{
				printw(" ");
			}
			printw("%s", sizestr);
		}
		move(i-offset+2, 0);
	}
}

void accessdenied()
{
	mvprintw(1,0,"Access denied");
}

void goback()
{
	int i;
	for (i = pwdlen-2; pwd[i]!='/'; pwd[i--]=0);
	pwdlen = i+1;
	pwd = realloc(pwd, pwdlen+1);
}

entry_t *enterObject(entry_t *entries, int entryID, int *qtyEntries)
{
	if (!*qtyEntries) return entries;
	if (S_ISDIR(entries[entryID].data.st_mode))
	{
		pwd = realloc(pwd, pwdlen+fnamelen(entries[entryID].name)+2);
		for (int i = 0; entries[entryID].name[i]; ++i)
		{
			pwd[pwdlen++] = entries[entryID].name[i];
		}
		pwd[pwdlen++] = '/';
		pwd[pwdlen] = 0;
		freeFileList(entries, *qtyEntries);
		entries = getFileList(qtyEntries);	
		if (entries) 
		{
			drawPath();
			sortEntries(SORTINGMETHOD, entries, *qtyEntries);
			drawObjects(entries, 0, *qtyEntries);
			highlightEntry(entries[0], 0);
		}
		else
		{
			drawPath();
			accessdenied();
		}
		return entries;
	}
	else
	{
		char *editor = getenv("EDITOR"), *command = "";
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
		command = realloc(command, size+1+fnamelen(entries[entryID].name));
		for (currSize = 0; entries[entryID].name[currSize]; ++currSize)
		{
			command[size+currSize] = entries[entryID].name[currSize];
		}
		command[size+currSize] = 0;
		endwin();
		system(command);
		initscr();
		drawPath();
		drawObjects(entries, 0, *qtyEntries);
		highlightEntry(entries[0], 0);
		return entries;
	}
}

int main()
{
	initscr();
	curs_set(0);
	if (has_colors())
	{
		color = 1;
		start_color();
		init_pair(1, COLOR_BLUE, COLOR_BLACK); // directory color
		init_pair(2, COLOR_CYAN, COLOR_BLACK); // symlink color
	}
	noecho();
	keypad(stdscr, 1);
	getmaxyx(stdscr, maxy, maxx);

	int keypressed, entrySelected = 0;
	char *temppwd = getenv("PWD");
	pwdlen = strlen(temppwd);
	pwd = malloc(pwdlen+2);
	strcpy(pwd, temppwd);
	pwd[pwdlen++] = '/';
	pwd[pwdlen] = 0;

	int qtyEntries = 0, currEntry = 0, offset = 0;
	entry_t *entries = getFileList(&qtyEntries);
	drawPath();
	drawObjects(entries, 0, qtyEntries);
	highlightEntry(entries[0], 0);

	while (keypressed=getch())
	{
		switch (keypressed) 
		{
			case 81: case 113:
			{	endwin(); return 0;		}
			case 261: case 10: case 32:
			{	if (entries) {entries = enterObject(entries, currEntry+offset, &qtyEntries); currEntry = offset = 0; } break;	}
			case 258:
			{	if (currEntry<qtyEntries-1) { deHighlightEntry(entries[currEntry], currEntry-offset); ++currEntry; if (currEntry-offset>maxy-2) { ++offset; } drawPath(); drawObjects(entries, offset, qtyEntries); highlightEntry(entries[currEntry], currEntry-offset); } break;	}
			case 259:
			{	if (currEntry>0) { deHighlightEntry(entries[currEntry], currEntry-offset); --currEntry; if (currEntry-offset<0) { --offset; drawPath(); drawObjects(entries, offset, qtyEntries); } highlightEntry(entries[currEntry], currEntry-offset); } break;	}
			case 260:
			{	if(pwdlen>1) {	goback(); drawPath(); offset = 0; currEntry = 0; if (entries) {freeFileList(entries, qtyEntries); } entries = getFileList(&qtyEntries); drawObjects(entries, offset, qtyEntries); highlightEntry(entries[0], 0); } break;	}
			default: break;
		}
	}
	endwin();
	return 0;
}
