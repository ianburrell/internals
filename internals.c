// Copyright 2002 Ian Burrell
// Author: Ian Burrell <iburrell@znark.com>


#include <PalmOS.h>
#include <PalmUtils.h>

#include "internals.h"
#include "utils.h"


// globals

static UInt32 romVersion; // ROM version

static UInt16 cardNo = 0;

//static Char** dbNames; // database names list
static Int16 dbIndex = -1; // current database number
static LocalID dbID; // current database LocalID
static DmOpenRef dbPtr; // open database pointer
static Boolean isResDb; // flag for resource database
static UInt16 startRecord = 0; // top record in list

static UInt16 recordIndex; // index of displayed record
static MemHandle recordHand; // pointer to displayed record
static UInt32 recordSize; // size of displayed record
static UInt32 recordOffset; // offset inside displayed record
static UInt32 recordLines; // number of lines

const Int16 lenDataRow = 8;
const Int16 numDataRows = 12;

const char hex[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a',
    'b','c','d','e','f'};

//static UInt16 recordIndex; // current record index


// Device Form

static Char* DeviceProcessorString()
{
    UInt32 id, chip;
    //UInt16 revision;
    Err err;
    
    err = FtrGet(sysFtrCreator, sysFtrNumProcessorID, &id);
    if (!err) {
        chip = id & sysFtrNumProcessorMask;
        //revision = id & 0x0ffff;
        if (chip == sysFtrNumProcessor328) {
            return "Motorola Dragonball";
        } else if (chip == sysFtrNumProcessorEZ) {
            return "Motorola Dragonball EZ";
        } else if (chip == sysFtrNumProcessorVZ) {
            return "Motorola Dragonball VZ";
        } else {
            // TODO: add id
            return "Unknown processor";
        }
    } else {
        return "Motorola Dragonball";
    }
}

static MemHandle DeviceOSVersionString()
{
    if (sysGetROMVerMajor(romVersion) <= 2) {
        Char buf[32];
        StrCopy(buf, "v. ");
        StrIToA(buf + StrLen(buf), sysGetROMVerMajor(romVersion));
        StrCat(buf, ".");
        StrIToA(buf + StrLen(buf), sysGetROMVerMinor(romVersion));
        //StrPrintf(buf, "v. %d.%d", major, minor);
        return CopyStringToHandle(buf);
    } else {
        return CopyStringToHandleAndFree(SysGetOSVersionString());
    }
}

static MemHandle DeviceSerialNumberString()
{
    Char* buf;
    Err err;
    UInt16 len;

    // SysGetROMToken support on PalmOS 3.0
    if (sysGetROMVerMajor(romVersion) <= 2) {
        return CopyStringToHandle("Not supported");
    }
    
    err = SysGetROMToken(0, sysROMTokenSnum, (UInt8**) &buf, &len);
    if (!err && buf != NULL && (UInt8) *buf != 0xFF) {
        return CopyStringToHandleLength(buf, len);
    } else {
        return CopyStringToHandle("No serial number");
    }
}

static void DeviceInitForm(FormPtr frm)
{
    SetFieldText(frm, DeviceVersionField, DeviceOSVersionString());
    SetFieldTextFromStr(frm, DeviceProcessorField, DeviceProcessorString());
    SetFieldText(frm, DeviceSerialNumberField, DeviceSerialNumberString());
}

Boolean DeviceFormHandler(EventPtr event)
{
    FormPtr form = FrmGetActiveForm();
    Boolean handled = false;

    switch (event->eType) {
    case frmOpenEvent:
        DeviceInitForm(form);
        FrmDrawForm(form);
        handled = true;
        break;

    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case DeviceDoneButton:
            FrmGotoForm(MainForm);
            handled = true;
            break;
        }
        break;

    default:
        break;
    }

    return handled;
}

// Display Form

// Generic string formation

static MemHandle MakeYesNoString(Boolean yesNo)
{
    return CopyStringToHandle((yesNo) ? "Yes" : "No");
}

static MemHandle MakeDisplaySize(UInt32 width, UInt32 height)
{
    Char buf[60];
    StrIToA(buf, width);
    StrCat(buf, " x ");
    StrIToA(buf + StrLen(buf), height);
    //StrPrintF(buf, "%ld x %ld", width, height);
    return CopyStringToHandle(buf);
}

static MemHandle MakeDisplayDepth(UInt32 depth)
{
    Char buf[60];
    StrIToA(buf, depth);
    return CopyStringToHandle(buf);
}

static void DisplayInitForm(FormPtr form)
{
    UInt32 width, height, curDepth;
    Boolean color;
    
    // TODO: get default, supported depths
    // TODO: color, width, height are currently fixed

    // Was named ScrDisplayMode in earlier versions, but this seems to
    // only be change in SDK and uses sames prototype and trap number
    WinScreenMode(winScreenModeGet, &width, &height, &curDepth, &color);

    SetFieldText(form, DisplaySizeField, MakeDisplaySize(width, height));
    SetFieldText(form, DisplayColorField, MakeYesNoString(color));
    SetFieldText(form, DisplayDepthField, MakeDisplayDepth(curDepth));

}

Boolean DisplayFormHandler(EventPtr event)
{
    FormPtr form = FrmGetActiveForm();
    Boolean handled = false;

    switch (event->eType) {
    case frmOpenEvent:
        DisplayInitForm(form);
        FrmDrawForm(form);
        handled = true;
        break;

    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case DisplayDoneButton:
            FrmGotoForm(MainForm);
            handled = true;
            break;
        }
        break;

    default:
        break;
    }

    return handled;
}


// Battery Form

static MemHandle BatteryVoltageString(UInt16 voltage)
{
    Char buf[20];
    StrIToA(buf, voltage / 100);
    StrCat(buf, ".");
    StrIToA(buf + StrLen(buf), (voltage % 100) / 10);
    StrIToA(buf + StrLen(buf), voltage % 10);
    StrCat(buf, " V");
    //StrPrintF(buf, "%d.%d V", voltage / 100, voltage % 100);
    return CopyStringToHandle(buf);
}

static Char* BatteryTypeString(SysBatteryKind type)
{
    switch (type) {
    case sysBatteryKindAlkaline:
        return "Alkaline";
    case sysBatteryKindNiCad:
        return "NiCad";
    case sysBatteryKindLiIon:
        return "LiIon";
    case sysBatteryKindRechAlk:
        return "Recharge Alkaline";
    case sysBatteryKindNiMH:
        return "NiMH";
    case sysBatteryKindLiIon1400:
        return "LiIon 1400";
    default:
        return "Unknown";
    }
}

static MemHandle BatteryPercentString(UInt8 percent)
{
    Char buf[20];
    if (percent == 255) {
        StrCopy(buf, "N/A");
    } else {
        StrIToA(buf, percent);
        StrCat(buf, "%");
    }
    //StrPrintF(buf, "%d%%", percent);
    return CopyStringToHandle(buf);
}

static void BatteryInitForm(FormPtr form)
{
    UInt16 voltage;
    UInt16 warnThreshold;
    UInt16 critThreshold;
    Int16 maxTicks;
    SysBatteryKind type;
    Boolean pluggedIn;
    UInt8 percent;

    if (sysGetROMVerMajor(romVersion) == 2) {
        voltage = SysBatteryInfoV20(false, &warnThreshold, &critThreshold, &maxTicks, &type, &pluggedIn);
        percent = 255;
    } else {
        voltage = SysBatteryInfo(false, &warnThreshold, &critThreshold,
                                 &maxTicks, &type, &pluggedIn, &percent);
    }

    SetFieldTextFromStr(form, BatteryTypeField, BatteryTypeString(type));
    SetFieldText(form, BatteryVoltageField, BatteryVoltageString(voltage));
    SetFieldText(form, BatteryWarningField, BatteryVoltageString(warnThreshold));
    SetFieldText(form, BatteryCriticalField, BatteryVoltageString(critThreshold));
    SetFieldText(form, BatteryPluggedInField, MakeYesNoString(pluggedIn));

    SetFieldText(form, BatteryPercentField, BatteryPercentString(percent));
    
}


Boolean BatteryFormHandler(EventPtr event)
{
    FormPtr form = FrmGetActiveForm();
    Boolean handled = false;

    switch (event->eType) {
    case frmOpenEvent:
        BatteryInitForm(form);
        FrmDrawForm(form);
        handled = true;
        break;

    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case BatteryDoneButton:
            FrmGotoForm(MainForm);
            handled = true;
            break;
        }
        break;

    default:
        break;
    }

    return handled;
}


// Memory Form

static MemHandle MakeDateTimeString(UInt32 time)
{
    Char buf[dateStringLength + timeStringLength + 1];
    DateFormatType dateFormat;
    TimeFormatType timeFormat;
    DateTimeType dt;

    if (sysGetROMVerMajor(romVersion) >= 2) {
        dateFormat = PrefGetPreference(prefDateFormat);
        timeFormat = PrefGetPreference(prefTimeFormat);
    } else {
        dateFormat = dfMDYWithSlashes;
        timeFormat = tfColonAMPM;
    }
    
    TimSecondsToDateTime(time, &dt);
    DateToAscii(dt.month, dt.day, dt.year, dateFormat, buf);
    StrCat(buf, " ");
    TimeToAscii(dt.hour, dt.minute, timeFormat, buf + StrLen(buf));
    
    return CopyStringToHandle(buf);
}

static MemHandle MemorySizeString(UInt32 size)
{
    Char buf[20];
    StrIToA(buf, size / 1024);
    StrCat(buf, " KB");
    //StrPrintF(buf, "%ld KB", size / 1024);
    return CopyStringToHandle(buf);
}

static void MemoryInitForm(FormPtr form)
{
    // TODO: handle card selections
    //UInt16 numCards = MemNumCards();

    Char nameBuf[20];
    Char manfBuf[20];
    UInt16 version;
    UInt32 creation;
    UInt32 romSize;
    UInt32 ramSize;
    UInt32 freeBytes;
    Err err;

    err = MemCardInfo(cardNo, nameBuf, manfBuf, &version, &creation, &romSize, &ramSize, &freeBytes);

    SetFieldTextFromStr(form, MemoryNameField, nameBuf);
    SetFieldTextFromStr(form, MemoryManfField, manfBuf);
    SetFieldText(form, MemoryCreationField, MakeDateTimeString(creation));
    SetFieldText(form, MemoryROMSizeField, MemorySizeString(romSize));
    SetFieldText(form, MemoryRAMSizeField, MemorySizeString(ramSize));
    SetFieldText(form, MemoryFreeBytesField, MemorySizeString(freeBytes));
    
}


Boolean MemoryFormHandler(EventPtr event)
{
    FormPtr form = FrmGetActiveForm();
    Boolean handled = false;

    switch (event->eType) {
    case frmOpenEvent:
        MemoryInitForm(form);
        FrmDrawForm(form);
        handled = true;
        break;

    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case MemoryHeapButton:
            FrmGotoForm(HeapForm);
            handled = true;
            break;
        case MemoryDoneButton:
            FrmGotoForm(MainForm);
            handled = true;
            break;
        }
        break;

    default:
        break;
    }

    return handled;
}


// Heap Form

void HeapDrawName(void* table, Int16 row, Int16 col, RectanglePtr bounds)
{
    UInt16 heap, heapID;
    Char buf[32];
    Char* name;
    
    heap = TblGetRowID(table, row);
    heapID = MemHeapID(cardNo, heap);
    
    if (MemHeapDynamic(heapID)) {
        name = "dynamic";
    } else if (MemHeapFlags(heapID) & 0x0001) {
        name = "static";
    } else {
        name = "storage";
    }

    StrIToA(buf, heap);
    StrCat(buf, " ");
    StrCat(buf, name);
    //StrPrintF(buf, "%d %s", heap, name);
    DrawCharsFitWidth(buf, bounds);
}

void HeapDrawSize(void* table, Int16 row, Int16 col, RectanglePtr bounds)
{
    UInt16 heap, heapID;
    UInt32 size, maxChunk;
    Char buf[32];
    heap = TblGetRowID(table, row);
    heapID = MemHeapID(cardNo, heap);

    if (col == 1) {
        MemHeapFreeBytes(heapID, &size, &maxChunk);
    } else if (col == 2) {
        size = MemHeapSize(heapID);
    }

    StrIToA(buf, size / 1024);
    StrCat(buf, " KB");
    //StrPrintF(buf, "%d KB", size / 1024);
    DrawCharsRightJustify(buf, bounds);
}


static void HeapDrawNumHeaps()
{
    UInt16 numHeaps;
    //UInt16 heap, heapID;
    //UInt16 dynamicHeaps, storageHeaps, staticHeaps;
    Char buf[60];

    numHeaps = MemNumHeaps(cardNo);
    
    /*
    dynamicHeaps = 0;
    storageHeaps = 0;
    staticHeaps = 0;
    for (heap = 0; heap < numHeaps; heap++) {
        heapID = MemHeapID(cardNo, heap);
        if (MemHeapDynamic(heapID)) {
            dynamicHeaps++;
        } else if (MemHeapFlags(heapID) & 0x0001) {
            staticHeaps++;
        } else {
            storageHeaps++;
        }
    }
    */

    StrIToA(buf, numHeaps);
    StrCat(buf, " heaps");
    
    //StrPrintF(buf, "%d tot %d dyn %d sto %d rom", numHeaps,
    //          dynamicHeaps, storageHeaps, staticHeaps);
    WinDrawChars(buf, StrLen(buf), 1, 16);
    
}


static void HeapInitForm(FormPtr form)
{
    //static Char* labels[] = {"Heap", "Free", "Total"};
    TablePtr table;
    UInt16 numHeaps, numRows, numDisplay;
    UInt16 col, row, heap;

    table = GetObjectPtr(form, HeapTable);
    numHeaps = MemNumHeaps(cardNo);
    numRows = TblGetNumberOfRows(table);

    /*
    // table header
    for (col = 0; col < 3; col++) {
        TblSetItemStyle(table, 0, col, labelTableItem);
        TblSetRowUsable(table, 0, true);
        TblSetItemPtr(table, 0, col, labels[col]);
    }
    */

    TblSetCustomDrawProcedure(table, 0, HeapDrawName);
    TblSetCustomDrawProcedure(table, 1, HeapDrawSize);
    TblSetCustomDrawProcedure(table, 2, HeapDrawSize);

    numDisplay = min(numRows, numHeaps);

    for (heap = 0; heap < numDisplay; heap++) {
        row = heap;

        TblSetRowID(table, row, heap);
        //heapID = MemHeapID(cardNo, heap);
        //totalSize = MemHeapSize(heapID);
        //MemHeapFreeBytes(heapID, &freeSize, &maxChunk);
        
        TblSetItemStyle(table, row, 0, customTableItem);
        //TblSetItemPtr(table, row, 0, MakeHeapName(heap, heapID));

        TblSetItemStyle(table, row, 1, customTableItem);
        //TblSetItemPtr(table, row, 1, MakeSizeString(freeSize));
        
        TblSetItemStyle(table, row, 2, customTableItem);
        //TblSetItemPtr(table, row, 2, MakeSizeString(totalSize));
        
        TblMarkRowInvalid(table, row);
        TblSetRowUsable(table, row, true);
    }

    for (col = 0; col < 3; col++) {
        TblSetColumnUsable(table, col, true);
    }

    for (row = numHeaps; row < numRows; row++) {
        TblSetRowUsable(table, row, false);
    }

}


Boolean HeapFormHandler(EventPtr event)
{
    FormPtr form = FrmGetActiveForm();
    Boolean handled = false;

    switch (event->eType) {
    case frmOpenEvent:
        HeapInitForm(form);
        FrmDrawForm(form);
        HeapDrawNumHeaps();
        handled = true;
        break;

    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case HeapDoneButton:
            FrmGotoForm(MemoryForm);
            handled = true;
            break;
        }
        break;

    default:
        break;
    }

    return handled;
}

// Database Form


static void DatabaseDrawNumber(void)
{
    Char buf[30];
    UInt16 numDatabases;
    numDatabases = DmNumDatabases(cardNo);
    StrIToA(buf, numDatabases);
    StrCat(buf, " databases");
    //StrPrintF(buf, "%d databases", num);
    WinDrawChars(buf, StrLen(buf), 1, 16);
}

void DatabaseDrawName(Int16 itemNum, RectanglePtr bounds, Char** itemsText)
{
    LocalID dbID;
    Char name[32];

    dbID = DmGetDatabase(cardNo, itemNum);
    DmDatabaseInfo(cardNo, dbID, name,
                   NULL, NULL, NULL, NULL, NULL,
                   NULL, NULL, NULL, NULL, NULL);

    DrawCharsFitWidth(name, bounds);
    
}

static void DatabaseInitForm(FormPtr form)
{
    ListPtr list;
    UInt16 numDatabases;

    list = GetObjectPtr(form, DatabaseList);
    numDatabases = DmNumDatabases(cardNo);

    /*
    SetFieldText(form, DatabaseNumField, DatabaseNumberString(numDatabases));
    
    dbNames = MemPtrNew(numDatabases * sizeof(Char*));
    buffer = MemPtrNew(numDatabases * 32);

    for (i = 0; i < numDatabases; i++) {
        LocalID dbID = DmGetDatabase(cardNo, i);
        dbNames[i] = buffer + i*32;
        // TODO: check error
        DmDatabaseInfo(cardNo, dbID, dbNames[i],
                       NULL, NULL, NULL, NULL, NULL,
                       NULL, NULL, NULL, NULL, NULL);
    }
    */
    
    LstSetListChoices(list, NULL, numDatabases);
    LstSetDrawFunction(list, DatabaseDrawName);
    LstSetSelection(list, dbIndex);
    if (dbIndex >= 0) {
        LstMakeItemVisible(list, dbIndex);
    }
}

/*
static void DatabaseFreeList()
{
    MemPtrFree(dbNames[0]);
    MemPtrFree(dbNames);
}
*/

static void DatabaseSelect(UInt16 index)
{
    dbIndex = index;
    dbID = DmGetDatabase(cardNo, dbIndex);
    FrmGotoForm(InfoForm);
}

Boolean DatabaseFormHandler(EventPtr event)
{
    FormPtr form = FrmGetActiveForm();
    Boolean handled = false;

    switch (event->eType) {
    case frmOpenEvent:
        DatabaseInitForm(form);
        FrmDrawForm(form);
        DatabaseDrawNumber();
        handled = true;
        break;

        /*
    case frmCloseEvent:
        DatabaseFreeList();
        break;
        */
        
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case DatabaseDoneButton:
            dbIndex = -1;
            FrmGotoForm(MainForm);
            handled = true;
            break;
        }
        break;

    case lstSelectEvent:
        DatabaseSelect(event->data.lstSelect.selection);
        handled = true;
        break;
        
    default:
        break;
    }

    return handled;
}


// Info Form


static MemHandle MakeIntString(UInt32 num)
{
    Char buf[maxStrIToALen];
    StrIToA(buf, num);
    return CopyStringToHandle(buf);
}

static MemHandle InfoFlagsString(UInt16 attrs)
{
    Char buf[60];

    buf[0] = '\0';
    if (attrs & dmHdrAttrResDB) {
        StrCat(buf, " rsrc");
    } 
    if (attrs & dmHdrAttrReadOnly) {
        StrCat(buf, " rom");
    }
    if (attrs & dmHdrAttrHidden) {
        StrCat(buf, " hidden");
    }
    if (attrs & dmHdrAttrBackup) {
        StrCat(buf, " backup");
    }
    if (attrs & dmHdrAttrStream) {
        StrCat(buf, " stream");
    }
    if (attrs & dmHdrAttrOpen) {
        StrCat(buf, " open");
    }
    return CopyStringToHandle(buf);
}

static void InfoInitForm(FormPtr form)
{
    Char name[32];
    UInt16 attrs, version;
    UInt32 crDate, modDate, bckDate, modNum;
    UInt32 type, creator;
    UInt32 numRecords, totalBytes, dataBytes;

    DmDatabaseInfo(cardNo, dbID, name, &attrs, &version,
                   &crDate, &modDate, &bckDate, &modNum, NULL, NULL,
                   &type, &creator);

    SetFieldTextFromStr(form, InfoNameField, name);
    // TODO: attrs
    // TODO: dates
    SetFieldText(form, InfoTypeField, CopyStringToHandleLength((char*) &type, 4));
    SetFieldText(form, InfoCreatorField, CopyStringToHandleLength((char*) &creator, 4));

    SetFieldText(form, InfoFlagsField, InfoFlagsString(attrs));

    DmDatabaseSize(cardNo, dbID, &numRecords, &totalBytes, &dataBytes);

    SetFieldText(form, InfoNumRecordsField, MakeIntString(numRecords));
    SetFieldText(form, InfoTotalSizeField, MemorySizeString(totalBytes));
    //SetFieldText(form, InfoDataSizeField, MemorySizeString(dataBytes));

    SetFieldText(form, InfoCreatedField, MakeDateTimeString(crDate));
    SetFieldText(form, InfoModifiedField, MakeDateTimeString(modDate));
    SetFieldText(form, InfoBackupedField,
                 (bckDate == 0) ? CopyStringToHandle("Never") : MakeDateTimeString(bckDate));
    
}

Boolean InfoFormHandler(EventPtr event)
{
    FormPtr form = FrmGetActiveForm();
    Boolean handled = false;

    switch (event->eType) {
    case frmOpenEvent:
        InfoInitForm(form);
        FrmDrawForm(form);
        handled = true;
        break;

    case frmCloseEvent:
        //InfoFreeInfo();
        break;
        
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case InfoDoneButton:
            FrmGotoForm(DatabaseForm);
            break;
        case InfoRecordButton:
            FrmGotoForm(RecordForm);
            break;
        }
        handled = true;
        break;

    default:
        break;
    }

    return handled;
}


// Record Form

static void RecordOpen()
{
    dbPtr = DmOpenDatabase(cardNo, dbID, dmModeReadOnly);
    if (!dbPtr) {
        // TODO: more informative message
        ErrAlert(DmGetLastErr());
        FrmGotoForm(InfoForm);
    }
    DmOpenDatabaseInfo(dbPtr, NULL, NULL, NULL, NULL, &isResDb);
}


void RecordDrawNum(void* table, Int16 row, Int16 col, RectanglePtr bounds)
{
    UInt16 rec;
    Char buf[20];
    
    rec = TblGetRowID(table, row);
    StrIToA(buf, rec);
    DrawCharsFitWidth(buf, bounds);
}

void RecordDrawSize(void* table, Int16 row, Int16 col, RectanglePtr bounds)
{
    UInt16 rec;
    UInt32 recSize;
    MemHandle recHand;
    Char buf[20];

    rec = TblGetRowID(table, row);

    if (isResDb) {
        // TODO: add resource info
        recHand = DmGetResourceIndex(dbPtr, rec);
    } else {
        // TODO: add record info
        recHand = DmQueryRecord(dbPtr, rec);
    }
    
    // TODO: check handle
    recSize = MemHandleSize(recHand);

    StrIToA(buf, recSize);
    DrawCharsRightJustify(buf, bounds);
}

static void RecordDrawType(void* table, Int16 row, Int16 col, RectanglePtr bounds)
{
    UInt16 rec;
    DmResType resType;
    DmResID resID;
    UInt16 flags;
    Char buf[32];

    rec = TblGetRowID(table, row);

    if (isResDb) {
        DmResourceInfo(dbPtr, rec, &resType, &resID, NULL);
        StrNCopy(buf, (Char*) &resType, 4);
        buf[4] = '\0';
        DrawCharsFitWidth(buf, bounds);
        StrIToA(buf, resID);
        DrawCharsRightJustify(buf, bounds);
    } else {
        DmRecordInfo(dbPtr, rec, &flags, NULL, NULL);
        buf[0] = '\0';
        if (flags & dmRecAttrDelete) {
            StrCat(buf, "del ");
        }
        if (flags & dmRecAttrDirty) {
            StrCat(buf, "dty ");
        }
        if (flags & dmRecAttrSecret) {
            StrCat(buf, "sec ");
        }
        DrawCharsFitWidth(buf, bounds);
    }
}

static void RecordLoadTable(TablePtr table)
{
    ScrollBarPtr scrollBar;
    UInt16 row, rec, numRecords, numRows;
    Int16 lastItem;
    //UInt32 recSize;
    //MemHandle recHand;

    numRows = TblGetNumberOfRows(table);
    numRecords = DmNumRecords(dbPtr);

    for (row = 0; row < numRows; row++) {
        rec = row + startRecord;
        if (rec < numRecords) {
            TblSetRowID(table, row, rec);

            /*
            TblSetItemInt(table, row, 0, rec);
            
            if (isResDb) {
                // TODO: add resource info
                recHand = DmGetResourceIndex(dbPtr, rec);
            } else {
                // TODO: add record info
                recHand = DmQueryRecord(dbPtr, rec);
            }

            // TODO: check handle
            recSize = MemHandleSize(recHand);

            // Note: Don't need to release anything
            // DmReleaseResource doesn't do anything except checks
            // DmReleaseRecord is only for setting dirty and clearing busy

            //TblSetItemStyle(table, row, 1, numericTableItem);
            TblSetItemInt(table, row, 1, recSize);
            */
            
            TblSetRowUsable(table, row, true);
        } else {
            TblSetRowUsable(table, row, false);
        }
        TblMarkRowInvalid(table, row);
    }


    lastItem = numRecords - numRows;
    if (lastItem < 0)
        lastItem = 0;

    scrollBar = GetObjectPtr(FrmGetActiveForm(), RecordScrollBar);
    SclSetScrollBar(scrollBar, startRecord, 0,  lastItem, numRows - 1);

}

static void RecordInitForm(FormPtr form)
{
    TablePtr table;
    UInt16 row, col, numRows;

    table = GetObjectPtr(form, RecordTable);
    numRows = TblGetNumberOfRows(table);

    TblHasScrollBar(table, true);
    
    for (row = 0; row < numRows; row++) {
        TblSetItemStyle(table, row, 0, customTableItem);
        TblSetItemStyle(table, row, 1, customTableItem);
        TblSetItemStyle(table, row, 2, customTableItem);
    }

    for (col = 0; col < 3; col++) {
        TblSetColumnUsable(table, col, true);
    }

    TblSetCustomDrawProcedure(table, 0, RecordDrawNum);
    TblSetCustomDrawProcedure(table, 1, RecordDrawType);
    TblSetCustomDrawProcedure(table, 2, RecordDrawSize);
    
    RecordLoadTable(table);
}

static void RecordDrawNumRecords()
{
    Char buf[60];
    UInt16 numRecords;
    numRecords = DmNumRecords(dbPtr);
    StrIToA(buf, numRecords);
    StrCat(buf, " record");
    if (numRecords != 1)
        StrCat(buf, "s");
    WinDrawChars(buf, StrLen(buf), 1, 16);
}

static void RecordClose()
{
    if (dbPtr) {
        // TODO: check error
        DmCloseDatabase(dbPtr);
    }
}

static void RecordScrollRows(Int16 rows)
{
    TablePtr table;
    UInt16 newTop;

    table = GetObjectPtr(FrmGetActiveForm(), RecordTable);
    newTop = startRecord + rows;
    if (newTop < 0)
        newTop = 0;

    if (newTop != startRecord) {
        startRecord = newTop;
        RecordLoadTable(table);
        //TblUnhighlightSelection(table);
        TblRedrawTable(table);
    }
}

static void RecordPageScroll(Boolean direction)
{
    TablePtr table;
    UInt16 numRows, scrollRows;

    table = GetObjectPtr(NULL, RecordTable);
    numRows = TblGetNumberOfRows(table);
    scrollRows = numRows - 1;
    if (direction)
        scrollRows = -scrollRows;
    RecordScrollRows(scrollRows);

}

static void RecordSelect(UInt16 row)
{
    recordIndex = startRecord + row;
    FrmGotoForm(DataForm);
}

Boolean RecordFormHandler(EventPtr event)
{
    FormPtr form = FrmGetActiveForm();
    Boolean handled = false;

    switch (event->eType) {
    case frmOpenEvent:
        RecordOpen();
        RecordInitForm(form);
        FrmDrawForm(form);
        RecordDrawNumRecords(form);
        handled = true;
        break;

    case frmCloseEvent:
        RecordClose();
        break;
        
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case RecordDoneButton:
            FrmGotoForm(InfoForm);
            break;
        }
        handled = true;
        break;

    case sclRepeatEvent:
        RecordScrollRows(event->data.sclRepeat.newValue - event->data.sclRepeat.value);
        handled = false;
        break;

    case keyDownEvent:
        if (event->data.keyDown.chr == pageUpChr) {
            RecordPageScroll(true);
            handled = true;
        } else if (event->data.keyDown.chr == pageDownChr) {
            RecordPageScroll(false);
            handled = true;
        }
        break;

    case tblSelectEvent:
        RecordSelect(event->data.tblSelect.row);
        handled = true;
        break;
        
    default:
        break;
    }

    return handled;
}

// Main Form

static void DataOpen(void)
{
    RecordOpen();
    if (isResDb) {
        // TODO: add resource info
        recordHand = DmGetResourceIndex(dbPtr, recordIndex);
    } else {
        // TODO: add record info
        recordHand = DmQueryRecord(dbPtr, recordIndex);
    }
    //recordPtr = MemHandleLock(recHand);
    recordSize = MemHandleSize(recordHand);
    recordOffset = 0;

    recordLines = recordSize / lenDataRow;
    if (recordSize % lenDataRow > 0)
        recordLines++;


}

static void DataClose(void)
{
    //MemHandleUnlock(recordPtr);
    //recordPtr = NULL;
    recordHand = NULL;
    RecordClose();
}


static void DataDrawBytes(void)
{
    const RectangleType eraseRect = {{0, 20}, {150, 120}};
    const Int16 startY = 20;
    const Int16 startX = 90;
    const Int16 avgWidth = 11;
    const Int16 height = 10;
    UInt16 row, col, len;
    Int16 y;
    MemPtr recordPtr;
    Char* rowPtr;
    Char* startPtr;
    Char* endPtr;
    Char bytes[2];
    Char byte;

    recordPtr = MemHandleLock(recordHand);
    endPtr = (Char*)recordPtr + recordSize;
    startPtr = (Char*)recordPtr + recordOffset;

    //avgWidth = FntAverageCharWidth() + 1;
    //height = FntCharHeight();

    WinEraseRectangle(&eraseRect, 0);
    
    for (row = 0; row < numDataRows; row++) {
        rowPtr = startPtr + (row * lenDataRow);
        if (rowPtr > endPtr)
            break;

        len = min(endPtr - rowPtr, lenDataRow);
        y = startY + (row * height);
        
        // draw hex
        for (col = 0; col < len; col++) {
            byte = *(rowPtr + col);
            bytes[0] = hex[(byte>>4) & 0xf];
            bytes[1] = hex[byte & 0xf];
            WinDrawChars(bytes, 2, col * avgWidth, y);
        }

        WinDrawChars(rowPtr, len, startX, y);
        
    }

    MemHandleUnlock(recordHand);
}

static void DataSetScroll()
{
    ScrollBarPtr bar;
    UInt16 row;
    Int16 height;
    
    bar = GetObjectPtr(NULL, DataScrollBar);
    row = recordOffset / lenDataRow;
    height = recordLines - numDataRows;
    if (height < 0)
        height = 0;
    SclSetScrollBar(bar, row, 0, height, numDataRows - 1);

}

static void DataScrollRows(Int16 rows)
{
    Int32 newOffset;
    newOffset = (Int32) recordOffset + (rows * lenDataRow);
    if (newOffset < 0)
        newOffset = 0;
    if (newOffset > recordSize)
        newOffset = recordSize;
    if (newOffset != recordOffset) {
        recordOffset = newOffset;
        DataSetScroll();
        DataDrawBytes();
    }
}

static void DataPageScroll(Boolean direction)
{
    Int16 scrollRows;
    scrollRows = numDataRows - 1;
    if (direction)
        scrollRows = -scrollRows;
    DataScrollRows(scrollRows);
}
 

Boolean DataFormHandler(EventPtr event)
{
    FormPtr form = FrmGetActiveForm();
    Boolean handled = false;

    switch (event->eType) {
    case frmOpenEvent:
        DataOpen();
        DataSetScroll();
        FrmDrawForm(form);
        DataDrawBytes();
        handled = true;
        break;

    case frmCloseEvent:
        DataClose();
        break;
        
    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case DataDoneButton:
            FrmGotoForm(RecordForm);
            break;
        }
        handled = true;
        break;

    case sclRepeatEvent:
        DataScrollRows(event->data.sclRepeat.newValue - event->data.sclRepeat.value);
        handled = false;
        break;

    case keyDownEvent:
        if (event->data.keyDown.chr == pageUpChr) {
            DataPageScroll(true);
            handled = true;
        } else if (event->data.keyDown.chr == pageDownChr) {
            DataPageScroll(false);
            handled = true;
        }
        break;

    default:
        break;
    }

    return handled;
}


// Remove buttons that can't be used
static void MainInitForm(FormPtr form)
{
    if (sysGetROMVerMajor(romVersion) <= 1) {
        CtlSetUsable(GetObjectPtr(form, BatteryButton), false);
    }
    if (sysGetROMVerMajor(romVersion) <= 2) {
        CtlSetUsable(GetObjectPtr(form, DisplayButton), false);
    }
}

Boolean MainFormHandler(EventPtr event)
{
    FormPtr form = FrmGetActiveForm();
    Boolean handled = false;

    switch (event->eType) {
    case frmOpenEvent:
        MainInitForm(form);
        FrmDrawForm(form);
        handled = true;
        break;

    case ctlSelectEvent:
        switch (event->data.ctlSelect.controlID) {
        case DeviceButton:
            FrmGotoForm(DeviceForm);
            handled = true;
            break;

        case DisplayButton:
            FrmGotoForm(DisplayForm);
            handled = true;
            break;

        case BatteryButton:
            FrmGotoForm(BatteryForm);
            handled = true;
            break;

        case MemoryButton:
            FrmGotoForm(MemoryForm);
            handled = true;
            break;

        case DatabaseButton:
            FrmGotoForm(DatabaseForm);
            handled = true;
            break;
            
        }

    default:
        break;
    }

    return handled;
}


// Application

static Boolean AppHandleMenuEvent(UInt16 menuID)
{
    switch (menuID) {
    case OptionsAboutMenu:
        FrmAlert(AboutBoxAlert);
        return true;

    default:
        return false;
    }
}

static Boolean AppHandleEvent(EventPtr event)
{
    Boolean handled = false;

    if (event->eType == frmLoadEvent) {
        // Load the form resource.
        UInt16 formID = event->data.frmLoad.formID;
        FormPtr	form = FrmInitForm(formID);
        FrmSetActiveForm(form);		

        switch(formID) {
        case MainForm:
            FrmSetEventHandler(form, MainFormHandler);
            break;

        case DeviceForm:
            FrmSetEventHandler(form, DeviceFormHandler);
            break;

        case DisplayForm:
            FrmSetEventHandler(form, DisplayFormHandler);
            break;

        case BatteryForm:
            FrmSetEventHandler(form, BatteryFormHandler);
            break;

        case MemoryForm:
            FrmSetEventHandler(form, MemoryFormHandler);
            break;

        case DatabaseForm:
            FrmSetEventHandler(form, DatabaseFormHandler);
            break;

        case HeapForm:
            FrmSetEventHandler(form, HeapFormHandler);
            break;

        case InfoForm:
            FrmSetEventHandler(form, InfoFormHandler);
            break;

        case RecordForm:
            FrmSetEventHandler(form, RecordFormHandler);
            break;

        case DataForm:
            FrmSetEventHandler(form, DataFormHandler);
            break;
        }
        handled = true;
    } else if (event->eType == menuEvent) {
        AppHandleMenuEvent(event->data.menu.itemID);
    }
        
	
    return handled;
}


static void AppEventLoop(void)
{
    UInt16 error;
    EventType event;

    do {
        // get an event to process.
        EvtGetEvent(&event, evtWaitForever);

        // normal processing of events.
        if (SysHandleEvent (&event)) continue;
        if (MenuHandleEvent(NULL, &event, &error)) continue;
        if (AppHandleEvent(&event)) continue;
        if (FrmDispatchEvent(&event)) continue;

    } while (event.eType != appStopEvent);

}	// AppEventLoop


static UInt16 AppStart(void)
{
    return 0;
}	


static void AppStop(void)
{
    FrmCloseAllForms();
}


UInt32 PilotMain(UInt16 cmd, MemPtr cmdPBP, UInt16 launchFlags)
{
    if (cmd == sysAppLaunchCmdNormalLaunch) {
        UInt16 error;

        romVersion = GetROMVersion();
        
        error = AppStart();
        if (error) return error;

        FrmGotoForm(MainForm);

        AppEventLoop();
        AppStop();
    }

    return 0;
}
