/*
 ============================================================================
 Name        : carver.c
 Author      : Brandon Birmingham
 Description : SmartCarver based on File Structure implemented in C
 Created On  : 20 September 2014
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "performance.h"
#include "helper.h"

int total_bytes = 0;					// the total number of bytes read from the disk image
int jpeg_headers = 0;					// total number of JPEG headers encountered
int jpeg_footers = 0;					// total number of JPEG footers encountered
float histogram_jpeg_dist[256];			// array containing the count of bytes found in the range between 0 and 255
float histogram_current_sector[256];	// temporary histogram in order to find the correlation between the two consecutive histograms

const char * file_name;
const char * output_folder_name;

FILE * file_jpeg = NULL;
FILE * file_in;

clock_t start_timer		= 0;
clock_t stop_timer		= 0;
double carving_time		= 0;

char file_jpeg_name[30];

int handle_jpeg_header(BYTE cluster_buffer[], int byte_offset)
{
	//TODO this needs to eventually be removed. This is set because when image is not totally recovered successfully
	//the check header will not be set accordingly
	if(byte_offset == 0)
		check_header = 0;

	//eliminating the possibility of finding two headers in the same cluster
	//and also handling the addition of EXIF Thumbnails to the current carving JPEG image

	//TODO : validated_header_position was removed for the datasets nps-2009-canon2-gen1.raw
	if(skip_next_header_exif != 1 && validated_header_position(cluster_buffer, byte_offset + 2))
	{
		//setting the exact byte position of the header found
		last_jpeg_header_file_offset = (cluster_index * SECTOR_SIZE) + byte_offset;

		//updating the number of JPEG headers found
		jpeg_headers++;
		header_offset = cluster_index;

		//creating jpeg's file name
		sprintf(file_jpeg_name, "%s%d%s", output_folder_name, jpeg_headers, ".jpg");

		printf("\nHeader %d found at Sector: %d\n", jpeg_headers, cluster_index);
		fprintf(file_logs, "Header %d found at Sector: %d\n\n", jpeg_headers, cluster_index);
		write_start_offset = byte_offset;

		//creating file for the carved JPEG
		file_jpeg = fopen(file_jpeg_name, "wb");
		file_opened = 1;

		if(file_jpeg == NULL)
			puts("File was not created for the reconstruction of jpeg file");

		else
			puts("Writing to file...");

		//checking next header position in case a thumbnail header is encountered before finding a footer for this header
		check_header = 1;
	}
	else
	{
		//Given that this is a thumbnail header, therefore you need to ignore the next footer,
		//since the next footer will be that of the thumbnail
		skip_next_footer = 1;
	}

	return byte_offset;
}

int handle_jpeg_footer(int byte_offset)
{
	//found a footer
	if(skip_next_footer)
	{
		skip_next_footer = 0;
	}
	else
	{
		jpeg_footers++;
		last_jpeg_footer = cluster_index;
		last_jpeg_footer_offset = (cluster_index * 512) + byte_offset;

		printf("\nFooter %d found at Sector: %d (%d)\n", jpeg_footers, cluster_index, last_marker);
		fprintf(file_logs, "Footer %d found at Sector: %d\n", jpeg_footers, cluster_index / SECTOR_SIZE);

		//prepared for closing the file
		file_opened = 2;

		//adding extra 1 for less than equivalence
		write_end_offset = byte_offset + 2;
		temp_intersection = -1;

		//Once we find the actual footer of the JPEG, the exif flag must be restarted
		skip_next_header_exif = 0;
		check_header = 0;
	}

	return byte_offset;
}

void handle_exif_marker()
{
	//If EXIF application marker is encountered we need to allow all the
	//following JPEG headers to be added to the current opened file that
	//is being used for recovery until we find the other exif application marker

	if(skip_next_header_exif)
		skip_next_header_exif = 0;
	else
		skip_next_header_exif = 1;
}

int handle_marker(BYTE cluster_buffer[], int byte_offset)
{
	//JPEG file marker is found
	int changed = 0;
	int current_marker = last_marker;
	BYTE next_byte [1];
	//if 0xff is found in the last byte of the sector we need to check the next byte

	if(byte_offset == (SECTOR_SIZE - 1))
	{
		int bytes_read = fread(next_byte, sizeof(unsigned char), 1, file_in);

		if(bytes_read == 1)
			fseek(file_in, -1, SEEK_CUR);
	}
	else
	{
		next_byte[0] = cluster_buffer[byte_offset + 1];
	}

	switch(next_byte[0])
	{
		case 0xd0:
			current_marker = 0;
			changed = 1;
			break;

		case 0xd1:
			current_marker = 1;
			changed = 1;
			break;

		case 0xd2:
			current_marker = 2;
			changed = 1;
			break;

		case 0xd3:
			current_marker = 3;
			changed = 1;
			break;

		case 0xd4:
			current_marker = 4;
			changed = 1;
			break;

		case 0xd5:
			current_marker = 5;
			changed = 1;
			break;

		case 0xd6:
			current_marker = 6;
			changed = 1;
			break;

		case 0xd7:
			current_marker = 7;
			changed = 1;
			break;
	}

	if((current_marker != -1) && changed && ((current_marker - last_marker != 1) && current_marker != 0))
	{
		printf("offset %d) current_marker : %d - last_marker: %d\n", (cluster_index*512) + byte_offset, current_marker, last_marker);
		printf("Skipping this sector due to invalid marker..\n");
		write_sector = 0;
	}
	else
	{
		last_marker = current_marker;
	}

	return byte_offset;
}

void write_sector_to_file(BYTE cluster_buffer[], int bytes_read)
{
	//checking if writing sector to file is enabled
	if(write_sector)
	{
		//Normalising sector's histogram values
		normalize_histogram(histogram_current_sector, (int)(sizeof(histogram_current_sector) / sizeof(histogram_current_sector[0])));

		fprintf(file_out, "\n\n");
		total_bytes += bytes_read;

		//printing histogram for each sector read from disk image
		fprintf(file_histograms, "Sector : %d - bar([0:255], [", cluster_index);

		int j;
		if(cluster_index == 0)
		{
			for(j = 0; j <= 255; j++)
				fprintf(file_histograms, "%.4f ", histogram_current_sector[j]);

			fprintf(file_histograms, "]) %f \n", get_sum(histogram_current_sector, 256));

		}
		else
		{
			histogram_difference(file_histograms, histogram_current_sector, histogram_jpeg_dist);
		}

		if(file_opened != 0)
		{
			histogram_statistics_write(histogram_current_sector, histogram_jpeg_dist, cluster_buffer, file_jpeg);
		}
	}
	else
	{
		//writing sector to file enabled
		write_sector = 1;
	}
}

int finalize()
{
	printf("\nCarving Results:\n");
	printf("Total JPEG headers found                     : %d\n", jpeg_headers);
	printf("Total JPEG footers found                     : %d\n", jpeg_footers);
	printf("Total bytes read                             : %d bytes\n", total_bytes);

	performance_metric((char *)output_folder_name, "external\\files");

	stop_timer = clock();
	carving_time = (double)(stop_timer - start_timer) / CLOCKS_PER_SEC;

	printf("Execution time %.2f sec / %.2f mins", carving_time, carving_time/60);
	fclose(file_in);
	fclose(file_out);
	fclose(file_histograms);
	fclose(jpegs_recovered);
	fclose(jpegs_partially_recovered);
	fclose(file_logs);
	return EXIT_SUCCESS;
}

int main(int argc, char * argv[])
{
	//setting timer
	start_timer = clock();

	//no buffer for stdout
	setvbuf (stdout, NULL, _IONBF, 0);

	file_name = argv[1];
	output_folder_name = argv[2];

	struct stat info;

	BYTE cluster_buffer[SECTOR_SIZE];		// holds the sequence of bytes fore each cluster read


	//setting the ideal uniform distribution of JPEG histogram
	set_jpeg_uniform_distribution(histogram_jpeg_dist, sizeof(histogram_jpeg_dist) / sizeof(histogram_jpeg_dist[0]));

	//zeroing the histogram array for the first sector
	zero_array(histogram_current_sector, sizeof(histogram_current_sector) / sizeof(histogram_current_sector[0]));

	if(stat(file_name, &info) != 0)
		puts("stat failed to retrieve file details");

	puts("SmartCarver initialisation...");
	puts("Initialising Histogram values...");

	int total_clusters = ceil(info.st_size / SECTOR_SIZE);
	printf("Number of clusters found : %d\n", total_clusters);

	//opening file in read and binary mode
	file_in = fopen(file_name, "rb");
	file_out = fopen("external/output/out.txt", "w");


	jpegs_recovered = fopen("external/output/jpegs_recovered.txt", "w");
	jpegs_partially_recovered = fopen("external/output/jpegs_partially_recovered.txt", "w");
	file_histograms = fopen("external/output/histograms.txt", "w");
	file_logs = fopen("external/output/logs.txt", "w");


	if(file_in == NULL)
	{
		puts("File for reading could not be opened...");
		return EXIT_FAILURE;
	}

	if(file_out == NULL)
	{
		puts("File for writing could not be opened...");
		return EXIT_FAILURE;
	}

	if(file_histograms == NULL)
	{
		puts("File for outputting histograms could not be opened...");
		return EXIT_FAILURE;
	}

	printf("Extracting JPEG images from %s...\n", file_name);

	while(!feof(file_in))
	{
		//reading one sector at a time
		size_t bytes_read = fread(cluster_buffer, sizeof(unsigned char), SECTOR_SIZE, file_in);

		//checking that the read operation was successful
		if(bytes_read > 0)
			cluster_index++;

		zero_array(histogram_current_sector, sizeof(histogram_current_sector) / sizeof(histogram_current_sector[0]));
		fprintf(file_out, "%d\n", cluster_index);

		//checking each byte that was read from each sector
		int byte_offset;

		// analyzing
		for(byte_offset = 0; byte_offset < sizeof(cluster_buffer) / sizeof(BYTE); byte_offset++)
		{
			if(cluster_buffer[byte_offset] == 0xff && cluster_buffer[byte_offset + 1] == 0xd8 && cluster_buffer[byte_offset + 2] == 0xff)
			{
				byte_offset = handle_jpeg_header(cluster_buffer, byte_offset);
			}
			else if(cluster_buffer[byte_offset] == 0xff && cluster_buffer[byte_offset + 1] == 0xd9 && (jpeg_headers - jpeg_footers > 0))
			{
				byte_offset = handle_jpeg_footer(byte_offset);
			}
			else if(cluster_buffer[byte_offset] == 0xff && cluster_buffer[byte_offset + 1] == 0xe1 && file_opened)
			{
				handle_exif_marker();
			}

			if(file_opened && cluster_buffer[byte_offset] == 0xff)
			{
				byte_offset = handle_marker(cluster_buffer, byte_offset);
			}

			//Dumping sectors byte values
			fprintf(file_out, "%x", cluster_buffer[byte_offset]);
			histogram_current_sector[cluster_buffer[byte_offset]]++;
		}

		write_sector_to_file(cluster_buffer, bytes_read);
	}

	return finalize();
}
