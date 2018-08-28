/*NB_REVISION*/

/*NB_COPYRIGHT*/


/* Select the card type */
#ifndef _CARDTYPE_H
#define _CARDTYPE_H

/*
  Change between devices by uncommenting one or the other. You cannot
  use both types at the same time. Whenever you change card types, you
  should do a make clean on your project.

  Warning! You must have CFC bus interface hardware on your platform or
  the code will repeatedly trap. If you are getting traps you will need to
  perform an application download using the monitor program to recover.
*/
#define USE_MMC         // SD/MMC cards
//#define USE_CFC       // Compact Flash cards
//#define USE_RAM       // RAM FileSystem, See EFFS-RAM-minimal for details
//#define EXT_FLASH_DRV_NUM (F_RAM_DRIVE0)

#if (defined USE_CFC)
#define EXT_FLASH_DRV_NUM (CFC_DRV_NUM)
#else
#define EXT_FLASH_DRV_NUM (MMC_DRV_NUM)
#endif

#endif   /* _CARDTYPE_H */

