/*
 * performance.c
 *
 *  Created on: 26 Nov 2014
 *      Author: Brandon Birmingham
 */

#include "globals.h"
#include "helper.h"
#include "decoding.h"
#include "carver.h"
#include "jpegtools.h"

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#include <opencv2/highgui/highgui.hpp>

void delete_directory(char * directory)
{
	struct dirent *dir;
	DIR * folder = opendir(directory);

	if(folder)
	{
		while((dir = readdir(folder)) != NULL)
		{
			char * file_name = dir -> d_name;

			char name[200];
			sprintf(name, "%s%s%s", directory, "/", file_name);

			if(!(strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0))
			{
				struct stat s;

				if(stat(name, &s) == 0)
				{
					if( s.st_mode & S_IFDIR )
					{
						delete_directory(name);
					}
					else if( s.st_mode & S_IFREG )
					{
						int file_del = unlink(name);

						if(file_del == ERROR)
							printf("FilE %s cannot be deleted", name);
					}
				}
			}
		}

		closedir(folder);
		rmdir(directory);
	}
	else
	{
		printf("delete_directory() - Directory cannot be found\n");
	}
}

int create_directory(char * directory)
{
	int ret = SUCCESS;
	errno = 0;
	mkdir(directory, S_IRWXU);

	if(errno == EEXIST)
	{
		printf("Directory %s is going to be replaced\n", directory);
		delete_directory(directory);

		mkdir(directory, S_IRWXU);

		/*waiting for directory to be available*/
		while(errno == EACCES)
			mkdir(directory,S_IRWXU);

		if (errno != EEXIST)
		{
			printf("Directory %s was not created\n", directory);
			ret = ERROR;
		}
	}
	else
	{
		ret = ERROR;
	}

	return ret;
}

/**
 * This function is used to decode the embedded JPEGs
 * and checks if they are recovered/fragmented/corruptes
 * Parameters folder - the directory where the embedded jpeg files reside
 */
void decode_embedded(DIR * folder)
{
	fprintf(file_logs, "\nFinding embedded JPEG images\n");
	printf("Finding embedded JPEG images\n");

	struct dirent * dir;

	/*Traverse the directory*/
	while((dir = readdir(folder)) != NULL)
	{
		char * file_name = dir -> d_name;

		/*Checks that this is an embedded jpeg file*/
		if (strstr(file_name, "embed") != NULL)
		{
			char file_path[50];
			sprintf(file_path, "%s%s%s", output_folder_name, "/", file_name);

			decoder_result decode_check;
			decode_check = decode_jpeg(file_path, FALSE, NULL);

			char new_file_path[100];

			/*Moving the embedded jpeg file according to the decoding result*/
			if (decode_check.decoder_type == SUCCESS)
			{
				num_jpeg_recovered++;
				sprintf(new_file_path, "%s%s%s", folder_recovered_images, "/", file_name);
				fprintf(file_logs,"%s was recovered successfuly\n", file_path);
			}

			else if (decode_check.decoder_type == FRAGMENTED)
			{
				num_jpeg_fragmented++;

				sprintf(new_file_path, "%s%s%s", folder_fragmented_images, "/", file_name);
				fprintf(file_logs, "%s is fragmented - decoder sector %d - decoder scanline %d\n",
						file_path, decode_check.decoder_sector, decode_check.decoder_scanline);
			}

			else if (decode_check.decoder_type == ERROR)
			{
				num_jpeg_corrupted++;
				fprintf(file_logs, "%s is corrupted\n", file_path);
				sprintf(new_file_path, "%s%s%s", folder_corrupted_images, "/", file_name);
			}

			rename(file_path, new_file_path);
		}
	}

	rewinddir(folder);
}

bool is_dir(char * file_name)
{
	if (strcmp(file_name, "corrupted") == 0 ||
		strcmp(file_name, "decoded") == 0   ||
		strcmp(file_name, "logs")    == 0   ||
		strcmp(file_name, ".") == 0         ||
		strcmp(file_name, "..") == 0)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}
void analyze_fully_recovered(DIR * folder)
{
	fprintf(file_logs, "\nFinding fully-recovered JPEG images\n");
	printf("Finding fully-recovered JPEG images\n");

	struct dirent * dir;

	while ((dir = readdir(folder)) != NULL)
	{
		char * file_name = dir -> d_name;

		if (!is_dir(file_name))
		{
			char file_path[50];
			sprintf(file_path, "%s%s%s", output_folder_name, "/", file_name);

			decoder_result decode_check = decode_jpeg(file_path, FALSE, NULL);
			char new_file_path[100];

			/*trying to decode JPEG*/
			if (decode_check.decoder_type == SUCCESS)
			{
				if (!is_file_embed(file_path))
				{
					long add_header = get_header_offset(file_path);
					int ret = add_allocations(add_header);

					if (ret == SUCCESS)
						num_sectors_used++;
				}

				num_jpeg_recovered++;
				sprintf(new_file_path, "%s%s%s", folder_recovered_images, "/", file_name);
				fprintf(file_logs,"%s was recovered successfuly\n", file_path);
				rename(file_path, new_file_path);
			}
		}
	}
}

void analyze_fragmented_corrupted(DIR * folder)
{
	fprintf(file_logs, "\nFinding fragmented and corrupted JPEG images\n");
	printf("Finding fragmented and corrupted JPEG images\n");

	struct dirent * dir;

	while ((dir = readdir(folder)) != NULL)
	{
		char * file_name = dir -> d_name;

		if (!is_dir(file_name))
		{
			char file_path[50];
			sprintf(file_path, "%s%s%s", output_folder_name, "/", file_name);

			FILE * fragmented_file = fopen(file_path, "rw");

			/*Get the corresponding thumbnail of the current image*/
			Thumbnail thumbnail_object = get_thumbnail(file_path);

			#if DECODER_THUMBNAIL_DEBUG
				fprintf(file_logs, "%s vs %s\n", file_path, thumbnail_object.path);
				fprintf(file_logs, "Line\tSector\tMAE\tMSE\n");
			#endif

			decoder_result decode_check = decode_jpeg(file_path, FALSE, thumbnail_object.image);

			char new_file_path[100] = "";

			if (decode_check.decoder_type == FRAGMENTED)
			{
				num_jpeg_fragmented++;

				fprintf(file_logs, "%s is fragmented (Decoder)   - sector: %d; line: %d\n", file_path, decode_check.decoder_sector, decode_check.decoder_scanline);
				fprintf(file_logs, "%s is fragmented (Thumbnail) - sector: %d; line: %d\n", file_path, decode_check.thumb_sector, decode_check.thumb_scanline);
				sprintf(new_file_path, "%s%s%s", folder_fragmented_images, "/", file_name);

				//todo: uncomment for enabling edge detection
				//decode_jpeg(file_path, TRUE, NULL);

				char smart_carve_file[150];
				sprintf(smart_carve_file, "%s/%s.jpg", output_folder_name, file_to_smart_carve);

				if (strcmp(file_path, smart_carve_file) == 0 || strcmp(file_to_smart_carve, "a") == 0)
				{
					if (smart_carving_flag)
					{
						long header_offset = get_header_offset(file_path);
						printf("SmartCarving %s...\n", file_path);
						arrange_fragmented_file(fragmented_file, decode_check, header_offset, thumbnail_object, UKNOWN, 1);
					}
				}
			}

			else if (decode_check.decoder_type == ERROR)
			{
				num_jpeg_corrupted++;
				sprintf(new_file_path, "%s%s%s", folder_corrupted_images, "/", file_name);
				fprintf(file_logs, "%s is corrupted\n", file_path);
			}

			if (thumbnail_object.image != NULL)
				cvRelease((void *) &thumbnail_object.image);

			/*moving file in the specific folder*/
			if (strcmp(new_file_path, "") != 0)
				rename(file_path, new_file_path);
		}
	}
}

void print_used_sectors()
{
	fprintf(file_sectors_used, "Sectors used of fully-recovered images:\n");

	int i;
	for (i = 0; i < (num_sectors_used)*2; i+=2)
	{
		long head = g_array_index(g_recovered, glong, i);
		long foot = g_array_index(g_recovered, glong, i+1);
		fprintf(file_sectors_used, "%lu - %lu\n", head, foot);
	}
}

void analyze_recovered_images(char * folder_actual)
{
	printf("Analyzing images...\n");
	DIR	* folder;

	folder = opendir(output_folder_name);

	sprintf(folder_corrupted_images, "%s%s", output_folder_name, "/corrupted");
	sprintf(folder_decoded_images, "%s%s", output_folder_name, "/decoded");
	sprintf(folder_fragmented_images, "%s%s", folder_decoded_images, "/fragmented");
	sprintf(folder_recovered_images, "%s%s", folder_decoded_images, "/recovered");

	create_directory(folder_decoded_images);
	create_directory(folder_corrupted_images);
	create_directory(folder_fragmented_images);
	create_directory(folder_recovered_images);

	if (folder)
	{
		decode_embedded(folder);
		analyze_fully_recovered(folder);

		closedir(folder);
		folder = opendir(output_folder_name);
		g_array_sort(g_recovered, (gconstpointer)compared_long);

		print_used_sectors();
		analyze_fragmented_corrupted(folder);

		int number_of_jpegs = 0;
		number_of_jpegs = num_jpeg_corrupted + num_jpeg_fragmented + num_jpeg_recovered;

		closedir(folder);
		printf("-----------------------------------------------------------------------\n");
		printf("Carving Results:\n");
		printf("Entopy Threshold used                         : %.2f\n", threshold_entropy);
		printf("Number of sectors traversed                   : %lu\n",cluster_index);
		printf("MB Traversed                                  : %.2f\n", (double)((cluster_index * SECTOR_SIZE) / (1024*1024)));
		printf("Sector Size                                   : %d\n", fs_sector_size);
		printf("Cluster Size                                  : %d\n", fs_cluster_size);
		printf("Total JPEG headers found                      : %d\n", jpeg_headers);
		printf("Total JPEG footers found                      : %d\n", jpeg_footers);

		printf("\nJPEGs embedded                                : %d/%d\n", num_jpeg_embed, number_of_jpegs);
		printf("JPEGs corrupted                               : %d/%d\n", num_jpeg_corrupted, number_of_jpegs);
		printf("JPEGs fragmented                              : %d/%d\n", num_jpeg_fragmented, number_of_jpegs);
		printf("JPEGs fully recovered                         : %d/%d\n", num_jpeg_recovered, number_of_jpegs);
		printf("JPEGs smart-carved                            : %d/%d\n", num_jpeg_arranged, number_of_jpegs);
		printf("-----------------------------------------------------------------------\n");
	}
	else
	{
		printf("[carver.performance_metric() : Folder %s cannot be opened", output_folder_name);
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

