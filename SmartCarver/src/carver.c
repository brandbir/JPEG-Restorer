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
#include <dirent.h>
#include "picojpeg.h"
#include "filecompare.h"

typedef unsigned char BYTE; // data type for BYTE object

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

FILE *jpeg_file;
FILE *file_out;

int file_opened = 0;
int cluster_size = 512;

int jpeg_size			= 0;
int jpeg_filepos		= 0;
int jpeg_corrupted		= 0;
int jpeg_recovered		= 0;
int jpeg_found			= 0;
int jpeg_actual			= 0;
int last_marker			= -1;
int last_jpeg_footer	= -1;
int jpeg_markers[8] 	= {0, 0, 0, 0, 0, 0, 0, 0};
int map_markers[8] 	= {0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7};

long last_jpeg_header_file_offset = -1;
long last_jpeg_footer_offset = -1;

int header_offset	 		= -1;
int skip_next_footer 		= 0;
int skip_next_header_exif	= 0;   //skipping next header because of exif marker
int check_header = 0;

int write_start_offset		= 0;
int write_end_offset		= 512;

int cluster_index			= -1; // the current index of a particular cluster
int write_sector			= 1;
int stop_write				= 0;

float temp_intersection = -1;

FILE * jpegs_recovered;
FILE * jpegs_partially_recovered;
FILE * file_histograms;
FILE * file_logs;

void copy_array(float copied[], float copy_from[], int size)
{
	int i;
	for(i = 0; i < size; i++)
		copied[i] = copy_from[i];
}


unsigned char pjpeg_need_bytes_callback(unsigned char* buf, unsigned char buf_size, unsigned char *bytes_actually_read, void *callback_data)
{
	unsigned int n = min((unsigned int)(jpeg_size - jpeg_filepos), (unsigned int)buf_size);

	if (n && (fread(buf, 1, n, jpeg_file) != n))
		return PJPG_STREAM_READ_ERROR;

	*bytes_actually_read = (unsigned char)(n);
	jpeg_filepos += n;
	return 0;
}

void zero_array(float array[], int size)
{
	int i;
	for(i = 0; i < size; i++)
		array[i] = 0;
}

/*
 * This method is used to check if a file is a JPEG image.
 * The method returns 1 if the file is a JPEG or a 0 if the file
 * is not a JPEG
 */
int decode_jpeg(char * filename)
{
	pjpeg_image_info_t image_details;
	jpeg_file = fopen(filename, "rb");

	if(!jpeg_file)
		printf("JPEG file %s was not found...", filename);

	fseek(jpeg_file, 0, SEEK_END);
	jpeg_size = ftell(jpeg_file);
	jpeg_filepos = 0;
	fseek(jpeg_file, 0, SEEK_SET);
	int status = pjpeg_decode_init(&image_details, pjpeg_need_bytes_callback, NULL, (unsigned char)1);

	if(status)
	{
		if(status == PJPG_UNSUPPORTED_MODE)
			printf("Progressive JPEG files are not supported...\n");

		fclose(jpeg_file);
		return 0;
	}
	else
	{
		while(status == 0)
		{
			status = pjpeg_decode_mcu();
			if(status == PJPG_BAD_RESTART_MARKER)
				fprintf(file_logs, "MCU status :: %d -> %s\n", status, filename);
		}

		return 1;
	}
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
void histogram_difference(float histogram_prev[], float histogram_next[])
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
		fprintf(file_histograms, "%.4f ", histogram_prev[j]);

	fprintf(file_histograms, "]) total : %f, intersection: %.4f\n", get_sum(histogram_prev, 256), intersection);
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

/*
 * Normalising the histogram to obtain the probability distribution
 * function for each symbol
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

void performance_metric(char * folder_produced, char * folder_actual)
{
	DIR	* fp;
	int number_of_jpegs = 0;
	struct dirent *dir;

	fp = opendir(folder_produced);

	if(fp)
	{
		while((dir = readdir(fp)) != NULL)
		{
			char * file_name = dir -> d_name;

			if(!(strcmp(file_name, ".") == 0 || strcmp(file_name, "..") == 0))
			{
				number_of_jpegs++;
				char file_path[50];
				sprintf(file_path, "%s%s%s", folder_produced, "\\", file_name);

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

		closedir(fp);

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

int main(int argc, char * argv[])
{
	//setting timer
	clock_t start_timer, stop_timer;
	double carving_time;
	start_timer = clock();

	//No buffer in stdout
	setvbuf (stdout, NULL, _IONBF, 0);

	const char * output_folder_name = "external/output/results/jpegs-dfrws-2006/";
	const char *file_name = "external\\dfrws-2006-challenge.raw";
	struct stat info;

	int total_bytes = 0;					// the total number of bytes read from the disk image
	int jpeg_headers = 0;					// total number of JPEG headers encountered
	int jpeg_footers = 0;					// total number of JPEG footers encountered
	BYTE cluster_buffer[cluster_size];		// holds the sequence of bytes fore each cluster read
	float histogram_jpeg_dist[256];			// array containing the count of bytes found in the range between 0 and 255
	float histogram_current_sector[256];	// temporary histogram in order to find the correlation between the two consecutive histograms

	//setting the ideal uniform distribution of JPEG histogram
	set_jpeg_uniform_distribution(histogram_jpeg_dist, sizeof(histogram_jpeg_dist) / sizeof(histogram_jpeg_dist[0]));

	//zeroing the histogram array for the first sector
	zero_array(histogram_current_sector, sizeof(histogram_current_sector) / sizeof(histogram_current_sector[0]));

	if(stat(file_name, &info) != 0)
		puts("stat failed to retrieve file details");

	puts("SmartCarver initialisation...");
	puts("Initialising Histogram values...");

	int total_clusters = ceil(info.st_size / cluster_size);
	printf("Number of clusters found : %d\n", total_clusters);

	//opening file in read and binary mode
	FILE * file_in = fopen(file_name, "rb");
	file_out = fopen("external/output/out.txt", "w");

	FILE * file_jpeg = NULL;
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
		size_t bytes_read = fread(cluster_buffer, sizeof(unsigned char), cluster_size, file_in);

		//checking that the read operation was successful
		if(bytes_read > 0)
			cluster_index++;

		int byte_offset;
		char file_jpeg_name[30];


		zero_array(histogram_current_sector, sizeof(histogram_current_sector) / sizeof(histogram_current_sector[0]));
		fprintf(file_out, "%d\n", cluster_index);


		/*
		 * jpeg_markers array is used to keep track of the order of jpeg clusters. Example : If the last marker of the last
		 * JPEG fragment contained for example 0xffd2, it will be assumed that the next fragment will contains the marker 0xffd3
		 */

		/*int i;
		for(i = 0; i < 8; i++)
		{
			if(jpeg_markers[i] != 0)
			{

				 * if the previous marker is greater than the current marker this means that an extra unwanted fragment was
				 * inserted in between. In order to eliminate the sector with invalid marker, that particular sector will be
				 * overwritten with actual jpeg sectors


				if(last_marker > i && (last_marker != 7 && i != 0) && file_opened)
				{
					printf("Performing backtracking of 1 sector\n");
					fseek(file_jpeg, cluster_size*-1, SEEK_CUR);
				}

				//setting last marker that was found from the previous sector
				last_marker = i;
			}

			//re-setting markers
			jpeg_markers[i] = 0;
		}*/

		//checking each byte that was read from each sector
		for(byte_offset = 0; byte_offset < sizeof(cluster_buffer) / sizeof(BYTE); byte_offset++)
		{
			if(cluster_buffer[byte_offset] == 0xff && cluster_buffer[byte_offset + 1] == 0xd8 && cluster_buffer[byte_offset + 2] == 0xff)
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
					last_jpeg_header_file_offset = (cluster_index * cluster_size) + byte_offset;

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
			}
			else if(cluster_buffer[byte_offset] == 0xff && cluster_buffer[byte_offset + 1] == 0xd9 && (jpeg_headers - jpeg_footers > 0))
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
					fprintf(file_logs, "Footer %d found at Sector: %d\n", jpeg_footers, cluster_index / cluster_size);

					//prepared for closing the file
					file_opened = 2;

					//adding extra 1 for less than equivalence
					write_end_offset = byte_offset + 2;
					temp_intersection = -1;

					//Once we find the actual footer of the JPEG, the exif flag must be restarted
					skip_next_header_exif = 0;
					check_header = 0;
				}
			}
			else if(cluster_buffer[byte_offset] == 0xff && cluster_buffer[byte_offset + 1] == 0xe1 && file_opened)
			{
				//If EXIF application marker is encountered we need to allow all the
				//following JPEG headers to be added to the current opened file that
				//is being used for recovery until we find the other exif application marker

				if(skip_next_header_exif)
					skip_next_header_exif = 0;
				else
					skip_next_header_exif = 1;
			}

			/*if(cluster_index == 11763)
			{
				int i;
				for (i = 0; i < 512; i++)
					printf("%x ", cluster_buffer[i]);
			}*/
			if(file_opened && cluster_buffer[byte_offset] == 0xff)
			{
				//JPEG file marker is found
				int changed = 0;
				int current_marker = last_marker;
				BYTE next_byte [1];
				//if 0xff is found in the last byte of the sector we need to check the next byte

				if(byte_offset == (cluster_size - 1))
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
			}

			//Dumping sectors byte values
			fprintf(file_out, "%x", cluster_buffer[byte_offset]);
			histogram_current_sector[cluster_buffer[byte_offset]]++;
		}

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
				histogram_difference(histogram_current_sector, histogram_jpeg_dist);
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

