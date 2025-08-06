#ifndef XMLTOOLS_H_
#define XMLTOOLS_H_

typedef struct {
	char *attr;
	char *value;
} xmlArgs;

typedef struct xmlStruct xml;
typedef struct xmlValueStruct xmlValue;
typedef unsigned char bool;

struct xmlValueStruct {
	char *tagName;
	union {
		char *str;
		xml *xmlVal;
	} value;
	xmlArgs *args;
	int argsQty;
	bool isNested;
};

struct xmlStruct {
	xml *parent;
	int tagQty;
	xmlValue *dataArr;
};

char *xmlToString(xml *ptr);
void fillEmptyXML(xml *ptr);
xml *parseXML(char *string);
void freeXML(xml *xmlDocument);

#endif
