/*
 * helper.h
 *
 *  Created on: 26 Nov 2014
 *      Author: Brandon
 */

#ifndef HELPER_H_
#define HELPER_H_

#include "globals.h"

void zero_array(float array[], int size);
void histogram_difference(FILE * file_out, float histogram_prev[], float histogram_next[]);
void histogram_statistics_write(float histogram_prev[], float histogram_next[], BYTE cluster_buffer[], FILE * file_jpeg);
void normalize_histogram(float histogram[], int hist_size);
void set_jpeg_uniform_distribution(float array[], int size);

int get_files_by_type(char * folder_name, char * file_type);
int validated_header_position(BYTE cluster_buffer[], int start_position);
int validate_jpeg(char * file_path, char * folder_actual);

float get_sum(float numbers[], int size);

#endif /* HELPER_H_ */
