/*
 * decoding.h
 *
 *  Created on: 26 Nov 2014
 *      Author: Brandon
 */

#ifndef DECODING_H_
#define DECODING_H_

#include "globals.h"
#include <opencv2/core/core.hpp>


typedef struct
{
	int decoder_type;
	int decoder_sector;
	int decoder_scanline;
	int thumb_sector;
	int thumb_scanline;
} decoder_result;

unsigned char pjpeg_need_bytes_callback(unsigned char*, unsigned char, unsigned char *, void *);
decoder_result decode_jpeg(char *, bool, IplImage *);


#endif /* DECODING_H_ */
