#ifndef XMLTOOLS_H_
#define XMLTOOLS_H_

#define bool unsigned char
#define appendElement(ptr, value) insertElement(ptr, value, ptr->tagQty)
#define initEmptyXML() parseXML("")

typedef struct {
	char *attr;
	char *value;
} xmlArgs;

typedef struct xmlStruct xml;
typedef struct xmlValueStruct xmlValue;


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
void freeXMLValue(xml *ptr, int index);
void copyElement(xml *ptr, xmlValue value, int position);
int removeElement(xml *ptr, int index);
int insertElement(xml *ptr, xmlValue value, int index);
int findElement(xml *ptr, char *textToFind);
int swapElements(xml *ptr, int firstElemId, int secondElemId);
int nestElement(xml *ptr, xml *newptr, int index);

#endif
