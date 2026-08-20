#ifndef XMLTOOLS_H_
#define XMLTOOLS_H_

#include <stdbool.h>
#include <wchar.h>
#include <stdint.h>

#define appendElement(ptr, value) insertElement(ptr, value, ptr->tagQty)
#define initEmptyXML() parseXML("")

// you can change this to char if you don't need wchars
// #define WIDECHAR

#ifdef WIDECHAR
#define char_t wchar_t
#define S(s) L##s
#define string_str wcsstr
#define string_cmp wcscmp
#define string_cpy wcscpy
#define string_ncpy wcsncpy
#define string_len wcslen
#define string_chr wcschr
#else
#define S(s) s
#define char_t char
#define string_str strstr
#define string_cmp strcmp
#define string_cpy strcpy
#define string_ncpy strncpy
#define string_len strlen
#define string_chr strchr
#endif

typedef struct {
    char_t *name;
    char_t *value;
} xmlArg;

typedef struct xmlStruct xml;
typedef struct xmlTagStruct xmlTag;

struct xmlTagStruct {
    char_t *tagName;
	xml *child;
    xmlArg *args;
    uint32_t argsQty;
	bool isString;
};

struct xmlStruct {
	xml *parent;
	uint32_t tagQty;
	xmlTag *tagArr;
};

char_t *xmlToString(xml *ptr, bool format);
void fillEmptyXML(xml *ptr);
xml *parseXML(char_t *string);
void freeXML(xml *xmlDocument);
void freeXMLValue(xml *ptr, uint32_t index);
void removeElement(xml *ptr, uint32_t index);
void insertElement(xml *ptr, xmlTag value, uint32_t index);
xmlTag *findElement(xml *ptr, char_t *text);
void swapElements(xml *ptr, uint32_t index1, uint32_t index2);
xml *nestElement(xml *ptr, uint32_t index);

#endif
