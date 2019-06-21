/*
 * globals.h
 *
 *  Created on: 26 Nov 2014
 *      Author: Brandon
 */

#include "helper.h"

char folder_corrupted_images[50];
char folder_decoded_images[50];
char folder_fragmented_images[50];
char folder_recovered_images[50];

char * output_folder_name;
char * file_to_smart_carve				= "n";

FILE * file_jpeg_to_decode				= NULL;			//file that will be used for the jpeg to be decoded by the decoder
FILE * file_sectors						= NULL;			// file that will be used to analyze the byte values for each sector
FILE * file_histograms 					= NULL;			//file that will be used to output the byte frequency distribution for each sector
FILE * file_jpegs_recovered				= NULL;			//file that lists the exact matched jpegs with actual jpegs
FILE * file_jpegs_partially_recovered	= NULL;			//file that lists the files that were not exactly matched with the actual jpegs to be recovered
FILE * file_classifications				= NULL;			//file that lists the classes of each sector
FILE * file_filesystem					= NULL;			//file that stores the filesystem metadata when found
FILE * file_sectors_used				= NULL;			//file that lists the classes of each sector
FILE * file_logs						= NULL;

int file_state							= FILE_CLOSED;	//state of the current file: opened/closed/ready to be closed
int embed_state							= FILE_CLOSED;	//state of the thumbnail file : opened/closed

int jpeg_size							= 0;			//size of jpeg to be decoded
int jpeg_filepos						= 0;			//jpeg files index

int num_jpeg_corrupted					= 0;			//number of corrupted carved jpeg images
int num_jpeg_recovered					= 0;			//number of fully recovered jpeg images when compared with the actual jpeg images
int num_jpeg_fragmented					= 0;			//number of fragmented images from the carved image
int num_jpeg_embed 						= 0;			// total number of JPEG thumbnails encountered
int num_sectors_used					= 0;			// total number of sector used
int num_jpeg_arranged 					= 0;			// total number of arranged JPEG images


int num_file_types						= 0;			//the number of file types to be matched for centroids

int marker_last							= MARKER_NONE;	//last restart marker found of a particular jpeg
long marker_last_sector					= -1;			//the sector at which the last valid restart marker was found
int marker_next							= -1;			//next restart marker that is needed to be found for the next continuation sector

bool flag_footer_skip_next 				= FALSE;		//flag to be used in order to skip the next jpeg footer once a header of a thumbnail is encountered
bool flag_header_check					= FALSE;		//flag to be used to indicate that the header encountered should be checked for its validity (ie. to check if it is a jpeg header or a thumbnail header)

int  write_start_offset					= 0;			//the sector offset from which the writing process should be started (header byte position)
int  write_end_offset					= 512;			//the sector offset at which the writing process should be terminated (footer byte position)

int  write_start_offset_embed			= 0;			//the sector offset from which the writing process should be started (header byte position) for thumbnails
int  write_end_offset_embed				= 512;			//the sector offset at which the writing process should be terminated (footer byte position) for thumbnails
bool write_mode							= TRUE;			//stopping the writing process of a JPEG if a fragment is encountered or if restart markers are not synchronized

long cluster_index						= -1;			// the current index of a cluster

float threshold_entropy					= 0.2;			//threshold to be used for distingushing jpeg fragment from other file type fragments
bool smart_carving_flag					= FALSE;		//flag to check whether smart carving needs to be applied or not
int iterations_to_decode				= 3000;			//default number of iterations for the decoding

char file_jpeg_name[100];								//the file that is currently being carved

//contains the file id and the corresponding start sector
GArray * g_file_id;
GArray * g_file_sector;

GArray * g_allocations;
GArray * g_recovered;


char * fragmented_folder = "fragmented";

int fs_sectors_before_fs = 0;
int fs_cluster_size = SECTOR_SIZE;
int fs_sector_size = SECTOR_SIZE;
int fs_cluster_start = 0;
int fs_encase_header_size = 0;
int fs_encase_header_region = 0;

long compared_long(gconstpointer a, gconstpointer b)
{
	return *(unsigned long *)a - *(unsigned long *)b;
}
