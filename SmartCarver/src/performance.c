/*
 * performance.c
 *
 *  Created on: 26 Nov 2014
 *      Author: Brandon
 */

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include "globals.h"
#include "helper.h"
#include "decoding.h"

void performance_metric(char * folder_produced, char * folder_actual)
{
	DIR	* folder;
	int number_of_jpegs = 0;
	struct dirent *dir;

	folder = opendir(folder_produced);

	if(folder)
	{
		while((dir = readdir(folder)) != NULL)
		{
			char * file_name = dir -> d_name;

			if(!(strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0))
			{
				number_of_jpegs++;
				char file_path[50];
				sprintf(file_path, "%s%s", folder_produced, file_name);

				//trying to decode JPEG
				if(decode_jpeg(file_path))
				{
					jpeg_recovered++;

					//check if the recovered JPEG file is found in the actual folder
					if(validate_jpeg(file_path, folder_actual) == 1)
						jpeg_found++;
				}

				else
				{
					jpeg_corrupted++;
				}
			}
		}

		closedir(folder);

		//carving statistics
		int actual_number_of_jpegs =  get_files_by_type("external\\files", "jpg");
		printf("JPEGs recovered  (decoded)                   : %d/%d\n", jpeg_recovered, number_of_jpegs);
		printf("JPEGs corrupted  (not decoded)               : %d/%d\n\n", jpeg_corrupted, number_of_jpegs);
		printf("Actual Number of JPEGs found to be recovered : %d\n", actual_number_of_jpegs);
		printf("JPEGs fully recovered                        : %d/%d\n", jpeg_found,actual_number_of_jpegs );
	}
	else
	{
		printf("[carver.performance_metric() : Folder %s cannot be opened", folder_produced);
	}
}

int compare_two_binay_files(char * filename_1, char * filename_2)
{
	FILE * file_1 = fopen(filename_1, "r");
	FILE * file_2 = fopen(filename_2, "r");

	if(file_1 != NULL && file_2 != NULL)
	{
		int file_1_size, file_2_size;
		char ch1, ch2;

		//moving pointer to the last byte found in both files
		fseek(file_1, 0, SEEK_END);
		fseek(file_2, 0, SEEK_END);

		//getting the offset of the last byte found both files
		file_1_size = ftell(file_1);
		file_2_size = ftell(file_2);

		if(file_1_size != file_2_size)
		{
			//they are not identical files
			fclose(file_1);
			fclose(file_2);
			return 0;
		}

		else
		{
			//compare each byte value of both files
			//setting pointer back to the starting position for both files
			fseek(file_1, 0, SEEK_SET);
			fseek(file_2, 0, SEEK_SET);

			while((ch1 = fgetc(file_1)) != EOF && (ch2 = fgetc(file_2) != EOF))
			{
				if(ch1 != ch2)
					return 0;
			}

			fclose(file_1);
			fclose(file_2);

			return 1;
		}
	}
	else
	{
		printf("filecompare.compare_two_binay_files() : When trying to open file %s or file %s, something went wrong", filename_1, filename_2);
		return -1;
	}
}

