#include "rawtui.h"
#include <stdlib.h>
#include <stdio.h>
#include "xmltools.h"

#define keybind_t unsigned char

struct keybind_s
{
	keybind_t goUp;
	keybind_t goDown;
	keybind_t goFwd;
	keybind_t editfile;
	keybind_t deletefile;
	keybind_t goBack;
	keybind_t savedir;
	keybind_t loaddir;
	keybind_t quit;
	keybind_t copy;
	keybind_t cut;
	keybind_t paste;
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
	dest[len+1] = 0;
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

void bindSetting(int offset, char *setting)
{
	wrattr(COLORPAIR(3));
	moveprint(1+offset, 0, setting);
	wrattr(NORMAL);
}

void highlightSetting(int offset, char *setting)
{
	wrattr(REVERSE);
	moveprint(1+offset, 0, setting);
	wrattr(NORMAL);
}

void dehighlightSetting(int offset, char *setting)
{
	moveprint(1+offset, 0, setting);
}

struct keybind_s loadKeybinds()
{
	struct keybind_s keybinds;
	configFile = fopen("/etc/fterman/fterman.conf", "r");
	if (!configFile)
	{	printf("No config file detected, you probably didn't run make install. Read README.md\n"); exit(1);	}
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
	keybinds.goUp = strTokeybind_t(config->dataArr[0].value.str);
	keybinds.goDown = strTokeybind_t(config->dataArr[1].value.str);
	keybinds.goFwd = strTokeybind_t(config->dataArr[2].value.str);
	keybinds.editfile = strTokeybind_t(config->dataArr[3].value.str);
	keybinds.deletefile = strTokeybind_t(config->dataArr[4].value.str);
	keybinds.goBack = strTokeybind_t(config->dataArr[5].value.str);
	keybinds.savedir = strTokeybind_t(config->dataArr[6].value.str);
	keybinds.loaddir = strTokeybind_t(config->dataArr[7].value.str);
	keybinds.quit = strTokeybind_t(config->dataArr[8].value.str);
	keybinds.copy = strTokeybind_t(config->dataArr[9].value.str);
	keybinds.cut = strTokeybind_t(config->dataArr[10].value.str);
	keybinds.paste = strTokeybind_t(config->dataArr[11].value.str);
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

void freeConfig()
{
	freeXML(config);
}

void drawSettings(struct keybind_s *keybinds)
{
	int currLine = 0;
	clear();
	moveprint(0,0, "Keybinds (press q to exit)\n");
	initcolorpair(3, BLACK, GREEN);
	char *settings[] = { "Move up an entry", "Move down an entry", "Open file or directory", "Rename file", "Delete file", "Go back a directory", "Save current path", "Load saved path", "Quit", "Copy", "Cut", "Paste" };
	for (int i = 0; i<12; ++i) moveprint(1+i, 0, settings[i]);

	highlightSetting(0, settings[0]);
	keybind_t ch;
	while((ch=inesc())!=keybinds->quit)
	{
		switch (ch)
		{
			case 13: 
			{	bindSetting(currLine, settings[currLine]);	ch = inesc(); config->dataArr[currLine].value.str = realloc(config->dataArr[currLine].value.str, getShortLen(ch)); keybind_tToStr(ch, config->dataArr[currLine].value.str); highlightSetting(currLine, settings[currLine]); break;	}
			case 189:
			{	if (currLine<11) { dehighlightSetting(currLine, settings[currLine]); highlightSetting(++currLine, settings[currLine+1]); } break;	}
			case 188:
			{	if (currLine>0) { dehighlightSetting(currLine, settings[currLine]); highlightSetting(--currLine, settings[currLine-1]); } break;	}
			default: break;
		}
	}
	saveKeybinds();
	*keybinds = loadKeybinds();
}
