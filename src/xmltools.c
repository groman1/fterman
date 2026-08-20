#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <wchar.h>

#include "xmltools.h"

#define min3(a, b, c) (((a)<(b)?(a):(b))<(c)?((a)<(b)?(a):(b)):(c))

typedef enum {
	TAG,
	ARG,
	VAL
} parseState;

void fillXMLHeader(xml *header, xml *parent)
{
	header->parent = parent;
	header->tagQty = 0;
	header->tagArr = 0;
}

char_t *trim(char_t *source, uint32_t len)
{
	while (*source==S(' ')||*source==S('\t')||*source==S('\n'))
	{
		++source;
		--len;
	}
	while ((source[len-1]==S(' ')||source[len-1]==S('\t')||source[len-1]==S('\n'))&&len>0) --len;
	char_t *dest = malloc(sizeof(char_t)*(len+1));
	string_ncpy(dest, source, len);
	dest[len] = 0;
	return dest;
}

#define currTag (currPtr->tagArr[currPtr->tagQty-1])

xml *parseXML(char_t *str)
{
	xml *document = malloc(sizeof(xml)), *currPtr = document;

	fillXMLHeader(document, 0);	

	parseState state = TAG;

	for (uint32_t i = 0; i<string_len(str); ++i)
	{
		if (str[i]==' ' || str[i]=='\n' || str[i]=='\t') continue;

		if (str+i == string_str(str+i, S("<!--"))) // comments
			i = string_str(str+i+4, S("-->"))-str+4;

		switch (state)
		{
			case TAG:
				if (str[i]==S('<')) // tag open
				{
					if (str[i+1]==S('?')) // pi, dont worry for now
					{
						i = string_str(str+i, S("?>"))-str+2;
						break;
					}
					currPtr->tagArr = realloc(currPtr->tagArr, (++currPtr->tagQty)*sizeof(xmlTag));
					currTag.child = NULL;
					currTag.args = NULL;
					currTag.argsQty = 0;
					currTag.isString = false;
					++i;
					uint32_t size = min3(string_chr(str+i, S(' '))-1, string_chr(str+i, S('/'))-1, string_chr(str+i, S('>'))-1)+1-str-i; // done so that if not found the value is overflown and isn't counted an min
					currTag.tagName = malloc(sizeof(char_t)*(size+1));
					string_ncpy(currTag.tagName, str+i, size);
					currTag.tagName[size] = 0;
					i += size-1;
					++state;
				}
				else // string
				{
					state = VAL;
					--i;
				}
				break;
			case ARG:
				if (str[i]==S('/'))
				{
					currTag.child = NULL;
					++i;
					state = VAL;
					break;
				}
				else if (str[i]==S('>'))
				{
					state = VAL;
					currTag.child = malloc(sizeof(xml));
					fillXMLHeader(currTag.child, currPtr);
					currPtr = currTag.child;
					break;
				}
				currTag.args = realloc(currTag.args, (++currTag.argsQty)*sizeof(xmlArg));
				uint32_t lenSpace = string_chr(str+i, S(' '))-str-i, lenEq = string_chr(str+i, S('='))-str-i;
				
				if (lenEq>lenSpace) // no value
				{
					currTag.args[currTag.argsQty-1].name = malloc(sizeof(char_t)*(lenSpace+1));
					string_ncpy(currTag.args[currTag.argsQty-1].name, str+i, lenSpace);
					currTag.args[currTag.argsQty-1].name[lenSpace] = 0;
					currTag.args[currTag.argsQty-1].value = NULL;
					i += lenSpace;
				}
				else
				{
					currTag.args[currTag.argsQty-1].name = malloc(sizeof(char_t)*(lenEq+1));
					string_ncpy(currTag.args[currTag.argsQty-1].name, str+i, lenEq);
					currTag.args[currTag.argsQty-1].name[lenEq] = 0;
					i += lenEq+1;
					if (str[i++]!=S('"')) return (xml*)1;
					lenSpace = string_chr(str+i, S('"'))-str-i;
					currTag.args[currTag.argsQty-1].value = malloc(sizeof(char_t)*(lenSpace+1));
					string_ncpy(currTag.args[currTag.argsQty-1].value, str+i, lenSpace);
					currTag.args[currTag.argsQty-1].value[lenSpace] = 0;
					i += lenSpace;
				}
				break;
			case VAL:
				if (str[i]==S('<'))
				{
					if (str[i+1]==S('/')) // tag closing
					{
						++i;
						uint32_t tagEnd = string_chr(str+i, S('>'))-str-i-1;
						char_t *tagEndText = malloc(sizeof(char_t)*(tagEnd+1));
						string_ncpy(tagEndText, str+i+1, tagEnd);
						tagEndText[tagEnd] = 0;
						currPtr = currPtr->parent;
						if (string_cmp(tagEndText, currTag.tagName)) return (xml*)1;
						i += tagEnd+1;
						free(tagEndText);
						break;
					}
					// nest
					state = TAG;
					--i;
				}
				else //string
				{
					currPtr->tagArr = realloc(currPtr->tagArr, (++currPtr->tagQty)*sizeof(xmlTag));
					currTag.isString = true;
					currTag.child = NULL;
					currTag.args = NULL;
					currTag.argsQty = 0;
					uint32_t size = string_chr(str+i, S('<'))-str-i;
					currTag.tagName = trim(str+i, size);
					i += size-1;
				}
				break;
			default:
				break;
		}
	}
	return document;
}

#undef currTag

void insertElement(xml *ptr, xmlTag tag, uint32_t index)
{
	if (index>ptr->tagQty) return;
	ptr->tagArr = realloc(ptr->tagArr, sizeof(xmlTag)*(++ptr->tagQty));
	for (uint32_t i = ptr->tagQty-1; i>index; --i)
		ptr->tagArr[i] = ptr->tagArr[i-1];
	ptr->tagArr[index] = tag;
}

void freeXMLTag(xmlTag ptr)
{
	if (ptr.child)
	{
		freeXML(ptr.child);
		ptr.child = NULL;
	}
	free(ptr.tagName);
	if (ptr.args)
	{
		for (uint32_t i = 0; i<ptr.argsQty; ++i)
		{
			free(ptr.args[i].name);
			if (ptr.args[i].value) free(ptr.args[i].value);
		}
		free(ptr.args);
	}
}

void freeXML(xml *ptr)
{
	ptr->parent = NULL;
	for (uint32_t i = 0; i<ptr->tagQty; ++i)
		freeXMLTag(ptr->tagArr[i]);
	free(ptr->tagArr);
	free(ptr);
}

void removeElement(xml *ptr, uint32_t index)
{
	if (index>=ptr->tagQty) return;
	freeXMLTag(ptr->tagArr[index]);
	for (; index<ptr->tagQty-1; ++index)
		ptr->tagArr[index] = ptr->tagArr[index+1];
	ptr->tagArr = realloc(ptr->tagArr, sizeof(xmlTag)*(--ptr->tagQty));
}

xmlTag *findElement(xml *ptr, char_t* text)
{
	for (uint32_t i = 0; i<ptr->tagQty; ++i)
		if (string_str(ptr->tagArr[i].tagName, text))
			return &ptr->tagArr[i];
	return NULL;
}

void swapElements(xml *ptr, uint32_t index1, uint32_t index2)
{
	if (index1>=ptr->tagQty||index2>=ptr->tagQty) return;
	xmlTag tempTag = ptr->tagArr[index1];
	ptr->tagArr[index1] = ptr->tagArr[index2];
	ptr->tagArr[index2] = tempTag;
}

char_t *xmlToString(xml *ptr, bool format)
{
	char_t *str = NULL;
	xml *currPtr = ptr;
	uint32_t currTag = 0, indent = 0, length = 0;

	while (!(currPtr==ptr && currTag==currPtr->tagQty))
	{
		if (currTag == currPtr->tagQty) // reached the end of current level
		{
			for (currTag = 0; currTag<currPtr->parent->tagQty; ++currTag)
				if (currPtr->parent->tagArr[currTag].child==currPtr)
					break;
			--indent;
			currPtr = currPtr->parent;
			str = realloc(str, sizeof(char_t)*(length+(indent+1)*format+string_len(currPtr->tagArr[currTag].tagName)+4));
			if (format)
				for (uint32_t i = 0; i<indent; ++i, ++length)
					str[length] = S('\t');

			str[length++] = S('<');
			str[length++] = S('/');

			for (uint32_t i = 0; currPtr->tagArr[currTag].tagName[i]; ++i)
				str[length++] = currPtr->tagArr[currTag].tagName[i];

			str[length++] = S('>');


			if (format)
				str[length++] = S('\n');

			++currTag;
			continue;
		}

		str = realloc(str, sizeof(char_t)*(length+(indent)*format+string_len(currPtr->tagArr[currTag].tagName)+!currPtr->tagArr[currTag].isString+format));
		if (format)
			for (uint32_t i = 0; i<indent; ++i)
				str[length++] = S('\t');

		if (!currPtr->tagArr[currTag].isString)
			str[length++] = S('<');

		for (uint32_t i = 0; currPtr->tagArr[currTag].tagName[i]; ++i)
			str[length++] = currPtr->tagArr[currTag].tagName[i];

		for (uint32_t i = 0; i<currPtr->tagArr[currTag].argsQty; ++i) // args, dont check for isString as it cant have args (if parsed properly ofc)
		{
			str = realloc(str, sizeof(char_t)*(length+string_len(currPtr->tagArr[currTag].args[i].name)+2));
			str[length++] = S(' ');
			for (uint32_t t = 0; currPtr->tagArr[currTag].args[i].name[t]; ++t)
				str[length++] = currPtr->tagArr[currTag].args[i].name[t];

			if (currPtr->tagArr[currTag].args[i].value)
			{
				str = realloc(str, sizeof(char_t)*(length+string_len(currPtr->tagArr[currTag].args[i].value)+4));
				str[length++] = S('=');
				str[length++] = S('"');
				for (uint32_t t = 0; currPtr->tagArr[currTag].args[i].value[t]; ++t)
					str[length++] = currPtr->tagArr[currTag].args[i].value[t];
				str[length++] = S('"');
			}
		}

		if (!currPtr->tagArr[currTag].isString)
		{
			str = realloc(str, sizeof(char_t)*(length+2+(currPtr->tagArr[currTag].child==0)));
			if (!currPtr->tagArr[currTag].child) str[length++] = S('/');
			str[length++] = S('>');
		}

		if (currPtr->tagArr[currTag].child)
		{
			currPtr = currPtr->tagArr[currTag].child;
			++indent;
			currTag = 0;
		}
		else
			++currTag;

		str[length++] = '\n';
	}

	str = realloc(str, sizeof(char_t)*(length+1));
	str[length] = 0;
	return str;
}

xml *nestElement(xml *ptr, uint32_t index)
{
	if (index>=ptr->tagQty || !ptr->tagArr[index].child) return ptr;
	return ptr->tagArr[index].child;
}
