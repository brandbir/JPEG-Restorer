/*
 * globals.h
 *
 *  Created on: 26 Nov 2014
 *      Author: Brandon
 */


#ifndef GLOBALS_H_
#define GLOBALS_H_

#include <stdio.h>


typedef unsigned char BYTE; // data type for BYTE object

extern int SECTOR_SIZE;

//files
extern FILE * jpeg_to_decode;
extern FILE * file_out;
extern FILE * file_histograms;
extern FILE * jpegs_recovered;
extern FILE * jpegs_partially_recovered;

extern int file_opened;
extern int jpeg_size;
extern int jpeg_filepos;
extern int jpeg_corrupted;
extern int jpeg_recovered;
extern int jpeg_found;
extern int jpeg_actual;
extern int last_marker;								//last marker possessed by a JPEG fragment
extern int next_marker;								//next marker of a JPEG sector
extern int marker_continuation;
extern int last_marker_sector;

extern int last_jpeg_footer;

extern long last_jpeg_header_file_offset;
extern long last_jpeg_footer_offset;

extern int header_offset;
extern int skip_next_footer;
extern int skip_next_header_exif;
extern int check_header;

extern int write_start_offset;
extern int write_end_offset;

extern int sector_index;
extern int write_sector;
extern int stop_write;

extern float temp_intersection;
extern float ENTROPY_THRESHOLD;

#endif /* GLOBALS_H_ */
