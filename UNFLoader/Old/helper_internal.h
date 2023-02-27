#ifndef __HELPER_INTERNAL_HEADER
#define __HELPER_INTERNAL_HEADER

	void __pdprint(short color, const char* str, ...);
	void __pdprintw(WINDOW* win, short color, char log, const char* str, ...);
	void __pdprint_replace(short color, const char* str, ...);

#endif