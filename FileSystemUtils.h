/*NB_REVISION*/

/*NB_COPYRIGHT*/

#ifndef _FILESYSUTIL_H
#define _FILESYSUTIL_H

#include "cardtype.h"
#include <effs_fat/fat.h>

#define MAX_EFFS_ERRORCODE   ( 38 )
extern char EffsErrorCode[][80];


#ifdef __cplusplus
extern "C"
{
#endif


//FAT Media Types for Format
#define F_FAT12_FORMAT (1)
#define F_FAT16_FORMAT (2)
#define F_FAT32_FORMAT (3)

void DisplayEffsErrorCode( int code );
BYTE InitExtFlash();
BYTE UnmountExtFlash();
BYTE FormatExtFlash( long FATtype = F_FAT32_FORMAT );
BYTE DisplayEffsSpaceStats();
BYTE DumpDir();
DWORD WriteFile( BYTE* pDataToWrite, char* pFileName, DWORD NumBytes );
DWORD AppendFile( BYTE* pDataToWrite, char* pFileName, DWORD NumBytes );
DWORD ReadFile( BYTE* pReadBuffer, char* pFileName, DWORD NumBytes );
BYTE DeleteFile( char* pFileName );
void ReadWriteTest( char *FileName = "TestFile.txt" );
void DisplayTextFile( char *FileName );
void fgets_test( char *FileName );
void fprintf_test();
void fputs_test( char *FileName );



#ifdef __cplusplus
}
#endif


#endif
