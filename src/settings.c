#include "rawtui.h"
#include <stdlib.h>
#include <stdio.h>
#include "xmltools.h"

#define option_t unsigned char

// Struct that stores all the keybinds and options necessary
struct config_s
{
	option_t goUp;
	option_t goDown;
	option_t goUpLong;
	option_t goDownLong;
	option_t goFwd;
	option_t editfile;
	option_t deletefile;
	option_t goBack;
	option_t savedir;
	option_t loaddir;
	option_t quit;
	option_t copy;
	option_t cut;
	option_t paste;
	option_t search;
	option_t cancelsearch;
	option_t sortingmethod;
	option_t showsize;
	option_t searchtype;
};

xml *config;
FILE *configFile;

// Returns the minimum required string length to store a *num* in it
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

// Converts *keybind* to string and puts it in *dest*
void option_tToStr(option_t keybind, char *dest)
{
	unsigned short multiplier = 1, len = getShortLen(keybind)-1;
	dest[len+1] = 0;
	while (keybind/multiplier)
	{
		dest[len--] = keybind/multiplier%10+48;
		multiplier*=10;
	}
	
}

// Converts *string* to option_t and return it as return value
option_t strTooption_t(char *string)
{
	option_t keybind = 0;
	for (int i = 0; string[i]; ++i)
	{
		keybind*=10;
		keybind+=string[i]-48;
	}
	return keybind;
}

// Rewrites the current string *setting* with color pair 3 enabled at line *offset*
void bindSetting(int offset, char *setting)
{
	wrattr(COLORPAIR(3));
	moveprint(1+offset, 0, setting);
	wrattr(NORMAL);
}

// Rewrites the current string *setting* with REVERSE attribute enabled at line *offset*
void highlightSetting(int offset, char *setting)
{
	wrattr(REVERSE);
	moveprint(1+offset, 0, setting);
	wrattr(NORMAL);
}

// clears the specified by *line* line
void clearSettingLine(int line)
{
	move(1+line, 0);
	clearline();
}

// Rewrites the current string *setting* with NORMAL attribute enabled at line *offset*
void dehighlightSetting(int offset, char *setting)
{
	moveprint(1+offset, 0, setting);
}

// Loads config from *configFile* and returns it as return value
struct config_s loadConfig()
{
	struct config_s configstruct;
	configFile = fopen("/etc/fterman/fterman.conf", "r");
	if (!configFile)
	{	deinit(); setcursor(1); printf("No config file detected, you probably didn't run make install\n"); exit(1);	}
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
	if (config==(void*)0x10)
	{
		deinit();
		setcursor(1);
		printf("The config file is empty, run make install-config\n");
		exit(1);
	}
	if (config->tagQty!=19)
	{
		deinit();
		setcursor(1);
		printf("Invalid config detected, run make install-config\n");
		exit(1);
	}
	configstruct.goUp = strTooption_t(config->dataArr[0].value.str);
	configstruct.goDown = strTooption_t(config->dataArr[1].value.str);
	configstruct.goUpLong = strTooption_t(config->dataArr[2].value.str);
	configstruct.goDownLong = strTooption_t(config->dataArr[3].value.str);
	configstruct.goFwd = strTooption_t(config->dataArr[4].value.str);
	configstruct.editfile = strTooption_t(config->dataArr[5].value.str);
	configstruct.deletefile = strTooption_t(config->dataArr[6].value.str);
	configstruct.goBack = strTooption_t(config->dataArr[7].value.str);
	configstruct.savedir = strTooption_t(config->dataArr[8].value.str);
	configstruct.loaddir = strTooption_t(config->dataArr[9].value.str);
	configstruct.quit = strTooption_t(config->dataArr[10].value.str);
	configstruct.copy = strTooption_t(config->dataArr[11].value.str);
	configstruct.cut = strTooption_t(config->dataArr[12].value.str);
	configstruct.paste = strTooption_t(config->dataArr[13].value.str);
	configstruct.search = strTooption_t(config->dataArr[14].value.str);
	configstruct.cancelsearch = strTooption_t(config->dataArr[15].value.str);
	configstruct.sortingmethod = config->dataArr[16].value.str[0]-48;
	configstruct.showsize = config->dataArr[17].value.str[0]-48;
	configstruct.searchtype = config->dataArr[18].value.str[0]-48;
	free(confstring);
	fclose(configFile);
	return configstruct;
}

// Writes config *config* at *configFile*
void saveConfig()
{
	configFile = fopen("/etc/fterman/fterman.conf", "w+");
	char *configString = xmlToString(config);
	fputs(configString, configFile);
	fclose(configFile);
	free(configString);
	freeXML(config);
}

// Frees config structure
void freeConfig()
{
	freeXML(config);
}

// Draws the main settings menu, returns the updated config
struct config_s drawSettings()
{
	int currLine = 0;
	clear();
	moveprint(0,0, "Keybinds			(press q to exit)\n");
	char *settings[20] = { "Move up an entry", "Move down an entry", "Move up a page", "Move down a page", "Open file or directory", "Rename file", "Delete file", "Go back a directory", "Save current path", "Load saved path", "Quit", "Copy", "Cut", "Paste", "Search", "Clear search entry", "Sorting method", "", "", "" };
	char *sortingmethods[] = { "Alphabetic (A-Z)", "Alphabetic (Z-A)", "Size (low to high)", "Size (high to low)", "Last accessed (old to new)", "Last accessed (new to old)", "Last modified (old to new)", "Last modified (new to old)" };
	char *sizestatetext[] = { "Hide size", "Show size" };
	char *searchtext[] = { "Use static search", "Use dynamic search" };

	for (int i = 0; i<17; ++i) moveprint(1+i, 0, settings[i]);
	moveprint(18, 0, sortingmethods[config->dataArr[16].value.str[0]-48]);
	moveprint(19, 0, sizestatetext[config->dataArr[17].value.str[0]-48]);
	moveprint(20, 0, searchtext[config->dataArr[18].value.str[0]-48]);

	highlightSetting(0, settings[0]);
	option_t ch;
	while((ch=inesc())!='q')
	{
		switch (ch)
		{
			case 13: 
			{	if (currLine<17)
				{
					bindSetting(currLine, settings[currLine]);	
					ch = inesc(); 
					config->dataArr[currLine].value.str = realloc(config->dataArr[currLine].value.str, getShortLen(ch)); 
					option_tToStr(ch, config->dataArr[currLine].value.str); 
					highlightSetting(currLine, settings[currLine]);
				}
				else if (currLine==18)
				{
					config->dataArr[17].value.str[0] += config->dataArr[17].value.str[0]==48?1:-1;
					highlightSetting(18, sizestatetext[config->dataArr[17].value.str[0]-48]);
				}
				else if (currLine==19)
				{
					config->dataArr[18].value.str[0] += config->dataArr[18].value.str[0]==48?1:-1;
					highlightSetting(19, searchtext[config->dataArr[18].value.str[0]-48]);
				}
				break;	
			}
			case 190:
			{
				if (currLine==17)
				{
					if (config->dataArr[16].value.str[0]==55) config->dataArr[16].value.str[0] = 47;
					++config->dataArr[16].value.str[0];
					clearSettingLine(17);
					highlightSetting(17, sortingmethods[config->dataArr[16].value.str[0]-48]);
				}
				break;
			}
			case 191:
			{
				if (currLine==17)
				{
					if (config->dataArr[16].value.str[0]==48) config->dataArr[16].value.str[0] = 56;
					--config->dataArr[16].value.str[0];
					clearSettingLine(17);
					highlightSetting(17, sortingmethods[config->dataArr[16].value.str[0]-48]);
				}
				break;
			}
			case 189:
			{	
				if (currLine<19)
				{
					if (currLine==15)
					{
						dehighlightSetting(currLine, settings[currLine]);
						++currLine;
						highlightSetting(++currLine, sortingmethods[config->dataArr[16].value.str[0]-48]);
					}
					else if (currLine==17)
					{
						dehighlightSetting(currLine, sortingmethods[config->dataArr[16].value.str[0]-48]); 
						highlightSetting(++currLine, sizestatetext[config->dataArr[17].value.str[0]-48]);
					}
					else if (currLine==18)
					{
						dehighlightSetting(currLine, sizestatetext[config->dataArr[17].value.str[0]-48]);
						highlightSetting(++currLine, searchtext[config->dataArr[18].value.str[0]-48]);
					}
					else
					{
						dehighlightSetting(currLine, settings[currLine]); 
						highlightSetting(++currLine, settings[currLine+1]);
					}
				}
				break;
			}
			case 188:
			{
				if (currLine>0)
				{
					if (currLine==17)
					{
						dehighlightSetting(currLine, sortingmethods[config->dataArr[16].value.str[0]-48]);
						--currLine;
						highlightSetting(--currLine, settings[currLine-1]); 
					}
					else if (currLine==18)
					{
						dehighlightSetting(currLine, sizestatetext[config->dataArr[17].value.str[0]-48]);
						highlightSetting(--currLine, sortingmethods[config->dataArr[16].value.str[0]-48]); 
					}
					else if (currLine==19)
					{
						dehighlightSetting(currLine, searchtext[config->dataArr[18].value.str[0]-48]); 
						highlightSetting(--currLine, sizestatetext[config->dataArr[17].value.str[0]-48]);
					}
					else
					{
						dehighlightSetting(currLine, settings[currLine]);
						highlightSetting(--currLine, settings[currLine-1]); 
					}
				}
				break;
			}
			default: break;
		}
	}
	saveConfig();
	return loadConfig();
}
