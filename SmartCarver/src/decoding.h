/*
 * decoding.h
 *
 *  Created on: 26 Nov 2014
 *      Author: Brandon
 */

#ifndef DECODING_H_
#define DECODING_H_


unsigned char pjpeg_need_bytes_callback(unsigned char* buf, unsigned char buf_size, unsigned char *bytes_actually_read, void *callback_data);
int decode_jpeg(char * filename);

#endif /* DECODING_H_ */
