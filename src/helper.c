/*
 * helper.c
 *
 *  Created on: 26 Nov 2014
 *      Author: Brandon Birmingham
 */

#include "globals.h"
#include "performance.h"
#include "constants.h"
#include "carver.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <math.h>

/**
 * Zeroing an aray
 */
void bfd_init(float array[])
{
	int i;
	for(i = 0; i < BYTES_RANGE; i++)
		array[i] = 0;
}

/**
 * Since the ideal JPEG probability distribution must follow
 * uniform distribution because of compression, as we know that
 * we have 256 (0 - 255) distinct bytes the probability of 1 byte to occur is 1/256.
 * Hence each value in the histogram is set to 0.00390625 which is equivalent to 1/256.
 * The value was defined in the for-loop in order to prevent the division calculation every iteration
 */
void bfd_uniform(float array[])
{
	int i;
	for(i = 0; i < BYTES_RANGE; i++)
		array[i] = 0.00390625;
}

/**
 * Returning the sum of an array
 */
float get_sum(float numbers[], int size)
{
	int i;
	float total = 0;

	for(i = 0; i < size; i++)
	{
		total += numbers[i];
	}

	return total;
}

/**
 * Getting number of file types to match the centroid for a given uknown sector of bytes
 */
int get_number_of_centroids(char filename[])
{
	FILE * file = fopen(filename, "r");

	//number of files
	int i = 0;

	if(file != NULL)
	{
		//max length of file name
		char line[100];

		while(fgets(line, sizeof(line), file) != NULL)
			i++;
	}

	return i;
}

/**
 * Returns the number of files of a particular type in a particular folder
 */
int get_files_by_type(char * folder_name, char * file_type)
{
	DIR * folder;
	struct dirent * dir;
	int files_found = 0;

	folder = opendir(folder_name);

	if(folder)
	{
		while((dir = readdir(folder)) != NULL)
		{
			char * actual_file_name = dir -> d_name;
			char * file_ext = strchr(actual_file_name,'.');
			sprintf(file_ext, "%s", file_ext + 1);

			if(strcmp(file_ext, file_type) == 0)
				files_found++;
		}
	}
	else
	{
		printf("carver.get_files_by_type() - Failed to open folder %s", folder_name);
		return -1;
	}

	closedir(folder);
	return files_found;
}

/*
 * Checking if the the byte frequency distribution of the current unknown file type sector is a jpeg fragment or not
 * Parameters: sector    - unknown byte frequency distribution of the unknown sector
 *             means     - mean of each byte frequency for each possible file type
 *             variances - variance of each byte for each possible file type
 *Returns    : true if jpeg/false if not jpeg
 *
 */
Centroid get_centroid(float sector_bfd[], float means[num_file_types][SECTOR_SIZE], float variances[num_file_types][SECTOR_SIZE])
{
	//looping all centroids
	Centroid optimal_centroid;

	int type, byte, matched_type;
	float optimal_distance = -1;
	float current_distance;
	matched_type = 0;

	//traversing each centroid
	for(type = 0; type < num_file_types; type++)
	{
		current_distance = 0;

		//calculating distance between the unknown sector byte frequency distribution and with every possible centroid
		for(byte = 0; byte < BYTES_RANGE; byte++)
		{
			float byte_mean = means[type][byte];
			float sector_byte_mean = sector_bfd[byte];
			float variance = variances[type][byte];

			float distance_byte = (pow(byte_mean - sector_byte_mean, 2))/(variance+ CENTROID_SMOOTHING);
			current_distance += distance_byte;
		}

		//setting optimal distance
		if (type == 0 || current_distance < optimal_distance)
		{
			optimal_distance = current_distance;
			matched_type = type + 1;
		}
	}

	optimal_centroid.centroid_num = matched_type;
	optimal_centroid.centroid_dist = optimal_distance;

	return optimal_centroid;
}

/**
 * Computes the histogram difference between histogram_prev[] and histogram_next[]
 * Parameters
 */
float intersection_histogram(float hist_a[], float hist_b[])
{
	float intersection = 0;
	int j;

	for(j = 0; j < BYTES_RANGE; j++)
	{
		//computing histogram difference metric
		if (hist_a[j] < hist_b[j])
			intersection += hist_a[j];
		else
			intersection += hist_b[j];
	}

	return intersection;
}

void intersection_analyze(FILE * file_out, float hist_sector[], float hist_jpeg[])
{
	int j;

	for(j = 0; j < BYTES_RANGE; j++)
	{
		if(j == (BYTES_RANGE - 1))
			fprintf(file_out, "%.4f", hist_sector[j]);
		else
			fprintf(file_out, "%.4f,",hist_sector[j]);

	}

	float intersection = intersection_histogram(hist_sector, hist_jpeg);

	fprintf(file_out, "]) total : %f, intersection: %.4f\n", get_sum(hist_sector, BYTES_RANGE), intersection);
}

/*
 * Normalizing byte frequency distribution
 */
void normalize_bfd()
{
	int i;

	for(i = 0; i < BYTES_RANGE; i++)
	{
		bfd_cluster[i] = bfd_cluster[i]/SECTOR_SIZE;
	}
}

/*
 * Checks if the current found JPEG header is a non-thumbnail header
 * Parameters: sector         - sequence of byte values that contains a header of a jpeg
 *             start_position - the byte position from which the search begins
 * Returns   : true if non-thumbnail/false if thumnail header
 */
bool is_embedded_header(BYTE sector[], int start_position)
{
	int i;

	//traversing all the bytes found in the sector starting from <start_position>
	for(i = start_position; i < SECTOR_SIZE; i++)
	{
		BYTE first = sector[i];
		BYTE second = sector[i + 1];

		if (sector[start_position +1] == 0xdb)
		{
			return TRUE;
		}

		//if another JPEG header is encountered therefore it can be said that this is a non-thumbnail header
		if(first == 0xff && second == 0xd8)
		{
			fprintf(file_logs, "This is a valid header (non-thumbnail) since another ffd8 was encountered in the same sector\n");
			return FALSE;
		}

		//restart interval signifies a thumbnail
		else if(first == 0xff && second == 0xdd)
		{
			fprintf(file_logs, "This is a thumbnail header since a restart interval was found in this sector\n");
			return TRUE;
		}

		//if the first restart marker (Oxffd0) is to be found in this sector it can be assumed
		//that this is the first part of the thumbnail of the current JPEG that is being carved
		else if(first == 0xff && second == 0xd0)
		{
			fprintf(file_logs, "This is a thumbnail header since the first restart marker was found in this sector\n");
			return TRUE;
		}
	}

	//no need to validate header position
	if(flag_header_check == FALSE)
		return FALSE;

	else
		return TRUE;
}

int validate_jpeg(char * file_path, char * folder_actual)
{
	int found = 0;
	DIR * fa;
	struct dirent * dir;

	//opening directory that holds the actual files to be recovered
	fa = opendir(folder_actual);

	if(fa)
	{
		while((dir = readdir(fa)) != NULL)
		{
			char * actual_file_name = dir -> d_name;

			if(!(strcmp(actual_file_name, ".") == 0 || strcmp(actual_file_name, "..") == 0))
			{
				char actual_file_path[50];
				sprintf(actual_file_path, "%s%s%s", folder_actual, "\\", actual_file_name);

				//trying to match JPEG
				if(compare_two_binay_files(file_path, actual_file_path) == 1)
				{
					fprintf(file_jpegs_recovered, "- File %s matches exactly with %s\n", file_path, actual_file_path);
					closedir(fa);
					return 1;
				}
			}
		}

		closedir(fa);
		//fprintf(jpegs_partially_recovered, "- %s\n", file_path);
		return found;
	}
	else
	{
		printf("carver.validate_jpeg() - Failed to open folder %s", folder_actual);
		return -1;
	}
}

/*
 * Load centroids from a given file
 */
int load_centroids(char filename[], float values[num_file_types][SECTOR_SIZE], int rows, int columns)
{
	FILE * file = fopen(filename, "r");

	if(file != NULL)
	{
		char line[2000];
		int row   = 0;
		int col;

		//reading each line
		while(fgets(line, sizeof(line), file) != NULL)
		{
			col = 0;
			char * s = strtok(line, ",");
			printf("\n");
			while (s != NULL)
			{

				values[row][col] = atof(s);
				s = strtok(NULL, ",");
				col++;
			}

			row++;
		}
	}

	return 1;
}


/**
 * Loading file types from a given filename
 */
char ** load_types(char filename[])
{
	//opening file
	FILE * file = fopen(filename, "r");

	//pointer to pointer of characters which will hold the name
	//of every file type loaded from file
	char **filetypes;

	//number of files
	int i = 0;

	if(file != NULL)
	{
		//max length of file name
		char line[100];

		int n = get_number_of_centroids(filename);
		//reading each line
		filetypes = (char**)malloc(sizeof(char**)*n);

		//looping file names
		while(fgets(line, sizeof(line), file) != NULL)
		{
			//allocating memory for file name
			filetypes[i] = (char*) malloc(sizeof(char) * (strlen(line) + 1 ));

			//writing file name to memory
			strcpy(filetypes[i], line);
			i++;
		}
	}

	return filetypes;
}

/*
 *Printing centroids
 */
void print_centroids(float values[num_file_types][SECTOR_SIZE])
{
	int i,j;
	for (i = 0; i < num_file_types; i++)
	{
		for(j = 0; j < BYTES_RANGE; j++)
		{
			printf("%.2f ", values[i][j]);
		}

		printf("\n");
	}
}

void asserting_used_sectors_dfrws_2006()
{
	/*Asserting allocated sectors*/
	assert(is_sector_allocated(3868) == TRUE);
	assert(is_sector_allocated(3869) == TRUE);
	assert(is_sector_allocated(3870) == TRUE);
	assert(is_sector_allocated(3871) == TRUE);
	assert(is_sector_allocated(4428) == TRUE);
	assert(is_sector_allocated(4429) == FALSE);
	assert(is_sector_allocated(4430) == FALSE);
	assert(is_sector_allocated(8284) == FALSE);
	assert(is_sector_allocated(8285) == TRUE);
	assert(is_sector_allocated(8286) == TRUE);
	assert(is_sector_allocated(8287) == TRUE);
	assert(is_sector_allocated(9472) == TRUE);
	assert(is_sector_allocated(9473) == TRUE);
	assert(is_sector_allocated(9474) == FALSE);
	assert(is_sector_allocated(45566) == TRUE);
	assert(is_sector_allocated(45565) == FALSE);
	assert(is_sector_allocated(45387) == FALSE);
	assert(is_sector_allocated(36300) == TRUE);
	assert(is_sector_allocated(95628) == FALSE);
	assert(is_sector_allocated(94836) == TRUE);
	assert(is_sector_allocated(31533) == FALSE);
	assert(is_sector_allocated(31534) == FALSE);
	assert(is_sector_allocated(31753) == FALSE);
	assert(is_sector_allocated(36291) == FALSE);
	assert(is_sector_allocated(36292) == TRUE);
	assert(is_sector_allocated(36293) == TRUE);
	assert(is_sector_allocated(43433) == FALSE);
	assert(is_sector_allocated(43434) == TRUE);
	assert(is_sector_allocated(44028) == TRUE);
	assert(is_sector_allocated(44029) == FALSE);
}

void asserting_used_sectors_canon_gen6()
{
	/*Asserting allocated sectors - corresponds to secors in logs file*/
	assert(is_sector_allocated(13261) == TRUE);
	assert(is_sector_allocated(36077) == TRUE);
	assert(is_sector_allocated(42509) == TRUE);
	assert(is_sector_allocated(47455) == TRUE);
}

void asserting_embed_files()
{
	assert(is_file_embed("external/output/results/gen6/embed6.jpg") == TRUE);
	assert(is_file_embed("external/output/results/gen6/carve9.jpg") == FALSE);
	assert(is_file_embed("external/output/results/gen6/embed100.jpg") == TRUE);
}


