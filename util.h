#ifndef _UTIL_H_
#define _UTIL_H_

#include <unistd.h>
#include <sys/stat.h>


/*******************************************************************************
 	Variables 
*******************************************************************************/

#ifdef _UTIL_MAIN_
int	Debug = 0; // Debug flag
int	Daemon = 0; // Daemon flag
int	Verbose = 0; // Verbos flag
#else
extern int	Debug; // Debug flag
extern int	Daemon; // Daemon flag
extern int	Verbose; // Verbos flag
#endif


#undef	EXTERN

/*******************************************************************************
 	Utility functions 
*******************************************************************************/

/* Output error message and exit */
void	ErrorExit(const char *, ...);

/* Put perror's message and exit */
void	PerrorExit(char *);

/* Output debug message */
void	DebugMsg(const char *, ...);

/* Output verbose message */
void	VerboseMsg(const char *, ...);


#endif /* _UTIL_H_ */
