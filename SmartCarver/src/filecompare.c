/*
 ============================================================================
 Name        : filecompare.c
 Author      : Brandon Birmingham
 Description : Offers the functionality of comparing binary files
 Created On  : 13 October 2014
 ============================================================================
 */

#include <stdio.h>
#include "filecompare.h"

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



