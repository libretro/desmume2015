#include <string.h>
#include <fcntl.h>


#include "libfat_public_api.h"
#include "common.h"
#include "disc_io.h"
#include "fatfile.h"


struct Instance
{
	void* buffer;
	int size_bytes;
	struct devoptab_t* devops;
};

struct Instance sInstance;
struct Instance* gInstance = NULL;

bool MEDIUM_STARTUP(void)
{
	return true;
}
bool MEDIUM_ISINSERTED(void)
{
	return true;
}
bool MEDIUM_io(bool write, sec_t sector, sec_t numSectors, void* buffer)
{
	int todo = (int)numSectors*512;
	int loc = (int)sector*512;
	int have = gInstance->size_bytes - loc;
	if(todo>have) 
		return false;
	if(write)
		memcpy((uint8_t*)gInstance->buffer + loc,buffer,todo);
	else
		memcpy(buffer,(uint8_t*)gInstance->buffer + loc,todo);
	return true;
}

bool MEDIUM_READSECTORS(sec_t sector, sec_t numSectors, void* buffer)
{
	return MEDIUM_io(false,sector,numSectors,buffer);
}
bool MEDIUM_WRITESECTORS(sec_t sector, sec_t numSectors, const void* buffer)
{
	return MEDIUM_io(true,sector,numSectors,(void*)buffer);
}
bool MEDIUM_CLEARSTATUS(void)
{
	return true;
}
bool MEDIUM_SHUTDOWN(void)
{
	return true;
}

struct DISC_INTERFACE_STRUCT discio = {
	0, //unsigned long			ioType ;
	FEATURE_MEDIUM_CANWRITE | FEATURE_MEDIUM_CANREAD, //unsigned long			features ;
	MEDIUM_STARTUP, //	FN_MEDIUM_STARTUP		startup ;
	MEDIUM_ISINSERTED, //FN_MEDIUM_ISINSERTED	isInserted ;
	MEDIUM_READSECTORS, //FN_MEDIUM_READSECTORS	readSectors ;
	MEDIUM_WRITESECTORS, //FN_MEDIUM_WRITESECTORS	writeSectors ;
	MEDIUM_CLEARSTATUS, //FN_MEDIUM_CLEARSTATUS	clearStatus ;
	MEDIUM_SHUTDOWN, //FN_MEDIUM_SHUTDOWN		shutdown ;
} ;


//not declared in any libfat headers...?
bool fatMountSimple (const char* name, const DISC_INTERFACE* interface);
void fatUnmountDirect (struct devoptab_t *devops);

void LIBFAT_Init(void* buffer, int size_bytes)
{
   gInstance = &sInstance;
   gInstance->buffer = buffer;
   gInstance->size_bytes = size_bytes;
   fatMountSimple("fat",&discio);
   gInstance->devops = GetDeviceOpTab(NULL);


   int zzz=9;
}

bool LIBFAT_MkDir(const char *path)
{
   struct _reent r;
   return gInstance->devops->mkdir_r(&r,path,0) == 0;
}

bool LIBFAT_WriteFile(const char *path, const void* data, int len)
{
   struct _reent r;
   FILE_STRUCT file;
   intptr_t fd = gInstance->devops->open_r(&r,&file,path,O_CREAT | O_RDWR,0);
   if(fd != -1)
   {
      ssize_t ret = gInstance->devops->write_r(&r, fd, (char*)data, len);
      gInstance->devops->close_r(&r, fd);
      if(ret == len)
         return true;
   }
   return false;
}

void LIBFAT_Shutdown(void)
{
   fatUnmountDirect(gInstance->devops);
   gInstance = NULL;
}
