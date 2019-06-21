/*
 ============================================================================
 Name        : carver.c
 Author      : Brandon Birmingham
 Description : SmartCarver based on File Structure implemented in C
 Created On  : 20 September 2014
 ============================================================================
 */

#include "constants.h"
#include "performance.h"
#include "helper.h"
#include "decoding.h"
#include "filesystem.h"
#include "jpegtools.h"

#include <opencv2/highgui/highgui.hpp>
#include <errno.h>
#include <omp.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <tsk3/libtsk.h>

char name_file_sectors_used [150];
char name_file_sectors [150];
char name_file_histograms [150];
char name_file_jpegs_rec [150];
char name_file_jpegs_partially_rec [150];
char name_file_classification [150];
char name_file_filesystem_info [150];
char name_file_logs [150];
char name_file_errors [150];

int jpeg_headers = 0;					// total number of JPEG headers encountered
int jpeg_footers = 0;					// total number of JPEG footers encountered
float bfd_jpeg[256];					// array containing the count of bytes found in the range between 0 and 255

const char * file_name;

FILE * file_jpeg;
FILE * file_in;
FILE * file_embed;

clock_t start_timer				= 0;
clock_t stop_timer				= 0;
double carving_time				= 0;

long sector_header_embed 		= 0;			//sector of the last embedded header found;
long disk_byte_offset 			= 0;

long bytesoffset_header_embed	= 0;			//bytes offset of the last embedded header found;
bool cluster_written			= FALSE;

/**
 * This function is used to check if a given sector is being used by an already recovered
 * JPEG image
 *
 * Parameters: sector offset
 * Returns TRUE  - if the sector is being used by another JPEG image
 *         FALSE - if the sector is not being used by another JPEG image
 */
bool is_sector_allocated(long sector)
{
	bool allocated = FALSE;

	int i;

	for (i = 0; i < num_sectors_used*2; i+=2)
	{
		if (sector >= g_array_index(g_recovered, glong, i) && sector <= g_array_index(g_recovered, glong, i+1))
		{
			allocated = TRUE;
			break;
		}
	}

	return allocated;
}
/*
 * This function is used to handle the needed proccessing when a jpeg header is encountered
 * Parameters : sector         - sequence of byte values which contains a header of a JPEG fragment
 *              header_offser  - the offset where the header of the JPEG is found in the sector buffer
 */
int process_header(BYTE sector[], int jpeg_header_offset)
{
	/*if the header of the JPEG is found exactly at the beginning of the sector
	 *it can be safely assumed that the header is a valid JPEG header and not of a
	 *embedded JPEG header, hence the check_header is turned off
	 */

	if(jpeg_header_offset == 0)
		flag_header_check = FALSE;

	if (!is_embedded_header(sector, jpeg_header_offset + 2))
	{
		/*closing any currently opened file*/
		if (file_state != FILE_CLOSED)
		{
			long index = cluster_index - 1;
			g_array_append_val(g_allocations, index);

			fprintf(file_logs, "Closing previous opened file\n");
			fclose(file_jpeg);
		}

		//updating the number of JPEG headers found
		jpeg_headers++;

		g_array_append_val(g_file_id, jpeg_headers);
		g_array_append_val(g_file_sector, cluster_index);
		g_array_append_val(g_allocations, cluster_index);

		//creating jpeg's file name
		sprintf(file_jpeg_name, "%s%s%d%s", output_folder_name, "/carve", jpeg_headers, ".jpg");

		fprintf(file_logs, "%lu: Start of image %d\n", cluster_index, jpeg_headers);
		write_start_offset = jpeg_header_offset;

		//opening a new file for the carved JPEG
		file_jpeg = fopen(file_jpeg_name, "wb");

		file_state = FILE_OPENED;

		if(file_jpeg == NULL)
		{
			fprintf(file_logs, "File was not created for the reconstruction of the jpeg file");
			exit(ERROR);
		}

		else
		{
			fprintf(file_logs, "Creating new file %s\n", file_jpeg_name);
		}

		//checking next header position in case of embedded header is encountered before finding a footer for this header
		flag_header_check = TRUE;
	}
	else
	{
		sector_header_embed = cluster_index;
		bytesoffset_header_embed = disk_byte_offset;

		num_jpeg_embed++;
		fprintf(file_logs,"%lu: Opening embed%d-%d.jpg..\n", cluster_index, jpeg_headers, num_jpeg_embed);
		sprintf(file_jpeg_name, "%s%s%d%s%d%s", output_folder_name, "/embed", jpeg_headers, "-", num_jpeg_embed, ".jpg");
		file_embed = fopen(file_jpeg_name, "wb");

		write_start_offset_embed = jpeg_header_offset;
		embed_state = FILE_OPENED;

		//Given that this is a embedded header, therefore the next footer needs to be discarded
		//since the next footer will be that of the embedded image
		flag_footer_skip_next = TRUE;
	}

	return jpeg_header_offset;
}

/*
 * This function is used to handle the processing when a jpeg footer is found
 * Parameter: the byte offset at which the footer was found in the current read sector
 */
int process_footer(int footer_jpeg_offset)
{
	//switch off skip_next_footer flag if it is currently switched on
	int embedded_sectors = cluster_index - sector_header_embed;
	int embedded_bytes = disk_byte_offset -bytesoffset_header_embed;

	if(flag_footer_skip_next && embedded_sectors < 1300)
	{
		fprintf(file_logs, "%lu: Closing embed%d.jpg.. (%d - %d)\n", cluster_index, num_jpeg_embed, embedded_sectors, embedded_bytes);

		embed_state = FILE_TO_BE_CLOSED;
		write_end_offset_embed = footer_jpeg_offset + 2;
		flag_footer_skip_next = FALSE;
	}
	else
	{
		g_array_append_val(g_allocations, cluster_index);

		jpeg_footers++;

		fprintf(file_logs, "%lu: End Of Image %d\n\n", cluster_index, jpeg_footers);

		//file is prepared to be closed
		file_state = FILE_TO_BE_CLOSED;

		//adding extra 1 since footer_jpeg_offset marks only the ff hex values and the d9 will be otherwise discarded
		write_end_offset = footer_jpeg_offset + 2;
		flag_header_check = FALSE;

		flag_footer_skip_next = FALSE;

	}

	marker_last = MARKER_NONE;
	return footer_jpeg_offset;
}

/**
 * Handling JPEG restart marker found at a particular byte_offset from the read sector
 * Parameters: sector - sequence of byte values
 */
int process_marker(BYTE sector[], int marker_byte_offset)
{
	bool found_restart_marker = FALSE;
	int current_marker = marker_last;
	BYTE next_byte [1];

	//if 0xff is found in the last byte of the sector we need to check the next byte
	if(marker_byte_offset == (SECTOR_SIZE - 1))
	{
		//read next byte
		int bytes_read = fread(next_byte, sizeof(unsigned char), 1, file_in);

		if(bytes_read == 1)
			fseek(file_in, -1, SEEK_CUR);
	}
	else
	{
		next_byte[0] = sector[marker_byte_offset + 1];
	}

	switch(next_byte[0])
	{
		case 0xd0:
			current_marker = 0;
			found_restart_marker = TRUE;
			break;

		case 0xd1:
			current_marker = 1;
			found_restart_marker = TRUE;
			break;

		case 0xd2:
			current_marker = 2;
			found_restart_marker = TRUE;
			break;

		case 0xd3:
			current_marker = 3;
			found_restart_marker = TRUE;
			break;

		case 0xd4:
			current_marker = 4;
			found_restart_marker = TRUE;
			break;

		case 0xd5:
			current_marker = 5;
			found_restart_marker = TRUE;
			break;

		case 0xd6:
			current_marker = 6;
			found_restart_marker = TRUE;
			break;

		case 0xd7:
			current_marker = 7;
			found_restart_marker = TRUE;
			break;
	}

	//if a restart marker between 0-7 is found
	if (found_restart_marker)
	{
		//found the next marker in sequence
		if (current_marker == marker_next)
		{
			fprintf(file_logs, "[%lu] : This the continuation sector after fragmentation point\n", cluster_index);
			//removing the flag in order to continue writing to the file
			//and also deallocating the next marker to be found
			write_mode = TRUE;
			marker_next = MARKER_NONE;
		}

		//setting the sector at which the restart marker was found
		marker_last_sector = cluster_index;
	}

	//invalid positioned marker
	int marker_difference = current_marker - marker_last;
	int invalid_marker = (current_marker != MARKER_NONE) && found_restart_marker && ((marker_difference != 1) && current_marker != 0);

	//setting last valid marker found
	if(!invalid_marker)
	{
		marker_last = current_marker;
	}

	return marker_byte_offset;
}

/**
 * Analyzing sector - performing bfd normalization and computing intersection with the uniform byte
 * frequence distribution (1/256) and logs each sector details in the corresponding output text file
 * Parameters: sector - sequence of bytes
 */
void sector_analyze()
{
	//printing histogram for each sector read from disk image
	fprintf(file_histograms, "Sector : %lu - bar([0:255], [", cluster_index);
	fprintf(file_histograms, "Sector : %lu - bar([0:255], [", cluster_index);
	fprintf(file_histograms, "[%lu]", cluster_index);

	if(cluster_index == 0)
	{
		int j;
		for(j = 0; j < BYTES_RANGE; j++)
		{
			if(j == (BYTES_RANGE - 1))
				fprintf(file_histograms, "%.4f", bfd_cluster[j]);
			else
				fprintf(file_histograms, "%.4f,", bfd_cluster[j]);
		}

		fprintf(file_histograms, "]) %f \n", get_sum(bfd_cluster, BYTES_RANGE));
	}
	else
	{
		//comparing sector probability distribution with the standard jpeg probability distribution
		intersection_analyze(file_histograms, bfd_cluster, bfd_jpeg);
	}
}

/**
 * Writes a sector to the current carved JPEG file after validating the sector's probability distribution
 */
void append_cluster(float hist_sector[], float hist_jpeg[], BYTE sector[])
{

	float intersection	= 0;
	int j;

	//computing intersection between the current sector histogram and the ideal jpeg histogram
	for(j = 0; j < BYTES_RANGE; j++)
	{
		if(hist_sector[j] < hist_jpeg[j])
			intersection += hist_sector[j];
		else
			intersection += hist_jpeg[j];
	}

	/*
	 * This sector is not an entropy encoded sector therefore it should only be eliminated if start of scan is not encountered.
	 * This is because only JPEG sectors containing the zigzagged frequency coefficients are entropy encoded. The first sequence of JPEG
	 * sectors contain entirely the Huffman tables and the Quantization tables which are not entropy encoded, but definitely need to be appended
	 * to the opened JPEG file, even if they are not entropy encoded sectors
	 */

	/*
	 * a previously encountered marker and is not in the current sector, therefore it means that we are waiting for an entropy coded
	 * sector with a restart marker in the correct sequence
	 */

	if (intersection < threshold_entropy && marker_last != MARKER_NONE && marker_last_sector != cluster_index)
	{
		fprintf(file_logs, "[%lu] : skipped since it is not JPEG\n", cluster_index);
		//setting the next marker to be found in the next entropy coded sector
		marker_next = marker_last + 1;

		//stopping writing to file
		write_mode = FALSE;
	}

	//writing sector
	if (write_mode)
	{
		int offset;

		//writing sector to the opened jpeg file
		for(offset = write_start_offset; offset < write_end_offset; offset++)
			fwrite(&sector[offset], sizeof(BYTE), sizeof(BYTE), file_jpeg);

		//writing sector to the opened embedded file
		if (embed_state != FILE_CLOSED)
		{
			for(offset = write_start_offset_embed; offset < write_end_offset_embed; offset++)
				fwrite(&sector[offset], sizeof(BYTE), sizeof(BYTE), file_embed);
		}

		//JPEG file is to be closed
		if(file_state == FILE_TO_BE_CLOSED)
		{
			write_end_offset = SECTOR_SIZE;
			fclose(file_jpeg);

			file_state = FILE_CLOSED;
		}

		if (embed_state == FILE_TO_BE_CLOSED)
		{
			write_end_offset_embed = SECTOR_SIZE;
			fclose(file_embed);

			embed_state = FILE_CLOSED;
		}

		//setting write offset to the beginning
		write_start_offset = 0;
		write_start_offset_embed = 0;
	}
}

/**
 * Gets a cluster of bytes from the disk image from a particular cluster offset
 */
BYTE * get_cluster_from_disk(FILE * file_in, long cluster_offset, int number_of_sectors)
{
	BYTE * sector = (BYTE *) malloc(sizeof(BYTE) * SECTOR_SIZE * number_of_sectors);

	long byte_offset = cluster_offset * SECTOR_SIZE;
	fseek(file_in, byte_offset, SEEK_SET);
	fread(sector, sizeof(BYTE), SECTOR_SIZE * number_of_sectors, file_in);

	return sector;
}

/**
 * This function is used to remove sectors from the end of a
 * particular file
 * Parameters: file - file from which sectors need to be removed
 *             num_of_sectors - number of sectors to be removed
 */
FILE * remove_sector_from_file(char * filename, int num_of_sectors)
{
	FILE * file = fopen(filename, "r+");
	fseek(file, 0, SEEK_END);
	long size = ftell(file);
	fseek(file, 0, SEEK_SET);

	long total_bytes = size - (num_of_sectors * SECTOR_SIZE);
	BYTE * file_data = (BYTE *) malloc(sizeof(BYTE) * total_bytes);
	fread(file_data, sizeof(BYTE), total_bytes, file);
	fclose(file);
	remove(filename);

	file = fopen(filename, "wb+");

	fwrite(file_data, sizeof(BYTE), total_bytes, file);

	return file;
}
/*
 * Copies parts of a file to a new file specified by the from and to offsets
 */
void copy_file(FILE * original, FILE * newfile, long from, long to)
{
	long total_bytes = to - from;

	BYTE * file_data = (BYTE *) malloc(sizeof(BYTE) * total_bytes);

	fseek(original, from, SEEK_SET);
	fread(file_data, sizeof(BYTE), total_bytes, original);
	fwrite(file_data, sizeof(BYTE), total_bytes, newfile);

	free(file_data);
}

/*
 * Appends a sector to a particular file
 */
void append_sector_to_file(FILE * file, BYTE * sector, int sectors_to_overwrite, int sectors_to_write)
{
	fseek(file, SECTOR_SIZE*sectors_to_overwrite*-1, SEEK_END);
	fwrite(sector, sizeof(BYTE), SECTOR_SIZE * sectors_to_write, file);
}

void backtrack_to_fragmentation_point(FILE * arranged_file, FILE * frag_file, char * arranged_file_name, int * prev_scanline,
									  int * check_mae, int * temp_invalid_sector, int * invalid_sector, long * next_cluster_offset,
									  long * last_cluster_offset, int * invalid_count, bool * next_sector_found, int * overwrite_sectors,
									  int * valid_sectors, int *empty_regions)
{
	fprintf(file_logs, "Backtrack to initial fragment position\n");
	fprintf(file_logs,"----------------------------------------------------------\n");

	*prev_scanline = -1;
	*check_mae = MAE_CHECK_FIRST_CLUSTER;

	fclose(arranged_file);
	arranged_file = fopen(arranged_file_name, "wb");

	/*Copying contents as it was orignally before the recovery stage added new sectors*/
	copy_file(frag_file, arranged_file, 0, *temp_invalid_sector * SECTOR_SIZE);

	/*Setting the invalid sector as it was initially*/
	*invalid_sector = *temp_invalid_sector;
	*next_cluster_offset = *last_cluster_offset;
	*invalid_count = 0;
	*next_sector_found = FALSE;
	*overwrite_sectors = 0;
	*valid_sectors = 0;
	*empty_regions = 0;
}

/**
 * Choosing the sector before the fragmentation point.
 * The chosen sector will be either retreived directly
 * from the decoder or else in accordance with the thumbnail
 * Parameters: sector_decoder   - the sector retrieved from the decoder
 *             sector_thumbnail - the sector retrieved by thumbnail comparison
 */
int choose_sector(int sector_decoder, int sector_thumbnail)
{
	int sectors_before_fp;

	int sector_difference = abs(sector_decoder - sector_thumbnail);

	if (abs(sector_difference) > 50 && sector_thumbnail != -1)
	{
		sectors_before_fp = sector_thumbnail;
		fprintf(file_logs, "Thumbnail sector is going to be used - %d\n", sectors_before_fp);
	}

	else
	{
		sectors_before_fp = sector_decoder;
		fprintf(file_logs, "Decoder sector is going to be used - %d\n", sectors_before_fp);
	}

	return sectors_before_fp;
}

int delete_arrange_file(char * filename, int trial, int fragment)
{
	sprintf(filename, "%s%s%d-%d-%d%s", output_folder_name, "/recovered_", num_jpeg_arranged, trial-1, fragment, ".jpg");
	fprintf(file_logs, "Deleting: %s\n", filename);
	int del = remove(filename);

	if (del != 0)
		fprintf(file_logs, "%s was not deleted\n", filename);

	return del;
}

/**
 * Tries to arrange a fragmented file starting from the fragmentation point
 * frag_file			- the file that needs to be arranged
 * invalid_sector		- the sector of fragmentation point
 * header_sector_offset	- the header offset of the
 */
void arrange_fragmented_file(FILE * frag_file, decoder_result decoder, long start_sector, Thumbnail thumbnail_object, long last_fragment_offset, int fragment)
{
	int invalid_sector_decoder = decoder.decoder_sector;
	int invalid_sector_thumbnail = decoder.thumb_sector;
	fprintf(file_logs, "Sectors before fragmentation point: Decoder %d - Thumbnail %d\n", invalid_sector_decoder, invalid_sector_thumbnail);

	int sector_before_fp = choose_sector(invalid_sector_decoder, invalid_sector_thumbnail);
	fprintf(file_logs, "Start sector from : %lu\n", start_sector);

	int decoded_type;
	bool stop = FALSE;

	int tries;
	int max_iterations = 15;

	for(tries = 0; tries < max_iterations; tries++)
	{
		fprintf(file_logs, "Decoding iteration starting from sector before FP : %d (try %d)..\n", sector_before_fp, tries);
		int check_mae = MAE_CHECK_FIRST_CLUSTER;
		int prev_scanline = -1;
		int empty_regions = 0;
		int iterations = 0;
		int invalid_sector = sector_before_fp - tries;
		int temp_invalid_sector = invalid_sector;
		int overwrite_sectors = 0;
		int valid_sectors = 0;

		decoded_type = ERROR;

		int invalid_count = 0;
		long last_cluster_offset = -1;
		long next_cluster_offset;

		bool next_sector_found = FALSE;

		decoder_result result;

		FILE * arranged_file;

		char * arranged_file_name = NULL;

		if (!stop)
		{
			arranged_file_name = (char *)malloc(sizeof(char) * 100);

			if (tries != 0)
				delete_arrange_file(arranged_file_name, tries, fragment);

			fprintf(file_logs, "Trying to fully-recover image starting at fragmentation point %d...\n", invalid_sector);
			sprintf(arranged_file_name, "%s%s%d-%d-%d%s", output_folder_name, "/recovered_", num_jpeg_arranged, tries, fragment, ".jpg");

			arranged_file = fopen(arranged_file_name, "wb");
			copy_file(frag_file, arranged_file, 0, invalid_sector * SECTOR_SIZE);
			fclose(arranged_file);

			if (last_fragment_offset == UKNOWN)
				next_cluster_offset = start_sector + invalid_sector;

			else
				next_cluster_offset = last_fragment_offset;
		}

		/* Try to recover the fragmented image until no error is returned from the decoder*/
		while (decoded_type != SUCCESS && !stop && iterations <= iterations_to_decode)
		{
			iterations++;
			arranged_file = fopen(arranged_file_name, "rb+");

			int number_of_sectors = fs_cluster_size / fs_sector_size;

			while (is_sector_allocated(next_cluster_offset))
				next_cluster_offset += number_of_sectors;

			fprintf(file_logs, "Next cluster %lu\n", next_cluster_offset);

			BYTE * sector = get_cluster_from_disk(file_in, next_cluster_offset + fs_sectors_before_fs, number_of_sectors);
			append_sector_to_file(arranged_file, sector, overwrite_sectors, number_of_sectors);

			result = decode_jpeg(arranged_file_name, FALSE, NULL);

			if (prev_scanline == -1)
				prev_scanline = result.decoder_scanline;

			else
			{
				float pixel_mae;
				int difference_scanlines = result.decoder_scanline - prev_scanline;

				/*int region_size = prev_scanline - invalid_scanline_decoder;*/

				/*Given that a corresponding thumbnail is found for this particular
				 * JPEG image, each scanline is compared with the thumbnail in order
				 * to detect correct multi-fragmented regions
				 */
				if (result.decoder_type != ERROR && thumbnail_object.image != NULL)
				{
					int region_size = result.decoder_scanline - prev_scanline;

					if (region_size == 0)
						empty_regions++;
					else
						empty_regions = 0;

					IplImage * image = cvLoadImage(arranged_file_name, CV_LOAD_IMAGE_UNCHANGED);
					pixel_mae = compare_region_with_thumbnail(image, thumbnail_object.image,prev_scanline, region_size);
					cvRelease((void *)&image);

					if (pixel_mae != ERROR)
						fprintf(file_logs, "MSE %.2f\n", pixel_mae);

					if (check_mae == MAE_CHECK_FIRST_CLUSTER && pixel_mae < 57 && pixel_mae != ERROR)
						check_mae = MAE_CHECK_OTHER_CLUSTER;

					else if (check_mae == MAE_CHECK_FIRST_CLUSTER)
						check_mae = MAE_NO_CHECK;

					if (check_mae == MAE_CHECK_OTHER_CLUSTER &&
						pixel_mae > 61  &&
						pixel_mae != ERROR &&
						valid_sectors > 70 )
					{
						check_mae = MAE_NO_CHECK;
						fprintf(file_logs, "This is a correct fragment\n");
						arranged_file = remove_sector_from_file(arranged_file_name, number_of_sectors);
						result.decoder_sector -= number_of_sectors;
						arrange_fragmented_file(arranged_file, result, start_sector, thumbnail_object, next_cluster_offset, ++fragment);
						tries = max_iterations;
						break;
					}

					else if (check_mae == MAE_NO_CHECK && pixel_mae > 85)
					{
						fprintf(file_logs, "This is not a correct fragment, stop.\n");
						backtrack_to_fragmentation_point(arranged_file, frag_file, arranged_file_name, &prev_scanline, &check_mae,
														 &temp_invalid_sector, &invalid_sector, &next_cluster_offset, &last_cluster_offset,
														 &invalid_count, &next_sector_found, &overwrite_sectors, &valid_sectors, &empty_regions);

						free(sector);
						next_cluster_offset+= number_of_sectors;
						continue;
					}
				}

				/*Limiting the discrepency between consecutive scanlines*/
				if (difference_scanlines > 100 && valid_sectors > 50)
				{
					fprintf(file_logs, "Large discrepency between scanlines %d\n", result.decoder_scanline);
					fprintf(file_logs, "Removing last cluster from image file\n");

					/*Removing previous cluster from file*/
					arranged_file = remove_sector_from_file(arranged_file_name, number_of_sectors);

					/*Matches with Thumbnail region*/
					if (pixel_mae < 35)
					{
						result = decode_jpeg(arranged_file_name, FALSE,thumbnail_object.image);
						arrange_fragmented_file(arranged_file, result, start_sector, thumbnail_object, next_cluster_offset, ++fragment);
						tries = max_iterations;
						break;
					}
				}
				else
				{
					prev_scanline = result.decoder_scanline;
				}
			}

			decoded_type = result.decoder_type;

			if (decoded_type == SUCCESS)
				stop = TRUE;

			/*Considering last sector as an invalid sector*/
			if (result.decoder_sector == invalid_sector && invalid_count > 0)
			{
				invalid_count++;

				/*
				 * Given that a new sector was appended (given that it was assumed that it was the continuation sector)
				 * if subsequent invalid sectors will be encountered it can be safely assumed that the added sector was an
				 * invalid one since after the addition, futher problems were encountered. Hence the added sector will be removed
				 * and start the recovery process again...
				*/
				if (invalid_count == 2 && next_sector_found)
				{
					fprintf(file_logs, "Backtrack to initial fragment position\n");
					fprintf(file_logs,"----------------------------------------------------------\n");

					prev_scanline = -1;
					check_mae = MAE_CHECK_FIRST_CLUSTER;

					fclose(arranged_file);
					arranged_file = fopen(arranged_file_name, "wb");

					copy_file(frag_file, arranged_file, 0, temp_invalid_sector * SECTOR_SIZE);

					invalid_sector = temp_invalid_sector;
					next_cluster_offset = last_cluster_offset;
					invalid_count = 0;
					next_sector_found = FALSE;
					overwrite_sectors = 0;
					valid_sectors = 0;
					empty_regions = 0;
				}
				else
				{
					overwrite_sectors = number_of_sectors;
				}
			}

			/*
			 * Considered as valid but will be fully-verified later in the process
			 * In this case, if for example the decoder detected that the last invalid sector
			 * was x, and after the last added sector the decoder detects the same error at the same
			 * location, it will also be added. If subsequent errors will be encountered, this will
			 * be then removed. The main point of doing this, is because the decoder, may not detect
			 * error found in sectors very close to each other
			*/
			else if (result.decoder_sector == invalid_sector && invalid_count == 0)
			{
				/*if the continuation sector is not already found*/
				if (!next_sector_found)
				{
					next_sector_found = TRUE;
					last_cluster_offset = next_cluster_offset;
				}

				invalid_count++;
				invalid_sector = result.decoder_sector;
				overwrite_sectors = 0;

				valid_sectors += number_of_sectors;
			}

			/* This sector is considered as valid*/
			else if (result.decoder_sector > invalid_sector)
			{
				/*if the continuation sector is not already found*/
				if (!next_sector_found)
				{
					next_sector_found = TRUE;
					last_cluster_offset = next_cluster_offset;
				}

				invalid_count = 0;
				invalid_sector = result.decoder_sector;
				overwrite_sectors = 0;

				valid_sectors += number_of_sectors;
			}

			/*Free memory for the last sector that was read and closing the file*/
			free(sector);
			fclose(arranged_file);
			next_cluster_offset+= number_of_sectors;
		}

		if (decoded_type == SUCCESS)
		{
			fprintf(file_logs, "Image was recovered after %d iterations..\n", iterations);
			fprintf(file_logs, "\nbeg 1 %lu\nend 1 %lu\nbeg 2 %lu\nend 2 %lu\n", start_sector, start_sector + temp_invalid_sector -1,last_cluster_offset, next_cluster_offset - 2);

			long foot_1 = start_sector + temp_invalid_sector -1;
			long foot_2 = next_cluster_offset - 2;
			g_array_append_val(g_recovered, start_sector);
			g_array_append_val(g_recovered, foot_1);
			g_array_append_val(g_recovered, last_cluster_offset);
			g_array_append_val(g_recovered, foot_2);
			g_array_sort(g_recovered, (gconstpointer)compared_long);
			break;
		}
	}

	num_jpeg_arranged++;
}

/**
 * This function is used to check if the filepath
 * is that of an embedded file
 * Parameters: filepath - path of the file to check if it is that of an embedded file
 * Returns TRUE if it is embedded file otherwise FALSE;
 */
bool is_file_embed(char * filepath)
{
	bool embed_file = FALSE;
	int start_offset = strlen(output_folder_name) + 1;

	char substr[5];

	strncpy(substr, filepath + start_offset, 5);

	if (strcmp(substr, "embed") == 0)
		embed_file = TRUE;

	return embed_file;
}
/**
 * Gets the header offset of a particular jpeg file
 * filepath - the file path for the requested header
 */
long get_header_offset(char * filepath)
{
	int out_folder = strlen(output_folder_name);
	int start_offset = out_folder + 6;
	char * ptr = strchr(filepath, '.');

	int end_offset = ptr - filepath;

	char file_pos[end_offset-start_offset + 1];
	memcpy(file_pos, filepath + start_offset, end_offset-start_offset);

	int file_position = atoi(file_pos);

	long sector = g_array_index(g_file_sector, glong, file_position-1);
	return sector;
}

int finalize()
{
	analyze_recovered_images("external\\files");

	/*closing all opened files*/
	fclose(file_in);
	fclose(file_sectors);
	fclose(file_histograms);
	fclose(file_jpegs_recovered);
	fclose(file_jpegs_partially_recovered);
	fclose(file_classifications);
	fclose(file_filesystem);
	fclose(file_logs);

	//asserting_used_sectors_canon_gen6();

	g_array_free(g_file_id, TRUE);
	g_array_free(g_file_sector, TRUE);
	g_array_free(g_allocations, TRUE);
	g_array_free(g_recovered, TRUE);

	stop_timer = clock();
	carving_time = (double)(stop_timer - start_timer) / CLOCKS_PER_SEC;

	printf("Execution time                                : %.2f sec / %.2f mins\n", carving_time, carving_time/60);
	printf("-----------------------------------------------------------------------\n");

	return EXIT_SUCCESS;
}

void prepare_log_files(char * logs_path)
{
	sprintf(name_file_sectors_used, "%s/%s", logs_path, "sectors_used.txt");
	sprintf(name_file_sectors, "%s/%s", logs_path, "sectors.txt");
	sprintf(name_file_histograms, "%s/%s", logs_path, "histograms.txt");
	sprintf(name_file_jpegs_rec, "%s/%s", logs_path, "jpegs_recovered.txt");
	sprintf(name_file_jpegs_partially_rec, "%s/%s", logs_path, "jpegs_partially_recovered.txt");
	sprintf(name_file_classification, "%s/%s", logs_path, "classifications.txt");
	sprintf(name_file_filesystem_info, "%s/%s", logs_path, "filesystem.txt");
	sprintf(name_file_logs, "%s/%s", logs_path, "logs.txt");
	sprintf(name_file_errors, "%s/%s", logs_path, "errors.txt");

	dup(fileno(stderr));
	freopen(name_file_errors,"a",stderr);

	file_sectors = fopen(name_file_sectors, "w");
	file_histograms = fopen(name_file_histograms, "w");
	file_jpegs_recovered = fopen(name_file_jpegs_rec, "w");
	file_jpegs_partially_recovered = fopen(name_file_jpegs_partially_rec, "w");
	file_classifications = fopen(name_file_classification, "w");
	file_filesystem = fopen(name_file_filesystem_info, "w+");
	file_sectors_used = fopen(name_file_sectors_used, "w+");
	file_logs = fopen(name_file_logs, "w+");
}

void initialization(char * file_name)
{
	printf("JPEGCarver Initialization...\n");
	start_timer = clock();
	setvbuf (stdout, NULL, _IONBF, 0);

	struct stat info;

	if(stat(file_name, &info) != 0)
	{
		printf("Failed to retrieve disk image details...\n");
		exit(ERROR);
	}

	file_in = fopen(file_name, "rb");

	if(file_in == NULL)
	{
		puts("Disk image was not found");
		exit(ERROR);
	}

	printf("Setting up environment...\n");

	printf("Creating directory for the carved images\n");
	create_directory(output_folder_name);

	printf("Creating Logs Directory...\n");
	char log_path[150];
	sprintf(log_path, "%s/logs", output_folder_name);
	create_directory(log_path);
	prepare_log_files(log_path);

	/*setting the ideal uniform distribution of jpeg*/
	bfd_uniform(bfd_jpeg);

	/*initialising byte frequency distribution for the fist sector to be read*/
	bfd_init(bfd_cluster);

	g_file_id = g_array_new(FALSE, FALSE, sizeof(gint));
	g_file_sector = g_array_new(FALSE, FALSE, sizeof(glong));
	g_allocations = g_array_new(FALSE, FALSE, sizeof(glong));
	g_recovered = g_array_new(FALSE, FALSE, sizeof(glong));

	printf("-----------------------------------------------------------------------\n");
}

void preprocessing(char * file_name)
{
	printf("Preprocessing...\n");
	TSK_TCHAR *drivename[] = {(TSK_TCHAR *) file_name};
	TSK_IMG_INFO * tsk_disk_info =  tsk_img_open(1, &drivename, TSK_IMG_TYPE_DETECT, SECTOR_SIZE);

	if (tsk_disk_info == NULL)
	{
		tsk_error_print(stderr);
		exit(ERROR);
	}

	if (tsk_disk_info->itype == TSK_IMG_TYPE_EWF_EWF)
	{
		printf("This is an encase disk image\n");

		TSK_FS_INFO * tsk_fs;

		while( (tsk_fs = tsk_fs_open_img(tsk_disk_info, fs_sectors_before_fs*tsk_disk_info->sector_size, TSK_IMG_TYPE_DETECT)) == NULL)
			fs_sectors_before_fs++;

		if (tsk_fs == NULL)
			printf("Problem opening filesystem");

		tsk_fs->fscheck(tsk_fs, stdout);
		tsk_fs->fsstat(tsk_fs, file_filesystem);

		fs_cluster_size = fs_get_value(file_filesystem, FS_CLUSTER_SIZE);
		fs_sector_size = fs_get_value(file_filesystem, FS_SECTOR_SIZE);
		fs_cluster_start = fs_get_value(file_filesystem, FS_CLUSTER_AREA_START);
		fs_encase_header_size = fs_get_header_case_info_size(file_in);
		fs_encase_header_region = fs_encase_header_size + 4;

		printf("File System type: %s\n", tsk_fs_type_toname(tsk_fs->ftype));
		printf("Sector Size: %d\n", fs_sector_size);
		printf("Cluster Size: %d\n", fs_cluster_size);

		printf("Encase Header region byte offsets: 0 - %d\n", fs_encase_header_region - 1);
		printf("Encase Data Blocks start from byte offset: %d\n", fs_encase_header_region);

		printf("File system starts at sector: %d\n", fs_sectors_before_fs);
		printf("Cluster Region starts at sector (wrt disk image): %d\n", fs_cluster_start + fs_sectors_before_fs);
		printf("Cluster Region starts at sector (wrt filesystem): %d\n", fs_cluster_start);

		printf("Preparing Encase disk image for recovery...\n");
		file_in = fs_discard_crc(file_in, tsk_disk_info->size, fs_encase_header_region);

		/*moving to cluster region*/
		if (fseek(file_in, (fs_sectors_before_fs + fs_cluster_start)*fs_sector_size, SEEK_SET) != 0)
		{
			printf("File pointer was not moved to the cluster region\n");
			exit(ERROR);
		}
		else
		{
			cluster_index = fs_cluster_start - 1;
			printf("File pointer was moved to cluster region\n");
		}
	}

	printf("-----------------------------------------------------------------------\n");
}

void collation(char * file_name)
{
	printf("Extracting JPEG images from %s...\n", file_name);

	/*holds the sequence of bytes for each cluster read*/
	BYTE cluster[fs_cluster_size];

	/*main loop of sector traversal*/
	while(!feof(file_in))
	{
		/*reading cluster*/
		size_t bytes_read = fread(cluster, sizeof(BYTE), SECTOR_SIZE, file_in);

		/*checking that the read operation was successful*/
		if(bytes_read > 0)
			cluster_index++;

		bfd_init(bfd_cluster);

		#if LOG
			fprintf(file_sectors, "\n[%ld] : ", cluster_index);
		#endif

		/*checking each byte that was read from each sector*/
		int byte_offset;

		for(byte_offset = 0; byte_offset < SECTOR_SIZE; byte_offset++)
		{
			disk_byte_offset++;

			if(cluster[byte_offset] == 0xff && cluster[byte_offset + 1] == 0xd8 && cluster[byte_offset + 2] == 0xff)
			{
				byte_offset = process_header(cluster, byte_offset);
			}

			else if(cluster[byte_offset] == 0xff && cluster[byte_offset + 1] == 0xd9 && file_state == FILE_OPENED)
			{
				byte_offset = process_footer(byte_offset);
				append_cluster(bfd_cluster, bfd_jpeg, cluster);
				cluster_written = TRUE;
			}

			if(file_state && cluster[byte_offset] == 0xff)
			{
				byte_offset = process_marker(cluster, byte_offset);
			}

			#if LOG
				/*Dumping sectors byte values*/
				fprintf(file_sectors, "%02x", cluster[byte_offset]);
			#endif

			/*byte frequency distribution*/
			bfd_cluster[cluster[byte_offset]]++;
		}

		/*normalizing the current sector*/
		normalize_bfd();

		#if LOG
			/*analayzing sector*/
			sector_analyze(bfd_cluster);
		#endif

		/*appending current sector with the current opened file*/
		if(file_state != FILE_CLOSED && !cluster_written)
			append_cluster(bfd_cluster, bfd_jpeg, cluster);

		if(cluster_written)
			cluster_written = FALSE;
	}

	printf("-----------------------------------------------------------------------\n");
}

int carve(char * file_name)
{
	initialization(file_name);;
	preprocessing(file_name);
	collation(file_name);
	return finalize();
}

/**
 * This function is used to remove allocated sectors that were featured in
 * fragmented JPEG images
 *
 * Parameters sector_to_rm - sector to be removed
 * Returns 	  ERROR if the sector was not removed
 *            SUCCESS if the sector was removed succcessfully
 */
int remove_allocations(long sector_to_rm)
{
	int ret = ERROR;
	int i;

	for (i = 0; i < jpeg_headers*2; i++)
	{
		if (g_array_index(g_allocations, glong, i) == sector_to_rm)
		{
			g_array_remove_index(g_allocations, i);
			g_array_remove_index(g_allocations, i);
			ret = SUCCESS;
		}
	}

	return ret;
}

/**
 * This function is used to keep track of the used sectors for totally
 * recovered JPEG images
 *
 * Parameters sector_to_add - used sector of a recovered JPEG image
 * Returns    SUCCESS - if the sector was added sucessfully
 *            ERROR   - if the sector was not added
 */
int add_allocations(long sector_to_add)
{
	int ret = ERROR;
	int i;

	long header;
	long footer;

	for (i = 0; i < jpeg_headers*2; i++)
	{
		header = g_array_index(g_allocations, glong, i);
		if (header == sector_to_add)
		{
			g_array_append_val(g_recovered, header);
			footer = g_array_index(g_allocations, glong, i+1);
			g_array_append_val(g_recovered, footer);

			remove_allocations(header);

			ret = SUCCESS;
			break;
		}
	}

	return ret;
}
