// Copyright 2002 Ian Burrell
// Author: Ian Burrell <iburrell@znark.com>

#include <PalmOS.h>

#include "utils.h"


UInt32 GetROMVersion()
{
    UInt32 version;

    if (FtrGet(sysFtrCreator, sysFtrNumROMVersion, &version) != 0)
        version = 0;

    return version;
}


Char* CopyStringToNew(Char* str)
{
    Char* dest = MemPtrNew(StrLen(str) + 1);
    StrCopy(dest, str);
    return dest;
}


MemHandle CopyStringToHandle(Char* str)
{
    MemHandle th = MemHandleNew(StrLen(str) + 1);
    Char* tp = MemHandleLock(th);
    StrCopy(tp, str);
    MemHandleUnlock(th);
    return th;
}

MemHandle CopyStringToHandleAndFree(Char* str)
{
    MemHandle h = CopyStringToHandle(str);
    MemPtrFree(str);
    return h;
}

MemHandle CopyStringToHandleLength(Char* src, UInt16 len)
{
    MemHandle sh = MemHandleNew(len + 1);
    Char* dest = MemHandleLock(sh);
    MemMove(dest, src, len);
    dest[len] = '\0';
    MemHandleUnlock(sh);
    return sh;
}

FieldPtr SetFieldText(FormPtr frm, UInt16 fieldID, MemHandle text)
{
    FieldPtr fld = GetObjectPtr(frm, fieldID);
    MemHandle oldTxt = FldGetTextHandle(fld);
    
    //Boolean fieldWasEditable = fld->attr.editable;
    //fld->attr.editable = true;
	
    FldSetTextHandle(fld, text);
    //FldDrawField(fld);
    //fld->attr.editable = fieldWasEditable;

    if (oldTxt != NULL)
        MemHandleFree(oldTxt);
	
    return fld;
}

FieldPtr SetFieldTextFromStr(FormPtr frm, UInt16 fieldID, Char* str)
{
    MemHandle txt = CopyStringToHandle(str);
    return SetFieldText(frm, fieldID, txt);
}

FieldPtr ClearFieldText(FormPtr form, UInt16 fldID)
{
    FieldPtr fld = GetObjectPtr(form, fldID);
    MemHandle txt = FldGetTextHandle(fld);
	
    if (txt != NULL) {
        FldSetTextHandle(fld, NULL);
        MemHandleFree(txt);
    }
    return fld;
}

void* GetObjectPtr(FormPtr form, UInt16 objectID)
{
    if (form == NULL)
        form = FrmGetActiveForm();
    return FrmGetObjectPtr(form, FrmGetObjectIndex(form, objectID));
}


// Draw strings at top of rectangle, but don't overwrite right-edge of
// rectangle
void DrawCharsFitWidth(const char *str, RectanglePtr rect)
{
    UInt16 length = StrLen(str);
    UInt16 width = rect->extent.x;
    Boolean truncate;
    FntCharsInWidth(str, &width, &length, &truncate);
    WinDrawChars(str, length, rect->topLeft.x, rect->topLeft.y);
}


void DrawCharsRightJustify(const char *str, RectanglePtr rect)
{
    UInt16 length = StrLen(str);
    UInt16 width = FntCharsWidth(str, length);
    UInt16 x = rect->topLeft.x + rect->extent.x - width - 1;
    WinDrawChars(str, length, x, rect->topLeft.y);
}


