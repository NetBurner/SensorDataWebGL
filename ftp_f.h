/* Revision: 2.8.7 */

/*******************************************************************************
 *
 *   Copyright (c) 2003 by HCC Embedded
 *
 *   This software is copyrighted by and is the sole property of HCC.  All
 *   rights, title, ownership, or other interests in the software remain the
 *   property of HCC.  This software may only be used in accordance with the
 *   corresponding license agreement.  Any unauthorized use, duplication,
 *   transmission, distribution, or disclosure of this software is expressly
 *   forbidden.
 *
 *   This copyright notice may not be removed or modified without prior written
 *   consent of HCC.
 *
 *   HCC reserves the right to modify this software without notice.
 *
 *   HCC Embedded
 *   Budapest 1132
 *   Victor Hugo Utca 11-15
 *   Hungary
 *
 *   Tel:    +36 (1) 450 1302
 *   Fax:    +36 (1) 450 1303
 *   http:   www.hcc-embedded.com
 *   E-mail: info@hcc-embedded.com
 *
 ******************************************************************************/

#ifndef _FTP_F_H
#define _FTP_F_H

#if ( ( defined USE_NOR ) | ( defined USE_STDRAM ) ) & ( ( defined USE_CFC ) | ( defined USE_HDD ) | ( defined USE_MMC ) | ( defined USE_FATRAM ) )
#define FS_WRAPPER
#elif ( defined USE_NOR ) | ( defined USE_STDRAM )
#define FS_STD
#elif ( defined USE_CFC ) | ( defined USE_HDD ) | ( defined USE_MMC ) | ( defined USE_FATRAM )
#define FS_FAT
#endif


#endif   /* _FTP_F_H */
