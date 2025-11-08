#include "rawtui.h"
#include <stdlib.h>
#include <stdio.h>
#include "xmltools.h"
#include <string.h>

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
<option for=\"sortingmethod\">0</option>\n\
<option for=\"showsize\">1</option>\n\
<option for=\"searchtype\">1</option>\n\
<option for=\"regfilecolor\">70</option>\n\
<option for=\"executablecolor\">20</option>\n\
<option for=\"directorycolor\">40</option>\n\
<option for=\"symlinkcolor\">60</option>\n\
<option for=\"brokensymlinkcolor\">10</option>";

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
		case 1 ... 26:
		{ strcpy(ret, "C-"); ret[2] = keycode+96; ret[3] = 0; break; }
		case 170 ... 181:
		{ ret[0] = 'f'; option_tToStr(keycode-169, &ret[1]); break; } // f's
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
		default: break;
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
void bindSetting(int offset, char *setting)
{
	wrattr(NORMAL|COLORPAIR(7));
	moveprint(1+offset, 0, setting);
	wrattr(NORMAL);
}

// Rewrites the current string *setting* with REVERSE attribute enabled at line *offset*
void highlightSetting(int offset, char *setting)
{
	wrattr(REVERSE);
	moveprint(1+offset, 0, setting);
	wrattr(NORMAL);
	if (offset<17)
	{
		print(" : ");
		char *keycode = getKeyName(strTooption_t(config->dataArr[offset].value.str));
		print(keycode);
		free(keycode);
	}
}

// Draws color example with attribute NORMAL and colorpair specified
void drawColorOption(char *entrytype, uint8_t colorpair)
{
	wrattr(NORMAL|COLORPAIR(colorpair));
	moveprint(21, 0, entrytype);
	wrattr(NORMAL);
}

// Draws color example with attribute REVERSE and colorpair specified
void highlightColorOption(char *entrytype, uint8_t colorpair)
{
	wrattr(REVERSE|COLORPAIR(colorpair));
	moveprint(21, 0, entrytype);
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
	wrattr(NORMAL);
	moveprint(1+offset, 0, setting);
}

// Updates color pair specified
void updatecolorpair(uint8_t colorpairid)
{
	initcolorpair(colorpairid, config->dataArr[18+colorpairid].value.str[0]-48, config->dataArr[18+colorpairid].value.str[1]-48);
}

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
	if (config==(void*)0x10||config->tagQty!=24) // 0 length file or outdated config
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
	configstruct.sortingmethod = config->dataArr[16].value.str[0]-48;
	configstruct.showsize = config->dataArr[17].value.str[0]-48;
	configstruct.searchtype = config->dataArr[18].value.str[0]-48;
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
void freeConfig() { freeXML(config); }

// Draws the main settings menu, returns the updated config
struct config_s drawSettings()
{
	uint8_t currLine = 0, currentrytype = 0;
	clear();
	moveprint(0,0, "Keybinds			(press q to exit)\n");
	char *settings[] = { "Move up an entry", "Move down an entry", "Move up a page", "Move down a page", "Open file or directory", "Rename file", "Delete file", "Go back a directory", "Save current path", "Load saved path", "Quit", "Copy", "Cut", "Paste", "Search", "Clear search entry", "Sorting method"};
	char *sortingmethods[] = { "< Alphabetic (A-Z) >", "< Alphabetic (Z-A) >", "< Size (low to high) >", "< Size (high to low) >", "< Last accessed (old to new) >", "< Last accessed (new to old) >", "< Last modified (old to new) >", "< Last modified (new to old) >" };
	char *sizestatetext[] = { "Hide size", "Show size" };
	char *searchtext[] = { "Use static search", "Use dynamic search" };
	char *entrytypes[] = { "< Regular file >", "< Executable >", "< Directory >", "< Symlink >", "< Broken symlink >" };
	char *colortext[] = { "< Foreground color >", "< Background color >" };

	for (int i = 0; i<16; ++i)
	{ 
		moveprint(1+i, 0, settings[i]);
		print(" : ");
		char *keycode = getKeyName(strTooption_t(config->dataArr[i].value.str));
		print(keycode);
		free(keycode);
	}
	moveprint(17, 0, settings[16]);
	moveprint(18, 0, sortingmethods[config->dataArr[16].value.str[0]-48]);
	moveprint(19, 0, sizestatetext[config->dataArr[17].value.str[0]-48]);
	moveprint(20, 0, searchtext[config->dataArr[18].value.str[0]-48]);
	drawColorOption(entrytypes[0], 1);
	for (int i = 0; i<2; ++i) moveprint(22+i, 0, colortext[i]);

	highlightSetting(0, settings[0]);
	option_t ch;
	while((ch=inesc())!='q')
	{
		switch (ch)
		{
			case 13: //enter
			{	if (currLine<17)
				{
					bindSetting(currLine, settings[currLine]);	
					do ch = inesc();
					while (findkeybind(ch, currLine));
					config->dataArr[currLine].value.str = realloc(config->dataArr[currLine].value.str, getShortLen(ch)+1);
					option_tToStr(ch, config->dataArr[currLine].value.str); 
					highlightSetting(currLine, settings[currLine]);
				}
				else if (currLine==18)
				{
					config->dataArr[17].value.str[0] = !(config->dataArr[17].value.str[0]!=48)+48;
					highlightSetting(18, sizestatetext[config->dataArr[17].value.str[0]-48]);
				}
				else if (currLine==19)
				{
					config->dataArr[18].value.str[0] = !(config->dataArr[18].value.str[0]!=48)+48;
					highlightSetting(19, searchtext[config->dataArr[18].value.str[0]-48]);
				}
				break;	
			}
			case 190: //right
			{
				switch (currLine)
				{
					case 17: // sorting methods
					{
						if (config->dataArr[16].value.str[0]==55) config->dataArr[16].value.str[0] = 47;
						++config->dataArr[16].value.str[0];
						clearSettingLine(17);
						highlightSetting(17, sortingmethods[config->dataArr[16].value.str[0]-48]);
						break;
					}
					case 20: // entry types for colors
					{
						if (currentrytype==4) currentrytype = -1;
						++currentrytype;
						clearSettingLine(20);
						highlightColorOption(entrytypes[currentrytype], currentrytype+1);
						break;
					}
					case 21:
					{
						config->dataArr[19+currentrytype].value.str[0] -= 8*(config->dataArr[19+currentrytype].value.str[0]==55);
						++config->dataArr[19+currentrytype].value.str[0];
						updatecolorpair(currentrytype+1);
						clearSettingLine(20);
						drawColorOption(entrytypes[currentrytype], currentrytype+1);
						break;
					}
					case 22:
					{
						config->dataArr[19+currentrytype].value.str[1] -= 8*(config->dataArr[19+currentrytype].value.str[1]==55);
						++config->dataArr[19+currentrytype].value.str[1];
						updatecolorpair(currentrytype+1);
						clearSettingLine(20);
						highlightColorOption(entrytypes[currentrytype], currentrytype+1);
						break;
					}
				}
				break;
			}
			case 191: //left
			{
				switch (currLine)
				{
					case 17:
					{
						if (config->dataArr[16].value.str[0]==48) config->dataArr[16].value.str[0] = 56;
						--config->dataArr[16].value.str[0];
						clearSettingLine(17);
						highlightSetting(17, sortingmethods[config->dataArr[16].value.str[0]-48]);
						break;
					}
					case 20:
					{
						if (currentrytype==0) currentrytype = 5;
						--currentrytype;
						clearSettingLine(20);
						highlightColorOption(entrytypes[currentrytype], currentrytype+1);
						break;
					}
					case 21:
					{
						if (config->dataArr[19+currentrytype].value.str[0]==48) config->dataArr[19+currentrytype].value.str[0] = 56;
						--config->dataArr[19+currentrytype].value.str[0];
						updatecolorpair(currentrytype+1);
						clearSettingLine(20);
						drawColorOption(entrytypes[currentrytype], currentrytype+1);
						break;
					}
					case 22:
					{
						if (config->dataArr[19+currentrytype].value.str[1]==48) config->dataArr[19+currentrytype].value.str[1] = 56;
						--config->dataArr[19+currentrytype].value.str[1];
						updatecolorpair(currentrytype+1);
						clearSettingLine(20);
						drawColorOption(entrytypes[currentrytype], currentrytype+1);
						break;
					}
					default: break;
				}
				break;
			}
			case 189: //down
			{	
				if (currLine<22)
				{
					switch (currLine)
					{
						case 15:
						{
							dehighlightSetting(currLine, settings[currLine]);
							++currLine;
							highlightSetting(++currLine, sortingmethods[config->dataArr[16].value.str[0]-48]);
							break;
						}
						case 17:
						{
							dehighlightSetting(currLine, sortingmethods[config->dataArr[16].value.str[0]-48]); 
							highlightSetting(++currLine, sizestatetext[config->dataArr[17].value.str[0]-48]);
							break;
						}
						case 18:
						{
							dehighlightSetting(currLine, sizestatetext[config->dataArr[17].value.str[0]-48]);
							highlightSetting(++currLine, searchtext[config->dataArr[18].value.str[0]-48]);
							break;
						}
						case 19:
						{
							dehighlightSetting(currLine, searchtext[config->dataArr[18].value.str[0]-48]);
							++currLine;
							highlightColorOption(entrytypes[currentrytype], currentrytype+1);
							break;
						}
						case 20:
						{
							drawColorOption(entrytypes[currentrytype], currentrytype+1);
							++currLine;
							highlightSetting(currLine, colortext[currLine-21]);
							break;
						}
						case 21:
						{
							dehighlightSetting(currLine, colortext[currLine-21]);
							++currLine;
							highlightSetting(currLine, colortext[currLine-21]);
							break;
						}
						default:
						{
							dehighlightSetting(currLine, settings[currLine]); 
							++currLine;
							highlightSetting(currLine, settings[currLine]);
							break;
						}
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
						case 17:
						{
							dehighlightSetting(currLine, sortingmethods[config->dataArr[16].value.str[0]-48]);
							currLine-=2;
							highlightSetting(currLine, settings[currLine]); 
							break;
						}
						case 18:
						{
							dehighlightSetting(currLine, sizestatetext[config->dataArr[17].value.str[0]-48]);
							highlightSetting(--currLine, sortingmethods[config->dataArr[16].value.str[0]-48]); 
							break;
						}
						case 19:
						{
							dehighlightSetting(currLine, searchtext[config->dataArr[18].value.str[0]-48]); 
							highlightSetting(--currLine, sizestatetext[config->dataArr[17].value.str[0]-48]);
							break;
						}
						case 20:
						{
							drawColorOption(entrytypes[currentrytype], currentrytype+1);
							highlightSetting(--currLine, searchtext[config->dataArr[18].value.str[0]-48]);
							break;
						}
						case 21:
						{
							dehighlightSetting(currLine, colortext[currLine-21]);
							--currLine;
							highlightColorOption(entrytypes[currentrytype], currentrytype+1);
							break;
						}
						case 22:
						{
							dehighlightSetting(currLine, colortext[currLine-21]);
							--currLine;
							highlightSetting(currLine, colortext[currLine-21]);
							break;
						}
						default:
						{
							dehighlightSetting(currLine, settings[currLine]);
							--currLine;
							highlightSetting(currLine, settings[currLine]); 
							break;
						}
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
