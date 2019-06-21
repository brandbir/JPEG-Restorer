/*
 * helper.h
 *
 *  Created on: 26 Nov 2014
 *      Author: Brandon
 */

#ifndef HELPER_H_
#define HELPER_H_

#include "constants.h"
#include "globals.h"

void bfd_init(float[]);
void intersection_analyze(FILE *, float[], float[]);
void normalize_bfd();
void bfd_uniform(float[]);

int get_files_by_type(char *, char *);
bool is_embedded_header(BYTE[], int);
int validate_jpeg(char *, char *);

float get_sum(float[], int);

void print_centroids(float [num_file_types][SECTOR_SIZE]);
char ** load_types(char[]);
int get_number_of_centroids(char[]);
int load_centroids(char [], float[num_file_types][SECTOR_SIZE], int, int);
Centroid get_centroid(float[], float[num_file_types][SECTOR_SIZE], float[num_file_types][SECTOR_SIZE]);
void asserting_used_sectors_dfrws_2006();
void asserting_used_sectors_canon_gen6();
void asserting_embed_files();

#endif /* HELPER_H_ */
