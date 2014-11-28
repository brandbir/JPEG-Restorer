/*
 * carver.h
 *
 *  Created on: 27 Nov 2014
 *      Author: Brandon
 */

#ifndef CARVER_H_
#define CARVER_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include "helper.h"

int total_bytes;
int jpeg_headers;
int jpeg_footers;
float histogram_jpeg_dist[256];
float histogram_current_sector[256];

const char * file_name;
const char * output_folder_name;

FILE * file_jpeg;
FILE * file_in;


clock_t start_timer;
clock_t stop_timer;
double carving_time;

char file_jpeg_name[30];

int handle_jpeg_header(BYTE cluster_buffer[], int byte_offset);

int handle_jpeg_footer(int byte_offset);

void handle_exif_marker();

int handle_marker(BYTE cluster_buffer[], int byte_offset);

void write_sector_to_file(BYTE cluster_buffer[], int bytes_read);
int finalize();
int carve(char * file_name, char * output_folder, float entropy_threshold);

#endif /* CARVER_H_ */
