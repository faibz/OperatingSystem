#include "types.h"
#include "user.h"
#include "fs.h"

bool fullDetailsFlagPresent(char** argv, int argc);
bool recursiveFlagPresent(char** argv, int argc);
int getSpecifiedDirectoryIndex(char** argv, int argc);
void printFileNameAndExtension(struct _DirectoryEntry* directoryEntry);
void processDirectoryEntry(int directoryDescriptor, bool fullDetails, bool recursive, char* startDirectory);
int readFileAttributes(int attributes, char* buffer);
int readFileDate(int date, int column);
int readFileTime(int time, int column);

char* fileAttributes[8] = { "Read Only", "Hidden", "System", "VolumeLabel", "Subdirectory", "Archive", "Device", "Unused" };

int main(int argc, char *argv[])
{
	if (argc > 4)
	{
		return -1;
	}

	int specifiedDirectoryIndex = getSpecifiedDirectoryIndex(argv, argc);
	int directoryDescriptor = specifiedDirectoryIndex == -1 ? opendir(" ") : opendir(argv[specifiedDirectoryIndex]);
	
	if (directoryDescriptor == 0)
	{
		exit();
	}

	processDirectoryEntry(directoryDescriptor, fullDetailsFlagPresent(argv, argc), recursiveFlagPresent(argv, argc), specifiedDirectoryIndex == - 1 ? " " : argv[specifiedDirectoryIndex]);

	if (closedir(directoryDescriptor) < 0)
	{
		printf("An error occurred when trying to close directory with id: %d", directoryDescriptor);
	}

	exit();

	return 0;
}

bool fullDetailsFlagPresent(char** argv, int argc)
{
	for(int i = 1; i < argc; ++i)
	{
		if (argv[i][0] == '-' && argv[i][1] == 'l') 
		{
			return true;
		}
	}

	return false;
}

bool recursiveFlagPresent(char** argv, int argc)
{
	for(int i = 1; i < argc; ++i)
	{
		if (argv[i][0] == '-' && argv[i][1] == 'R') 
		{
			return true;
		}
	}

	return false;
}

int getSpecifiedDirectoryIndex(char** argv, int argc)
{
	for(int i = 1; i < argc; ++i)
	{
		if (argv[i][0] != '-') return i;
	}

	return -1;
}

void printFileNameAndExtension(struct _DirectoryEntry* directoryEntry)
{
	char fileName[9] = {0};
	char fileExtension[4] = {0};
	char fileNameAndExtension[13] = {0};

	for (int i = 0; i < 8; ++i)
	{
		if (directoryEntry->Filename[i] == ' ')
		{
			break;
		}

		fileName[i] = directoryEntry->Filename[i];
	}

	fileName[8] = 0;

	for (int i = 0; i < 3; ++i)
	{
		if (directoryEntry->Ext[i] == ' ')
		{
			break;
		}

		fileExtension[i] = directoryEntry->Ext[i];
	}

	fileExtension[3] = 0;

	memmove(fileNameAndExtension, fileName, strlen(fileName));

	if (fileExtension[0] != 0) 
	{
		fileNameAndExtension[strlen(fileName)] = '.';
	}

	memmove(fileNameAndExtension + strlen(fileName) + 1, fileExtension, strlen(fileExtension));
	fileNameAndExtension[strlen(fileNameAndExtension)] = 0;
	printf("%s ", fileNameAndExtension);
}

void processDirectoryEntry(int directoryDescriptor, bool fullDetails, bool recursive, char* startDirectory)
{
	struct _DirectoryEntry* directoryEntry = malloc(32);

	while (readdir(directoryDescriptor, directoryEntry) >= 0)
	{
		int isSubdir = (directoryEntry->Attrib >> 4) & 1;
		int isArchive = (directoryEntry->Attrib >> 5) & 1;

		if (isSubdir != 1 && isArchive != 1)
		{
			continue;
		}

		printFileNameAndExtension(directoryEntry);

		if (fullDetails)
		{
			char attributeBuffer[100] = {0};
			readFileAttributes(directoryEntry->Attrib, attributeBuffer);

			int times[6] = {0};

			for(int i = 5; i >= 0; --i)
			{
				int time = i >= 3 ? directoryEntry->LastModTime : directoryEntry->TimeCreated;
				times[5 - i] = readFileTime(time, i % 3);
			}

			//adds the 'missing' second if timecreated is over 100 (1s)
			if (directoryEntry->TimeCreatedMs >= 100)
			{
				++times[2];
				directoryEntry->TimeCreatedMs -= 100;
			}

			//%s before %d for times adds a "0" if necessary
			printf("Created: %d/%d/%d %s%d:%s%d:%s%d.%d. Last mod: %d/%d/%d %s%d:%s%d:%s%d. Size: %d bytes. Attributes: %s", 
				readFileDate(directoryEntry->DateCreated, 0), readFileDate(directoryEntry->DateCreated, 1), 
				readFileDate(directoryEntry->DateCreated, 2), times[0] >= 10 ? "" : "0", times[0],
				times[1] >= 10 ? "" : "0", times[1], times[2] >= 10 ? "" : "0", times[2],
				directoryEntry->TimeCreatedMs * 10, 
				readFileDate(directoryEntry->LastModDate, 0), readFileDate(directoryEntry->LastModDate, 1), 
				readFileDate(directoryEntry->LastModDate, 2), times[3] >= 10 ? "" : "0", times[3], 
				times[4] >= 10 ? "" : "0", times[4], times[5] >= 10 ? "" : "0", times[5],
				directoryEntry->FileSize, attributeBuffer);
		}

		printf("\n");
		
		if (recursive && directoryEntry->FileSize == 0 && directoryEntry->Attrib == 0b00010000 && directoryEntry->Filename[0] != '.') // 5th bit = subdir -> 0b00010000/0x10/16
		{
			char fileName[255] = {0};

			if (*startDirectory != ' ')
			{
				memmove(fileName, startDirectory, strlen(startDirectory));

				if(fileName[strlen(startDirectory) - 1] != '\\' && fileName[strlen(startDirectory) - 1] != '/')
				{
					fileName[strlen(startDirectory)] = '/';
				}
			}

			memmove(fileName + strlen(fileName), directoryEntry->Filename, 8);

			for (int i = strlen(fileName) - 1; i >= 0; --i)
			{
				if (fileName[i] != ' ')
				{
					break;
				}

				fileName[i] = 0;
			}

			printf("{\n");
			int recDirDesc = opendir(fileName);
			processDirectoryEntry(recDirDesc, fullDetails, true, fileName);
			closedir(recDirDesc);
			printf("}\n");
		}
	}
}

int readFileAttributes(int attributes, char* buffer)
{
	//method to check which bits are present in an int and then puts the string version of the attribute into a buffer to be displayed
	for(int i = 0; i < 8; ++i)
	{
		int bitActive = (attributes >> i) & 1;
		if (bitActive == 1) 
		{
			memmove(buffer + strlen(buffer), fileAttributes[i], strlen(fileAttributes[i]));
			buffer[strlen(buffer)] = ',';
		}
	} 

	if (strlen(buffer) > 0) 
	{
		buffer[strlen(buffer) - 1] = '.';
	}

	return 0;
}

int readFileDate(int date, int column)
{
	//2 bytes - bits 0-4: day (1-31) | bits 5-8: month (1-12) | bits 9-15: year from 1980 (21 -> 2001)
	//method to get the value of a number of consecutive bits from an integer

	int bitsToExtract = 0;
	int position = 1;

	if (column == 0) //days
	{
		bitsToExtract = 5;
	}
	else if (column == 1) //month
	{
		bitsToExtract = 4;
		position = 6;
	}
	else if (column == 2) //year
	{
		bitsToExtract = 7;
		position = 10;
	}

	int value = (((1 << bitsToExtract) - 1) & (date >> (position - 1)));

	return column == 2 ?  1980 + value : value;
}

int readFileTime(int time, int column)
{
	//2 bytes - bits 0-4: seconds (0-29 [2 second accuracy so 29 -> 58]) | bits 5-10: minutes (0-59) | bits 11-15: hours (0-23)
	//same as readFileDate

	int bitsToExtract = 0;
	int position = 1;

	if (column == 0) //seconds
	{
		bitsToExtract = 5;
	}
	else if (column == 1) //minutes
	{
		bitsToExtract = 6;
		position = 6;
		
	}
	else if (column == 2) //hours
	{
		bitsToExtract = 5;
		position = 12;
	}

	int value = (((1 << bitsToExtract) - 1) & (time >> (position - 1)));

	return column == 0 ? value * 2 : value;
}