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

void zero_array(float array[], int size)
{
	int i;
	for(i = 0; i < size; i++)
		array[i] = 0;
}

int get_files_by_type(char * folder_name, char * file_type)
{
	DIR * fa;
	struct dirent * dir;
	int files_found = 0;

	fa = opendir(folder_name);

	if(fa)
	{
		while((dir = readdir(fa)) != NULL)
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

	closedir(fa);
	return files_found;
}

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

void histogram_statistics_write(float histogram_prev[], float histogram_next[], BYTE cluster_buffer[], FILE * file_jpeg)
{

	float intersection	= 0;
	int j;
	int header_footer = 0;

	for(j = 0; j <= 255; j++)
	{
		if(histogram_prev[j] < histogram_next[j])
			intersection += histogram_prev[j];
		else
			intersection += histogram_next[j];
	}

	fprintf(file_out, "%d - Intersection %f - last marker %d\n", cluster_index, intersection, last_marker);

	if(intersection < 0.10)
		printf("Sector %d is not a JPEG!\n", cluster_index);

	//Writing the header/footer to the current opened file
	if(temp_intersection == -1)
	{
		int offset;

		//writing sector to the opened file
		for (offset = write_start_offset; offset < write_end_offset; offset++)
			fwrite(&cluster_buffer[offset], sizeof(BYTE), sizeof(BYTE), file_jpeg);

		//checking if file needs to be closed
		if(file_opened == 2)
		{
			write_end_offset = 512;
			fclose(file_jpeg);
			file_opened = 0;
		}

		header_footer = 1;
		write_start_offset = 0;
	}

	//not header/footer
	//dfrws:2006 challenge - 0.55
	else if(fabs(temp_intersection - intersection) > 0.55)
	{
		/*
		 * if there was a rapid change in the histogram of the sectors
		 * switch writing mode (this means that if the carver was carving a particular
		 * file it needs to stop carving the current file and stops for an other rapid change
		 * to continue from the stopping point
		 */

		if(stop_write == 1)
			stop_write = 0;
		else
			stop_write = 1;

		printf("Last Marker: %d, Rapid Change in Histograms detected at sector %d [Previous %f - Current %f] \n", last_marker, cluster_index, temp_intersection, intersection);
	}

	//This needs to be executed if and only if the write operation is enabled given
	//that no rapid change was encountered between two successive histograms and also
	//that the current sector is not a fragment of a header/footer
	if(stop_write == 0 && header_footer == 0)
	{
		int offset;
		for(offset = write_start_offset; offset < write_end_offset; offset++)
			fwrite(&cluster_buffer[offset], sizeof(BYTE), sizeof(BYTE), file_jpeg);

		if(file_opened == 2)
		{
			write_end_offset = 512;
			fclose(file_jpeg);
			file_opened = 0;
		}

		write_start_offset = 0; // can be deleted!!
	}

	temp_intersection = intersection;
}


void normalize_histogram(float histogram[], int hist_size)
{
	int i;
	for(i = 0; i <= hist_size; i++)
		histogram[i] /= 512;
}

int validated_header_position(BYTE cluster_buffer[], int start_position)
{
	int i;

	//no need to validate
	if(check_header == 0)
		return 1;

	for(i = start_position; i < cluster_size; i++)
	{
		BYTE current = cluster_buffer[i];
		BYTE next    = cluster_buffer[i + 1];

		//if another JPEG header is encountered this sector will
		//obviously not contain the thumbnail of the current jpeg that
		//is being recovered
		if(current == 0xff && next == 0xd8)
			return 1;

		else if(current == 0xff && next == 0xdd)
			return 0;

		//if the first restart marker (oxffd0) is
		//to be found in this sector it can be assumed
		//that this is the first part of the thumbnail
		//of the current JPEG that is being carved
		else if(current == 0xff && next == 0xd0)
			return 0;
	}

	return 0;
}

int validate_jpeg(char * file_path, char * folder_actual)
{
	int found = 0;
	DIR * fa;
	struct dirent * dir;

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

