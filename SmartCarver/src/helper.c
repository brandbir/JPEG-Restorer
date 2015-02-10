/*
 * helper.c
 *
 *  Created on: 26 Nov 2014
 *      Author: Brandon
 */


#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <math.h>
#include "globals.h"
#include "performance.h"

/**
 * Zeroing an aray
 */
void zero_array(float array[], int size)
{
	int i;
	for(i = 0; i < size; i++)
		array[i] = 0;
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
			char * file_ext= strchr(actual_file_name,'.');
			sprintf(file_ext,file_ext + 1);

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
 * Computes the histogram difference between histogram_prev[] and histogram_next[]
 * At this point in time, this function also prints the histogram values to the file
 */
void histogram_difference(FILE * file_out, float histogram_prev[], float histogram_next[])
{
	float intersection	= 0;
	int j;

	//looping both histograms
	for(j = 0; j <= 255; j++)
	{
		//computing histogram difference metric
		if(histogram_prev[j] < histogram_next[j])
			intersection += histogram_prev[j];
		else
			intersection += histogram_next[j];
	}

	for(j = 0; j <= 255; j++)
		fprintf(file_out, "%.4f ", histogram_prev[j]);

	fprintf(file_out, "]) total : %f, intersection: %.4f\n", get_sum(histogram_prev, 256), intersection);
}

/**
 * Writes a sector to the current carved JPEG file after validating the sector's probability distribution
 */
void histogram_statistics_write(float hist_sector[], float hist_jpeg[], BYTE cluster_buffer[], FILE * file_jpeg)
{

	float intersection	= 0;
	int j;

	//computing intersection between the current sector histogram
	//and the ideal jpeg histogram
	for(j = 0; j <= 255; j++)
	{
		if(hist_sector[j] < hist_jpeg[j])
			intersection += hist_sector[j];
		else
			intersection += hist_jpeg[j];
	}

	//This sector is not an entropy encoded sector therefore it should only be eliminated if start of scan is not encountered.
	//This is because only JPEG sectors containing the zigzagged frequency coefficients are entropy encoded. The first sequence of JPEG
	//sectors contain entirely the Huffman tables and the Quantization tables which are not entropy encode, but definitely need to be appended
	//to the opened JPEG file, even if they are not entropy encoded sectors
	if(intersection < ENTROPY_THRESHOLD)
	{
		//a previously encountered marker and is not in the current sector, therefore it means that we are waiting for an entropy coded
		//sector with a restart marker in the correct sequence
		if(last_marker != -1 && last_marker_sector != sector_index)
		{
			printf("Sector %d is not a JPEG (%d) - last marker: %d!\n", sector_index, last_marker, last_marker_sector);
			printf("Next sector needs to be a JPEG sector with a restart marker %d\n", last_marker + 1);

			//setting the next marker to be found in the next entropy coded sector
			next_marker = last_marker + 1;

			//stopping writing to file
			stop_write = 1;
		}
	}

	//writing sector
	if(stop_write == 0)
	{
		int offset;

		//writing sector to the opened jpeg file
		for(offset = write_start_offset; offset < write_end_offset; offset++)
			fwrite(&cluster_buffer[offset], sizeof(BYTE), sizeof(BYTE), file_jpeg);

		//file is to be closed
		if(file_opened == 2)
		{
			write_end_offset = SECTOR_SIZE;
			fclose(file_jpeg);
			file_opened = 0;
		}

		//setting write offset to the beginning
		write_start_offset = 0;
	}
}

/**
 * Performing Histogram Normalization
 */
void normalize_histogram(float histogram[], int hist_size)
{
	int i;
	for(i = 0; i <= hist_size; i++)
		histogram[i] /= 512;
}

int validated_header_position(BYTE cluster_buffer[], int start_position)
{
	int i;

	//no need to validate header position
	if(check_header == 0)
		return 1;

	for(i = start_position; i < SECTOR_SIZE; i++)
	{
		BYTE current = cluster_buffer[i];
		BYTE next    = cluster_buffer[i + 1];

		//if another JPEG header is encountered this sector will
		//obviously not contain the thumbnail of the current jpeg that
		//is being recovered
		if(current == 0xff && next == 0xd8)
		{
			printf("validated_header: ffd8\n");
			return 1;
		}

		//restart interval signifies a thumbnail
		else if(current == 0xff && next == 0xdd)
		{
			printf("validated_header: ffdd\n");
			return 0;
		}

		//if the first restart marker (Oxffd0) is
		//to be found in this sector it can be assumed
		//that this is the first part of the thumbnail
		//of the current JPEG that is being carved
		else if(current == 0xff && next == 0xd0)
		{
			printf("validated_header: ffd0\n");
			return 0;
		}
	}

	return 0;
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
					fprintf(jpegs_recovered, "- File %s matches exactly with %s\n", file_path, actual_file_path);
					closedir(fa);
					return 1;
				}
			}
		}

		closedir(fa);
		fprintf(jpegs_partially_recovered, "- %s\n", file_path);
		return found;
	}
	else
	{
		printf("carver.validate_jpeg() - Failed to open folder %s", folder_actual);
		return -1;
	}
}

/**
 * Since the ideal JPEG probability distribution must follow
 * uniform distribution because of compression, as we know that
 * we have 256 (0 - 255) distinct bytes the probability of 1 byte to occur is 1/256.
 * Hence each value in the histogram is set to 0.00390625 which is equivalent to 1/256.
 * The value was defined in the for-loop in order to prevent the division calculation every iteration
 */
void set_jpeg_uniform_distribution(float array[], int size)
{
	int i;
	for(i = 0; i < size; i++)
		array[i] = 0.00390625; // 1 / 256
}

