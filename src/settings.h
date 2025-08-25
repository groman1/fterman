#ifndef SETTINGS_H_
#define SETTINGS_H_

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

struct keybind_s loadKeybinds();
void saveKeybinds();
void freeConfig();
void drawSettings(struct keybind_s *keybinds);

#endif
