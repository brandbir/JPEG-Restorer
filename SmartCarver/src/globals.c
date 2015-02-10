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
int next_marker						= -1;
int last_marker_sector				= -1;
int last_jpeg_footer				= -1;

long last_jpeg_header_file_offset 	= -1;
long last_jpeg_footer_offset 		= -1;

int header_offset					= -1;
int skip_next_footer 				= 0;
int check_header					= 0;

int write_start_offset				= 0;
int write_end_offset				= 512;

int sector_index					= -1;		// the current index of a particular sector
int write_sector					= 1;
int stop_write						= 0;

float temp_intersection				= -1;
float ENTROPY_THRESHOLD				= 0.3;

FILE * jpeg_to_decode				= NULL;
FILE * file_histograms 				= NULL;
FILE * file_out						= NULL;
FILE * jpegs_recovered				= NULL;
FILE * jpegs_partially_recovered	= NULL;

