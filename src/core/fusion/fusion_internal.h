#ifndef __FUSION_INTERNAL_H__
#define __FUSION_INTERNAL_H__

#include <sys/types.h>
#include <sys/user.h>
#include <asm/page.h>
#include <string.h>

#include <config.h>

#define MAX_SHMALLOC_SEGMENTS       2000
#define MAX_REACTOR_NODES            500

#define MAX_ARENA_NODES              200
#define MAX_ARENA_FIELDS             100
#define MAX_ARENA_FIELD_NAME_LENGTH  100


#define FUSION_MSGMNI     1024
#define FUSION_SEM_ARRAYS 4096

/* 4 bit Fusion's namespace   v  */
#define FUSION_KEY_PREFIX   0x70000000  /* Fusion's IPC keys start with 0x7 */

/* 4 bit other internal part   v */
#define FUSION_KEY_SHMALLOC 0x01000000  /*  */
#define FUSION_KEY_REACTOR  0x02000000  /*  */
#define FUSION_KEY_ARENA    0x03000000  /*  */

static inline key_t keygen (const char *name, const long type)
{
  int   i;
  key_t key = 0;

  for (i=0; i<strlen (name); i++)
    key ^= name[i] << ((i*7) % (sizeof(key_t)*8 - sizeof(char)*8 - 7));

  return (FUSION_KEY_PREFIX | type | key);
}

//#define FUSION_DEBUG

#ifndef FUSION_DEBUG
# define FDEBUG(x...) do {} while (0)
#else
# define FDEBUG(x...) do \
{ \
  fprintf (stderr, " -Fusion-Debug-  "__FUNCTION__": "x); \
} while (0)
#endif

#define FERROR(x...) do \
{ \
  fprintf (stderr, "**Fusion-Error** "__FUNCTION__": "x); \
} while (0)

#define FPERROR(x...) do \
{ \
  perror ("**Fusion-Error** "__FUNCTION__" - "x); \
} while (0)


#define PAGE_ALIGN(x) ((((x) + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE)
#define SHMSIZE(shm) (PAGE_ALIGN (sizeof(shm)))

/***************************************
 *  Fusion internal type declarations  *
 ***************************************/

typedef enum {
  AS_Initialize,
  AS_Attach,
  AS_Failure
} AcquisitionStatus;

typedef enum {
  AB_Destroyed,
  AB_Detached,
  AB_Failure
} AbolitionStatus;

/*******************************************
 *  Fusion internal function declarations  *
 *******************************************/

/*
 * from fusion.c
 */
int   _fusion_id();

/*
 * from util.c
 */
AcquisitionStatus _shm_acquire (key_t key, int size, int *shmid);
AbolitionStatus   _shm_abolish (int shmid, void *addr);

#endif /* __FUSION_INTERNAL_H__ */

