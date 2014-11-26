/*
 * globals.h
 *
 *  Created on: 26 Nov 2014
 *      Author: Brandon
 */

#include "helper.h"

int SECTOR_SIZE						= 512;
int file_opened						= 0;

int jpeg_size						= 0;
int jpeg_filepos					= 0;
int jpeg_corrupted					= 0;
int jpeg_recovered					= 0;
int jpeg_found						= 0;
int jpeg_actual						= 0;
int last_marker						= -1;
int last_jpeg_footer				= -1;

long last_jpeg_header_file_offset 	= -1;
long last_jpeg_footer_offset 		= -1;

int header_offset					= -1;
int skip_next_footer 				= 0;
int skip_next_header_exif			= 0;		//skipping next header because of exif marker
int check_header					= 0;

int write_start_offset				= 0;
int write_end_offset				= 512;

int cluster_index					= -1;		// the current index of a particular cluster
int write_sector					= 1;
int stop_write						= 0;

float temp_intersection				= -1;

//files
FILE * jpegs_recovered;
FILE * jpegs_partially_recovered;
FILE * file_histograms;
FILE * file_logs;
FILE * jpeg_file;
FILE * file_out;
