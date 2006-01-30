// Copyright 2002 Ian Burrell
// Author: Ian Burrell <iburrell@znark.com>

UInt32 GetROMVersion();

Char* CopyStringToNew(Char* str);
MemHandle CopyStringToHandle(Char* str);
MemHandle CopyStringToHandleAndFree(Char* str);
MemHandle CopyStringToHandleLength(Char* str, UInt16 len);

FieldType* SetFieldText(FormType* frm, UInt16 fieldID, MemHandle text);
FieldType* SetFieldTextFromStr(FormType* frm, UInt16 fieldID, Char* str);

void* GetObjectPtr(FormType* form, UInt16 objectID);

void DrawCharsFitWidth(const char *str, RectanglePtr rect);
void DrawCharsRightJustify(const char *str, RectanglePtr rect);
