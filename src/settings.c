#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xmltools.h"
#include "rawtui.h"
#include "settings.h"

const char *defaultconfigstring = "<option for=\"goUp\">188</option>\n\
<option for=\"goDown\">189</option>\n\
<option for=\"goUpLong\">186</option>\n\
<option for=\"goDownLong\">187</option>\n\
<option for=\"goFwd\">190</option>\n\
<option for=\"editfile\">171</option>\n\
<option for=\"deletefile\">183</option>\n\
<option for=\"goBack\">191</option>\n\
<option for=\"savedir\">19</option>\n\
<option for=\"loaddir\">12</option>\n\
<option for=\"quit\">113</option>\n\
<option for=\"copy\">3</option>\n\
<option for=\"cut\">24</option>\n\
<option for=\"paste\">22</option>\n\
<option for=\"search\">47</option>\n\
<option for=\"cancelsearch\">63</option>\n\
<option for=\"createfile\">102</option>\n\
<option for=\"createdir\">100</option>\n\
<option for=\"sortingmethod\">0</option>\n\
<option for=\"showsize\">1</option>\n\
<option for=\"searchtype\">1</option>\n\
<option for=\"regfilecolor\">70</option>\n\
<option for=\"executablecolor\">20</option>\n\
<option for=\"directorycolor\">40</option>\n\
<option for=\"symlinkcolor\">60</option>\n\
<option for=\"brokensymlinkcolor\">10</option>";

#define option_t unsigned char

xml *config;
FILE *configFile;

uint8_t offset = 0, currLine = 0, currentrytype = 0, selected = 0;

// Returns the minimum required string length to store a *num* in it
unsigned char getShortLen(uint8_t num)
{
	unsigned short multiplier = 10, len = 1;
	while (num/multiplier)
	{
		++len;
		multiplier*=10;
	}
	return len;
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

// Gets a name for key *keycode*
char *getKeyName(uint8_t keycode)
{
	char *ret = malloc(4);
	switch (keycode)
	{
		case 32 ... 126: 
		{ ret[0] = keycode; ret[1] = 0; break; }
		case 127:
		{ strcpy(ret, "B-S"); break; }
		case 1 ... 26:
		{ strcpy(ret, "C-"); ret[2] = keycode+96; ret[3] = 0; break; }
		case 27:
		{ strcpy(ret, "Esc"); break; }
		case 170 ... 181:
		{ ret[0] = 'F'; option_tToStr(keycode-169, &ret[1]); break; } // f's
		case 182:
		{ strcpy(ret, "Ins"); break; }
		case 183:
		{ strcpy(ret, "Del"); break; }
		case 185:
		{ strcpy(ret, "End"); break; }
		case 186:
		{ strcpy(ret, "PgU"); break; }
		case 187:
		{ strcpy(ret, "PgD"); break; }
		case 188:
		{ strcpy(ret, "Up"); break; }
		case 189:
		{ strcpy(ret, "Dn"); break; }
		case 190:
		{ strcpy(ret, "ARr"); break; }
		case 191:
		{ strcpy(ret, "ARl"); break; }
		default:
		{ strcpy(ret, "?-?"); break; }
	}
	return ret;
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

// Rewrites the current string *setting* with color pair 4 enabled at line *offset*
void bindSetting(uint8_t line, char *setting)
{
	wrcolorpair(7);
	moveprint(line-offset, 0, setting);
	wrcolorpair(0);
}

// Rewrites the current string *setting* with REVERSE attribute enabled at line *offset*
void highlightSetting(uint8_t line, char *setting)
{
	wrattr(REVERSE);
	move(line-offset, 0);
	print(setting);
	wrattr(NORMAL);

	if (line<18)
	{
		print(" : ");
		char *keyname = getKeyName(strTooption_t(config->dataArr[line].value.str));
		print(keyname);
		free(keyname);
	}
}

// Draws color example with attribute NORMAL and colorpair specified
void drawColorOption(uint8_t line, char *entrytype, uint8_t colorpair)
{
	wrattr(NORMAL);
	wrcolorpair(colorpair);
	moveprint(line-offset, 0, entrytype);
	wrattr(NORMAL);
}

// Draws color example with attribute REVERSE and colorpair specified
void highlightColorOption(uint8_t line, char *entrytype, uint8_t colorpair)
{
	wrattr(REVERSE);
	wrcolorpair(colorpair);
	moveprint(line-offset, 0, entrytype);
	wrcolorpair(0);
	wrattr(NORMAL);
}

// clears the specified by *line* line
void clearSettingLine(int line)
{
	move(line-offset, 0);
	clearline();
}

// Rewrites the current string *setting* with NORMAL attribute enabled at line *offset*
void dehighlightSetting(int line, char *setting)
{
	moveprint(line-offset, 0, setting);
}

// Updates color pair specified
void updatecolorpair(uint8_t colorpairid)
{
	initcolorpair(colorpairid, config->dataArr[20+colorpairid].value.str[0]-48, config->dataArr[20+colorpairid].value.str[1]-48);
}

// Checks if a keybind is already in use
int findkeybind(uint8_t keybind, uint8_t id)
{
	char keybind_s[3];
	option_tToStr(keybind, (char*)keybind_s);
	if (keybind>='1'&&keybind<='4') return 1;
	for (int i = 0; i<16; ++i)
	{
		if (i==id) continue;
		if (!strcmp(keybind_s, config->dataArr[i].value.str)) return 1;
	}
	return 0;
}

// Loads config from *configFile* and returns it as return value
struct config_s loadConfig()
{
	struct config_s configstruct;
	char *homepwd = getenv("HOME");
	char *configPath = strccat(homepwd, "/.config/fterman.conf");
	configFile = fopen(configPath, "r");
	if (!configFile)
	{
createconfig:
		configFile = fopen(configPath, "w");
		fputs(defaultconfigstring, configFile);
		fclose(configFile);
		configFile = fopen(configPath, "r");
	}
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
	if (config==(void*)0x10||config->tagQty!=26) // 0 length file or outdated config
	{
		fclose(configFile);
		goto createconfig;
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
	configstruct.createfile = strTooption_t(config->dataArr[16].value.str);
	configstruct.createdir = strTooption_t(config->dataArr[17].value.str);
	configstruct.sortingmethod = config->dataArr[18].value.str[0]-48;
	configstruct.showsize = config->dataArr[19].value.str[0]-48;
	configstruct.searchtype = config->dataArr[20].value.str[0]-48;
	updatecolorpair(1);
	updatecolorpair(2);
	updatecolorpair(3);
	updatecolorpair(4);
	updatecolorpair(5);
	free(confstring);
	free(configPath);
	fclose(configFile);
	return configstruct;
}

// Writes config *config* at *configFile*
void saveConfig()
{
	char *homepwd = getenv("HOME");
	char *configPath = strccat(homepwd, "/.config/fterman.conf");
	configFile = fopen(configPath, "w+");
	char *configString = xmlToString(config);
	fputs(configString, configFile);
	fclose(configFile);
	free(configString);
	free(configPath);
	freeXML(config);
}

// Frees config structure
void freeConfig()
{
	freeXML(config);
}

static char *settings[] = { "Move up an entry", "Move down an entry", "Move up a page", "Move down a page", "Open file or directory", "Rename file", "Delete file", "Go back a directory", "Save current path", "Load saved path", "Quit", "Copy", "Cut", "Paste", "Search", "Clear search entry", "Create a file", "Create a directory"};
static char *sortingmethods[] = { "< Alphabetic (A-Z) >", "< Alphabetic (Z-A) >", "< Size (low to high) >", "< Size (high to low) >", "< Last accessed (old to new) >", "< Last accessed (new to old) >", "< Last modified (old to new) >", "< Last modified (new to old) >" };
static char *sizestatetext[] = { "Hide size", "Show size" };
static char *searchtext[] = { "Use static search", "Use dynamic search" };
static char *entrytypes[] = { "< Regular file >", "< Executable >", "< Directory >", "< Symlink >", "< Broken symlink >" };
static char *colortext[] = { "< Foreground color >", "< Background color >" };

// Used to (re)draw the settings on opening or after a resize
void drawSettings()
{
	wrcolorpair(0);
	if (currLine-offset>=maxy)
		offset = currLine;

	clear();

	for (int i = offset; i<24&&(i-offset<=maxy); ++i)
	{
		if (i==currLine) wrattr(REVERSE);
		if (selected)
		{
			wrattr(NORMAL);
			wrcolorpair(7);
		}
		switch (i)
		{
			case 0 ... 17:
			{
				moveprint(i-offset, 0, settings[i]);
				wrattr(NORMAL);
				print(" : ");
				char *keycode = getKeyName(strTooption_t(config->dataArr[i].value.str));
				print(keycode);
				free(keycode);
				break;
			}
			case 18:
			{
				uint8_t index = config->dataArr[i].value.str[0]-48;
				moveprint(i-offset, 0, sortingmethods[index]);
				break;
			}
			case 19:
			{
				uint8_t index = config->dataArr[i].value.str[0]-48;
				moveprint(i-offset, 0, sizestatetext[index]);
				break;
			}
			case 20:
			{
				uint8_t index = config->dataArr[i].value.str[0]-48;
				moveprint(i-offset, 0, searchtext[index]);
				break;
			}
			case 21:
			{
				wrcolorpair(currentrytype+1);
				moveprint(i-offset, 0, entrytypes[currentrytype]);
				wrcolorpair(0);
				break;
			}
			case 22:
			{
				moveprint(i-offset, 0, colortext[0]);
				break;
			}
			case 23:
			{
				moveprint(i-offset, 0, colortext[0]);
				break;
			}
		}
		wrattr(NORMAL);
	}
}

// Draws the main settings menu, returns the updated config
struct config_s openSettings()
{
	drawSettings();
	option_t ch;
	while((ch=inesc())!=strTooption_t(config->dataArr[10].value.str))
	{
		switch (ch)
		{
			case 13: //enter
			{
				switch (currLine)
				{
					case 0 ... 17: // keybinds
					{
						selected = 1;
						bindSetting(currLine, settings[currLine]);
						do ch = inesc();
						while (findkeybind(ch, currLine));
						selected = 0;
						config->dataArr[currLine].value.str = realloc(config->dataArr[currLine].value.str, getShortLen(ch)+1);
						option_tToStr(ch, config->dataArr[currLine].value.str); 
						clearSettingLine(currLine);
						highlightSetting(currLine, settings[currLine]);
						break;
					}
					case 19: // show size
					{
						config->dataArr[currLine].value.str[0] = !(config->dataArr[currLine].value.str[0]!=48)+48;
						clearSettingLine(currLine);
						highlightSetting(currLine, sizestatetext[config->dataArr[currLine].value.str[0]-48]);
						break;
					}
					case 20: // search : dynamic/static
					{
						config->dataArr[currLine].value.str[0] = !(config->dataArr[currLine].value.str[0]!=48)+48;
						clearSettingLine(currLine);
						highlightSetting(currLine, searchtext[config->dataArr[currLine].value.str[0]-48]);
						break;
					}
					default: break;
				}
				break;
			}
			case 190: //right
			{
				switch (currLine)
				{
					case 18: // sorting methods
					{
						if (config->dataArr[currLine].value.str[0]==55) config->dataArr[currLine].value.str[0] = 47;
						++config->dataArr[currLine].value.str[0];
						clearSettingLine(currLine);
						highlightSetting(currLine, sortingmethods[config->dataArr[currLine].value.str[0]-48]);
						break;
					}
					case 21: // entry types for colors
					{
						if (currentrytype==4) currentrytype = -1;
						++currentrytype;
						clearSettingLine(currLine);
						highlightColorOption(currLine, entrytypes[currentrytype], currentrytype+1);
						break;
					}
					case 22: // foreground
					{
						config->dataArr[21+currentrytype].value.str[0] -= 8*(config->dataArr[21+currentrytype].value.str[0]==55);
						++config->dataArr[21+currentrytype].value.str[0];
						updatecolorpair(currentrytype+1);
						drawColorOption(currLine-1, entrytypes[currentrytype], currentrytype+1);
						break;
					}
					case 23: // background
					{
						config->dataArr[21+currentrytype].value.str[1] -= 8*(config->dataArr[21+currentrytype].value.str[1]==55);
						++config->dataArr[21+currentrytype].value.str[1];
						updatecolorpair(currentrytype+1);
						drawColorOption(currLine-2, entrytypes[currentrytype], currentrytype+1);
						break;
					}
				}
				break;
			}
			case 191: //left
			{
				switch (currLine)
				{
					case 18: // sorting method
					{
						if (config->dataArr[currLine].value.str[0]==48) config->dataArr[currLine].value.str[0] = 56;
						--config->dataArr[currLine].value.str[0];
						clearSettingLine(currLine);
						highlightSetting(currLine, sortingmethods[config->dataArr[currLine].value.str[0]-48]);
						break;
					}
					case 21: // color preview
					{
						if (currentrytype==0) currentrytype = 5;
						--currentrytype;
						clearSettingLine(currLine);
						highlightColorOption(currLine, entrytypes[currentrytype], currentrytype+1);
						break;
					}
					case 22: // foreground
					{
						if (config->dataArr[21+currentrytype].value.str[0]==48) config->dataArr[21+currentrytype].value.str[0] = 56;
						--config->dataArr[21+currentrytype].value.str[0];
						updatecolorpair(currentrytype+1);
						drawColorOption(currLine-1, entrytypes[currentrytype], currentrytype+1);
						break;
					}
					case 23: // background
					{
						if (config->dataArr[21+currentrytype].value.str[1]==48) config->dataArr[21+currentrytype].value.str[1] = 56;
						--config->dataArr[21+currentrytype].value.str[1];
						updatecolorpair(currentrytype+1);
						drawColorOption(currLine-2, entrytypes[currentrytype], currentrytype+1);
						break;
					}
					default: break;
				}
				break;
			}
			case 189: //down
			{	
				if (currLine<23)
				{
					switch (currLine)
					{
						case 17: // keybinds => sorting method
						{
							dehighlightSetting(currLine, settings[currLine]);
							++currLine;
							highlightSetting(currLine, sortingmethods[config->dataArr[currLine].value.str[0]-48]);
							break;
						}
						case 18: // sorting method => show size
						{
							dehighlightSetting(currLine, sortingmethods[config->dataArr[currLine].value.str[0]-48]); 
							++currLine;
							highlightSetting(currLine, sizestatetext[config->dataArr[currLine].value.str[0]-48]);
							break;
						}
						case 19: // show size => search type
						{
							dehighlightSetting(currLine, sizestatetext[config->dataArr[currLine].value.str[0]-48]);
							++currLine;
							highlightSetting(currLine, searchtext[config->dataArr[currLine].value.str[0]-48]);
							break;
						}
						case 20: // search type => color preview
						{
							dehighlightSetting(currLine, searchtext[config->dataArr[currLine].value.str[0]-48]);
							++currLine;
							highlightColorOption(currLine, entrytypes[currentrytype], currentrytype+1);
							break;
						}
						case 21: // color preview => foreground selector
						{
							drawColorOption(currLine, entrytypes[currentrytype], currentrytype+1);
							++currLine;
							highlightSetting(currLine, colortext[0]);
							break;
						}
						case 22: // foreground selector => background selector
						{
							dehighlightSetting(currLine, colortext[0]);
							++currLine;
							highlightSetting(currLine, colortext[1]);
							break;
						}
						default: // moving between keybinds
						{
							dehighlightSetting(currLine, settings[currLine]); 
							++currLine;
							highlightSetting(currLine, settings[currLine]);
							break;
						}
					}
					if (currLine-offset>=maxy)
					{
						++offset;
						drawSettings();
					}
				}
				break;
			}
			case 188: //up
			{
				if (currLine>0)
				{
					switch (currLine)
					{
						case 18: // keybinds <= sorting method 
						{
							dehighlightSetting(currLine, sortingmethods[config->dataArr[currLine].value.str[0]-48]);
							--currLine;
							highlightSetting(currLine, settings[currLine]);
							break;
						}
						case 19: // sorting method <= show size
						{
							dehighlightSetting(currLine, sizestatetext[config->dataArr[currLine].value.str[0]-48]);
							--currLine;
							highlightSetting(currLine, sortingmethods[config->dataArr[currLine].value.str[0]-48]); 
							break;
						}
						case 20: // show size <= search type
						{
							dehighlightSetting(currLine, searchtext[config->dataArr[currLine].value.str[0]-48]); 
							--currLine;
							highlightSetting(currLine, sizestatetext[config->dataArr[currLine].value.str[0]-48]);
							break;
						}
						case 21: // search type <= color preview
						{
							drawColorOption(currLine, entrytypes[currentrytype], currentrytype+1);
							--currLine;
							highlightSetting(currLine, searchtext[config->dataArr[currLine].value.str[0]-48]);
							break;
						}
						case 22: // color preview <= foreground selector
						{
							dehighlightSetting(currLine, colortext[0]);
							--currLine;
							highlightColorOption(currLine, entrytypes[currentrytype], currentrytype+1);
							break;
						}
						case 23: // foreground selector <= background selector
						{
							dehighlightSetting(currLine, colortext[1]);
							--currLine;
							highlightSetting(currLine, colortext[0]);
							break;
						}
						default: // moving between keybinds
						{
							dehighlightSetting(currLine, settings[currLine]);
							--currLine;
							highlightSetting(currLine, settings[currLine]);
							break;
						}
					}
					if (currLine<offset)
					{
						--offset;
						drawSettings();
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
