/*
 * decoding.c
 *
 *  Created on: 26 Nov 2014
 *      Author: Brandon
 */

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#include "globals.h"
#include "picojpeg.h"

unsigned char pjpeg_need_bytes_callback(unsigned char* buf, unsigned char buf_size, unsigned char *bytes_actually_read, void *callback_data)
{
	unsigned int n = min((unsigned int)(jpeg_size - jpeg_filepos), (unsigned int)buf_size);

	if (n && (fread(buf, 1, n, jpeg_to_decode) != n))
		return PJPG_STREAM_READ_ERROR;

	*bytes_actually_read = (unsigned char)(n);
	jpeg_filepos += n;

	return 0;
}

/*
 * This method is used to check if a file is a JPEG image.
 * The method returns 1 if the file is a JPEG or a 0 if the file
 * is not a JPEG
 */

int decode_jpeg(char * filename)
{
	pjpeg_image_info_t image_details;
	jpeg_to_decode = fopen(filename, "rb");

	if(!jpeg_to_decode)
		printf("JPEG file %s was not found...", filename);

	fseek(jpeg_to_decode, 0, SEEK_END);
	jpeg_size = ftell(jpeg_to_decode);
	jpeg_filepos = 0;
	fseek(jpeg_to_decode, 0, SEEK_SET);
	int status = pjpeg_decode_init(&image_details, pjpeg_need_bytes_callback, NULL, (unsigned char)1);

	if(status)
	{
		if(status == PJPG_UNSUPPORTED_MODE)
			printf("Progressive JPEG files are not supported...\n");

		return 0;
	}
	else
	{
		while(status == 0)
		{
			status = pjpeg_decode_mcu();
		}

		return 1;
	}

}
