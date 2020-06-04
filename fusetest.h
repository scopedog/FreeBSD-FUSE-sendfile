#ifndef _FUSETEST_H_
#define _FUSETEST_H_

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>

/*********************************************************************
	Definitions
*********************************************************************/

#define MountDir	"/home/www/mnt" // Mount dir
#define VIDEO_FILE	"BigBuckBunny-Full-web.mp4" // Video filename
//#define URL	"http://rnci002.ddns.net/raw-videos/BigBuckBunny-Full-web.mp4"
#define URL	"http://rnc02.asusa.net/raw/BigBuckBunny-Full-web.mp4"
			// Video file URL
#define CACHE_FILE	"cache" // Cache filename
#define DUMMY_STAT_FILE	"stat" // File containing struct stat of VIDEO_FILE

/*********************************************************************
	Structures
*********************************************************************/

/*********************************************************************
	Global varibales
*********************************************************************/

#ifdef  _FUSETEST_MAIN_
#define EXTERN
#else
#define EXTERN extern
#endif

EXTERN char	Program[PATH_MAX]; // Command name 
EXTERN int	Argc; // Arguments
EXTERN char	**Argv;
/* Moved to util.h
EXTERN int	Daemon;
EXTERN int	Debug;
EXTERN int	Verbose;
*/


#endif // _FUSETEST_H_
