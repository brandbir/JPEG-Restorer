/*
 * filesystem.c
 *
 *  Created on: 14 Mar 2015
 *      Author: brandon
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesystem.h"
#include "constants.h"
#include "globals.h"
/*
 * This function is used to extract specific data retreived from ffstat
 * Parameters: file     - file where the extracted data is located
 *             property - property to be searched for ex cluster size/sector size
 */
int fs_get_value(FILE * file, const char * property)
{
	fseek(file, 0, SEEK_SET);

	size_t property_size = strlen(property);
	size_t line_size = 100;
	size_t length = 0;
	int i;
	int val = -1;
	char * line = (char *) malloc(line_size);
	char * substr = (char *) malloc(property_size);
	char * value = (char *)malloc(25);
	int found = 0;

	while (fgets(line, line_size, file) != NULL && !found)
	{
		length = strlen(line);
		if(length >= property_size)
		{
			for (i = 0; i < length - property_size; i++)
			{
				strncpy(substr, line+i, property_size);
				substr[property_size] = '\0';
				/*property match*/
				if (strcmp(substr, property) == 0)
				{
					int index_start = i+property_size+2;

					/*returing start of cluster only*/
					if (strcmp(property, FS_CLUSTER_AREA_START) == 0)
					{
						char * seperator = strchr(line, '-');
						int index_stop = seperator - line - 1;
						int length = index_stop - index_start;
						strncpy(value, line+index_start,length);
						value[length] = '\0';
					}
					else
					{
						strncpy(value, line+index_start, length);
						value[length] = '\0';
					}

					found = 1;
					val = atoi(value);
					break;
				}
			}
		}
	}

	/*memory deallocation*/
	free(line);
	free(substr);
	free(value);

	return val;
}
/**
 * This function is used to find the length of an encase header information
 * Parameters : encase disk image file pointer
 */
int fs_get_header_case_info_size (FILE * diskimage)
{
	BYTE * sector = (BYTE *) malloc(sizeof(BYTE) * SECTOR_SIZE);
	size_t bytes_read;

	int length = 0;
	int stop = 0;
	int i;

	while (( bytes_read = fread(sector, sizeof(BYTE), SECTOR_SIZE, diskimage) == SECTOR_SIZE) && !stop)
	{
		for (i = 0; i < SECTOR_SIZE; i++)
		{
			length++;
			if (sector[i] == 0x73 && sector[i+1] == 0x65 && sector[i+2] == 0x63 &&
				sector[i+3] == 0x74 && sector[i+4] == 0x6f && sector[i+5] == 0x72 &&
				sector[i+6] == 0x73)
			{
				stop = 1;
				/*length of string SECTORS*/
				length += 7;
				break;
			}
		}
	}

	fseek(diskimage, 0, SEEK_SET);

	//adding extra 64 after the string SECTORS should include all the header case info
	return length + 64;
}

/**
 * This function is used to discard CRCs from an encase disk image
 * and returns a new disk image without CRCs and ready for recovery
 * Parameters: diskimage    - original encase disk image
 *             size         - size of original disk image
 *             start_offset - offset of data region found int the disk image (excluding header)
 */
FILE * fs_discard_crc(FILE * diskimage, long size, int start_offset)
{
	size_t size_crc = 4;
	size_t size_md5 = 4;
	size_t size_block = 32*1024;	// 32KB
	size_t bytes_read;
	size_t bytes_written;

	/*Excluding footer of encase disk image - last CRC + MD5*/
	long end_offset = size - (size_crc + size_md5);

	char temp_disk_image [150];
	sprintf(temp_disk_image, "%s/disk_image_temp.raw", output_folder_name);

	FILE * diskimage_new = fopen(temp_disk_image, "w+");
	BYTE * block_data = (BYTE *) malloc(sizeof(BYTE) * size_block);
	int i;

	for (i = start_offset; i < end_offset; i += size_block)
	{
		fseek(diskimage, i,SEEK_SET);
		bytes_read = fread(block_data, sizeof(BYTE), size_block, diskimage);

		if (bytes_read == size_block)
		{
			i += size_crc;
			bytes_written = fwrite(block_data, sizeof(BYTE), size_block, diskimage_new);

			if(bytes_written != size_block)
			{
				puts("fs_discard_crc: Problem writing to the new encase disk image");
			}
		}
		else
		{
			puts("fs_discard_crc: Problem reading from original encase disk image");
			exit(ERROR);
		}
	}

	fseek(diskimage, 0, SEEK_SET);
	free(block_data);
	return diskimage_new;
}
