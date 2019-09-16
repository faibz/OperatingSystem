#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "mmu.h"
#include "proc.h"
#include "buf.h"
#include "bpb.h"

Device devices[NDEV];
int lastSector = 0;
int lastIndex = 0;

struct
{
	Spinlock		Lock;
	File			File[NFILE];
} FileTable;

void filesInitialise(void)
{
	File *f;

	spinlockInitialise(&FileTable.Lock, "FileTable");
	spinlockAcquire(&FileTable.Lock);
	for (f = FileTable.File; f < FileTable.File + NFILE; f++)
	{
		f->ReferenceCount = 0;
	}
	spinlockRelease(&FileTable.Lock);
}

// Allocate a file structure.
File* allocateFileStructure(void)
{
	File *f;

	spinlockAcquire(&FileTable.Lock);
	for (f = FileTable.File; f < FileTable.File + NFILE; f++) 
	{
		if (f->ReferenceCount == 0) 
		{
			f->ReferenceCount = 1;
			spinlockRelease(&FileTable.Lock);
			return f;
		}
	}
	spinlockRelease(&FileTable.Lock);
	return 0;
}

// Increment ref count for file f.
File* fileDup(File *f)
{
	spinlockAcquire(&FileTable.Lock);
	if (f->ReferenceCount < 1)
	{
		panic("fileDup");
	}
	f->ReferenceCount++;
	spinlockRelease(&FileTable.Lock);
	return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void fileClose(File *f)
{
	File ff;

	spinlockAcquire(&FileTable.Lock);
	if (f->ReferenceCount < 1)
	{
		panic("fileClose");
	}
	if (--f->ReferenceCount > 0) 
	{
		spinlockRelease(&FileTable.Lock);
		return;
	}
	ff = *f;
	f->ReferenceCount = 0;
	f->Type = FD_NONE;
	spinlockRelease(&FileTable.Lock);
	
	if (ff.Type == FD_PIPE)
	{
		pipeclose(ff.Pipe, ff.Writable);
	}
	else if (ff.Type == FD_FILE || ff.Type == FD_DIR) 
	{
		fsFat12Close(&ff);
	}

}

// Get metadata about file f.
int fileStat(File *f, Stat *st)
{
	if (f->Type == FD_FILE || f->Type == FD_DIR) 
	{
		//    ilock(f->ip);
		//    stati(f->ip, st);
		//    iunlock(f->ip);
		return 0;
	}
	return -1;
}

// Read from file f.
int fileRead(File *f, char *addr, int n)
{
	int r;

	if (f->Readable == 0)
	{
		return -1;
	}
	if (f->Type == FD_DEVICE)
	{
		return devices[f->DeviceID].read(f, addr, n);
	}
	else if (f->Type == FD_PIPE)
	{
		return piperead(f->Pipe, addr, n);
	}
	else if (f->Type == FD_FILE || f->Type == FD_DIR)
	{
		r = fsFat12Read(f, (unsigned char *)addr, n);
		return r;
	}
	panic("fileRead");
}

// Write to file f.
int fileWrite(File *f, char *addr, int n)
{
	if (f->Writable == 0)
	{
		return -1;
	}
	if (f->Type == FD_DEVICE)
	{
		return devices[f->DeviceID].write(f, addr, n);
	}
	else if (f->Type == FD_PIPE)
	{
		return pipewrite(f->Pipe, addr, n);
	}
	// Code to write to a disk file needs to be added
	panic("fileWrite");
}

int fdalloc(File *f)
{
	int fd;
	Process *curproc = myProcess();

	for (fd = 1; fd < NOFILE; fd++) 
	{
		if (curproc->OpenFile[fd] == 0) 
		{
			curproc->OpenFile[fd] = f;
			return fd;
		}
	}
	return 0;
}

int opendir(char* directory)
{
	Process* curproc = myProcess();
	File* file = 0;

	if (directory[0] == ' ')
	{
		char modCwd[200];
		char currentDirectoryName[200];
		int secondToLastSlashIndex = -1;

		strncpy(modCwd, curproc->Cwd, strlen(curproc->Cwd));
		modCwd[strlen(curproc->Cwd)] = 0;
		
		if (strlen(curproc->Cwd) > 1)
		{
			for (int16_t i = strlen(modCwd) - 3; i >= 0; i--)
			{
				if (modCwd[i] == '/')
				{
					secondToLastSlashIndex = i;
					break;
				}
			}

			for (uint16_t i = secondToLastSlashIndex + 1; i < strlen(modCwd) - 1; ++i)
			{
				currentDirectoryName[i - secondToLastSlashIndex - 1] = modCwd[i];
			}

			currentDirectoryName[strlen(currentDirectoryName)] = 0;

			for (uint16_t i = strlen(modCwd) - 2; i > 0; --i)
			{
				if (modCwd[i] == '/')
				{
					safestrcpy(curproc->Cwd, modCwd, strlen(modCwd) + 1);
					break;
				}

				modCwd[i] = 0;
			}

			file = fsFat12Open(modCwd, currentDirectoryName, 1);
		}	
	}
	else
	{
		file = fsFat12Open(curproc->Cwd, directory, 1);
	}

	if (strlen(curproc->Cwd) > 1 || file != 0)
	{
		file->Readable = 1;
		return file == 0 ? 0 : fdalloc(file);
	}

	file = allocateFileStructure();
	lastSector = 0;
	lastIndex = 0;
	curproc->OpenFile[NOFILE-1] = file;

	return NOFILE - 1;
}

int readdir(int directoryDescriptor, struct _DirectoryEntry* dirEntry)
{
	Process* curproc = myProcess();

	if (directoryDescriptor <= 0)
	{
		return -1;
	}

	File* file = curproc->OpenFile[directoryDescriptor];

	if (file == 0)
	{
		return -1;
	}

	if (directoryDescriptor < (NOFILE - 1))
	{
		char buffer[32] = {0};

		fileRead(file, buffer, 32);

		if (buffer[0] == 0 || buffer[0] == ' ') 
		{
			return -1;
		}

		memmove(dirEntry->Filename, buffer, 8);
		memmove(dirEntry->Ext, buffer + 8, 3);
		memmove(&dirEntry->Attrib, buffer + 11, 1);
		memmove(&dirEntry->Reserved, buffer + 12, 1);
		memmove(&dirEntry->TimeCreatedMs, buffer + 13, 1);
		memmove(&dirEntry->TimeCreated, buffer + 14, 2);
		memmove(&dirEntry->DateCreated, buffer + 16, 2);
		memmove(&dirEntry->DateLastAccessed, buffer + 18, 2);
		memmove(&dirEntry->FirstClusterHiBytes, buffer + 20, 2);
		memmove(&dirEntry->LastModTime, buffer + 22, 2);
		memmove(&dirEntry->LastModDate, buffer + 24, 2);
		memmove(&dirEntry->FirstCluster, buffer + 26, 2);
		memmove(&dirEntry->FileSize, buffer + 28, 4);
	}
	else
	{
		MountInfo* mountInfo = getMountInfo();
		int rootDirectorySectorCount = mountInfo->NumRootEntries / 16;

		DiskBuffer* diskBuffer;
		DirectoryEntry* directoryEntry;

		for(; lastSector < rootDirectorySectorCount; lastSector++)
		{
			diskBuffer = diskBufferRead(0, mountInfo->RootOffset + lastSector);
			directoryEntry = (DirectoryEntry*) diskBuffer->Data;

			//because of the nature of this solution, the directory entry needs to be caught up to where it would have been.
			for(int i = 0; i < lastIndex; i++) 
			{
				directoryEntry++;
			}

			for (; lastIndex < 16; lastIndex++)
			{
				if (*(directoryEntry->Filename) != 0)
				{
					memmove(dirEntry->Filename, directoryEntry->Filename, 8);
					memmove(dirEntry->Ext, directoryEntry->Ext, 3);
					memmove(&dirEntry->Attrib, &directoryEntry->Attrib, 1);
					memmove(&dirEntry->Reserved, &directoryEntry->Reserved, 1);
					memmove(&dirEntry->TimeCreatedMs, &directoryEntry->TimeCreatedMs, 1);
					memmove(&dirEntry->TimeCreated, &directoryEntry->TimeCreated, 2);
					memmove(&dirEntry->DateCreated, &directoryEntry->DateCreated, 2);
					memmove(&dirEntry->DateLastAccessed, &directoryEntry->DateLastAccessed, 2);
					memmove(&dirEntry->FirstClusterHiBytes, &directoryEntry->FirstClusterHiBytes, 2);
					memmove(&dirEntry->LastModTime, &directoryEntry->LastModTime, 2);
					memmove(&dirEntry->LastModDate, &directoryEntry->LastModDate, 2);
					memmove(&dirEntry->FirstCluster, &directoryEntry->FirstCluster, 2);
					memmove(&dirEntry->FileSize, &directoryEntry->FileSize, 4);

					diskBufferRelease(diskBuffer);
					lastIndex++;
					return 0;
				}

				directoryEntry++;
			}

			lastIndex = 0;
			diskBufferRelease(diskBuffer);
		}

		return -1;
	}

	return 0;
}

int closedir(int directoryDescriptor)
{
	Process* curproc = myProcess();

	if (curproc->OpenFile[directoryDescriptor] == 0)
	{
		return -1;
	}

	File* file = curproc->OpenFile[directoryDescriptor];
	curproc->OpenFile[directoryDescriptor] = 0;
	fileClose(file);

	return 0;
}