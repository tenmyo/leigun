#include <strhash.h>
#define DENT_DIR        (1)
#define DENT_FILEOBJ    (2)

typedef struct Dirent Dirent; 
typedef struct Directory Directory;
typedef struct Filesystem Filesystem;
typedef struct FS_File FS_File;

typedef struct DirHandle 
{
	Directory *dir;
	Dirent *cursor;
} FS_DIR;


Dirent *Dent_Create(Filesystem *filesys,char *name,int type);

static inline Directory *
FS_CreateDir(Filesystem *filesys,char *name)
{
        return (Directory*)Dent_Create(filesys,name,DENT_DIR);
}

static inline FS_File *
FS_CreateFile(Filesystem *filesys,char *name)
{
        return (FS_File*)Dent_Create(filesys,name,DENT_FILEOBJ);
}

Filesystem * FS_New(void) ;
void FS_Del(Filesystem *filesys);


FS_DIR *FS_Opendir(Filesystem *,const char *name);
void FS_Closedir(FS_DIR *dir);
