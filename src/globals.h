/*
 * globals.h
 *
 *  Created on: 26 Nov 2014
 *      Author: Brandon
 */


#ifndef GLOBALS_H_
#define GLOBALS_H_

typedef unsigned char BYTE;
typedef enum { FALSE, TRUE } bool;

#include <stdio.h>
#include <glib.h>

typedef struct
{
	int centroid_num;
	float centroid_dist;
} Centroid;

extern float max_mae;
extern float max_mse;

extern char folder_corrupted_images[50];
extern char folder_decoded_images[50];
extern char folder_fragmented_images[50];
extern char folder_recovered_images[50];

extern char * output_folder_name;
extern char * file_to_smart_carve;

extern int iterations_to_decode;

extern FILE * file_jpeg_to_decode;
extern FILE * file_sectors;
extern FILE * file_histograms;
extern FILE * file_jpegs_recovered;
extern FILE * file_jpegs_partially_recovered;
extern FILE * file_classifications;
extern FILE * file_filesystem;
extern FILE * file_sectors_used;
extern FILE * file_logs;


extern int file_state;
extern int embed_state;

extern int jpeg_size;
extern int jpeg_filepos;

extern int num_jpeg_corrupted;
extern int num_jpeg_recovered;
extern int num_jpeg_fragmented;
extern int num_jpeg_embed;
extern int num_sectors_used;
extern int num_jpeg_arranged;

extern int num_jpeg_actual;
extern int num_file_types;

extern int marker_last;
extern long marker_last_sector;
extern int marker_next;

extern bool flag_footer_skip_next;
extern bool flag_header_check;

extern int write_start_offset;
extern int write_end_offset;

extern int write_start_offset_embed;
extern int write_end_offset_embed;
extern bool write_mode;

extern long cluster_index;

extern float threshold_entropy;
extern bool smart_carving_flag;

extern float bfd_cluster[256];

extern char file_jpeg_name[100];

//contains the file id and the corresponding start sector
extern GArray * g_file_id;
extern GArray * g_file_sector;
extern GArray * g_allocations;
extern GArray * g_recovered;

extern char * fragmented_folder;

extern int fs_sectors_before_fs;
extern int fs_cluster_size;
extern int fs_sector_size;
extern int fs_cluster_start;
extern int fs_encase_header_size;
extern int fs_encase_header_region;

extern char * output_folder_name;

long compared_long(gconstpointer a, gconstpointer b);


#endif /* GLOBALS_H_ */
