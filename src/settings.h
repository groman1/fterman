#ifndef SETTINGS_H_
#define SETTINGS_H_

#define option_t unsigned char

struct option_s
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

struct option_s loadConfig();
void saveConfig();
void freeConfig();
struct option_s drawSettings();
char *strccat(char*, const char*);

#endif
