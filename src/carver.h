/*
 * carver.h
 *
 *  Created on: 27 Nov 2014
 *      Author: Brandon Birmingham
 */

#ifndef CARVER_H_
#define CARVER_H_

#include "jpegtools.h"
#include "decoding.h"
#include "helper.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

int total_bytes;
int jpeg_headers;
int jpeg_footers;
float bfd_jpeg[256];
float bfd_cluster[256];

const char * file_name;

FILE * file_in;


clock_t start_timer;
clock_t stop_timer;
double carving_time;

void append_cluster(float[], float[], BYTE[]);

int process_header(BYTE, int);

int process_footer(int);

void handle_exif_marker();

int process_marker(BYTE[]);

void sector_analyze(BYTE[]);

int finalize();

int carve(char *);

void arrange_fragmented_file(FILE *, decoder_result, long, Thumbnail, long, int);

long get_header_offset(char *);

int remove_allocations(long);

int add_allocations(long);

bool is_sector_allocated(long);

bool is_file_embed(char *);


#endif /* CARVER_H_ */
