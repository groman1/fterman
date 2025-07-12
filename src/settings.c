#include <ncurses.h>
#include <stdlib.h>
#include <stdio.h>
#include "xmltools.h"

#define keybind_t unsigned short

struct keybind_s
{
	keybind_t goUp;
	keybind_t goDown;
	keybind_t goBack;
	keybind_t goFwd;
	keybind_t editfile;
	keybind_t deletefile;
	keybind_t savedir;
	keybind_t loaddir;
	keybind_t quit;
};

xml *config;
FILE *configFile;

unsigned char getShortLen(unsigned short num)
{
	unsigned short multiplier = 10, len = 1;
	while (num/multiplier)
	{
		++len;
		multiplier*=10;
	}
	return len;
}

void keybind_tToStr(keybind_t keybind, char *dest)
{
	unsigned short multiplier = 1, len = getShortLen(keybind)-1;
	while (keybind/multiplier)
	{
		dest[len--] = keybind/multiplier%10+48;
		multiplier*=10;
	}
}

keybind_t strTokeybind_t(char *string)
{
	keybind_t keybind = 0;
	for (int i = 0; string[i]; ++i)
	{
		keybind*=10;
		keybind+=string[i]-48;
	}
	return keybind;
}

void highlightSetting(int offset)
{
	mvchgat(1+offset, 0, -1, A_REVERSE, 0, NULL);
}

void dehighlightSetting(int offset)
{
	mvchgat(1+offset, 0, -1, A_NORMAL, 0, NULL);
}

struct keybind_s loadKeybinds()
{
	struct keybind_s keybinds;
	configFile = fopen("/etc/fterman/fterman.conf", "r");
	char *confstring = malloc(1), buff;
	int i;
	for (i = 0; (buff=getc(configFile))!=EOF; ++i)
	{
		confstring = realloc(confstring, i+1);
		confstring[i] = buff;
	}
	confstring = realloc(confstring, i+1);
	confstring[i] = 0;

	config = parseXML(confstring);
	config = config->dataArr->value.xmlVal;	// IF THE PROGRAM CRASHES HERE, YOU DIDNT CREATE THE CONFIG FILE
	keybinds.goUp = strTokeybind_t(config->dataArr[0].value.str);
	keybinds.goDown = strTokeybind_t(config->dataArr[1].value.str);
	keybinds.goBack = strTokeybind_t(config->dataArr[2].value.str);
	keybinds.goFwd = strTokeybind_t(config->dataArr[3].value.str);
	keybinds.editfile = strTokeybind_t(config->dataArr[4].value.str);
	keybinds.deletefile = strTokeybind_t(config->dataArr[5].value.str);
	keybinds.savedir = strTokeybind_t(config->dataArr[6].value.str);
	keybinds.loaddir = strTokeybind_t(config->dataArr[7].value.str);
	keybinds.quit = strTokeybind_t(config->dataArr[8].value.str);
	free(confstring);
	fclose(configFile);
	return keybinds;
}

void saveKeybinds()
{
	configFile = fopen("/etc/fterman/fterman.conf", "w+");
	config = config->parent;
	char *configString = xmlToString(config);
	fprintf(configFile, "%s", configString);
	fclose(configFile);
	freeXML(config);
}

void drawSettings(struct keybind_s *keybinds)
{
	int currLine = 0;
	clear();
	printw("Keybinds (press q to exit)\n");
	if (has_colors()) init_pair(3, COLOR_BLACK, COLOR_GREEN);
	printw("Move up an entry\nMove down an entry\nOpen file or directory\nRename file\nDelete file\nGo back a directory\nSave current path\nLoad saved path\nQuit");
	highlightSetting(0);

	keybind_t ch;
	while((ch=getch())!=keybinds->quit)
	{
		switch (ch)
		{
			case 10: 
			{	ch = getch(); config->dataArr[currLine].value.str = realloc(config->dataArr[currLine].value.str, getShortLen(ch)); keybind_tToStr(ch, config->dataArr[currLine].value.str); break;	}
			case 258:
			{	if (currLine<8) { dehighlightSetting(currLine); highlightSetting(++currLine); } break;	}
			case 259:
			{	if (currLine>0) { dehighlightSetting(currLine); highlightSetting(--currLine); } break;	}
			default: break;
		}
	}
	saveKeybinds();
	*keybinds = loadKeybinds();
}
