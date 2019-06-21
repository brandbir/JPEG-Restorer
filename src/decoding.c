/*
 * decoding.c
 *
 *  Created on: 26 Nov 2014
 *      Author: Brandon Birmingham
 */

#ifndef min
#define min(a,b) (((a) < (b)) ? (a) : (b))
#endif

#define JFREAD(file,buf,sizeofbuf)  \
  ((size_t) fread((void *) (buf), (size_t) 1, (size_t) (sizeofbuf), (file)))

#include "decoding.h"
#include "constants.h"
#include "globals.h"
#include "picojpeg.h"
#include "jpegtools.h"

#include <jpeglib.h>
#include <jerror.h>
#include <setjmp.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <opencv2/highgui/highgui.hpp>


#define INPUT_BUF_SIZE	512

bool fragmented = FALSE;
char * current_file;

JDIMENSION scanlines = 0;
int current_sector = -1;
int fragment_sector = -1;
int scanline_last = -1;

JSAMPLE * scanline_prev = NULL;
JSAMPLE * scanline_mid = NULL;
JSAMPLE * scanline_next = NULL;
bool edge_detection = FALSE;

struct error_mgr
{
	//public fields
	struct jpeg_error_mgr public;

	//returning to caller
	jmp_buf setjmp_buffer;
};

typedef struct error_mgr * error_ptr;


typedef struct
{
	struct jpeg_source_mgr pub;

	FILE * infile;			/* source stream */
	JOCTET * buffer;		/* start of buffer */
	boolean start_of_file;	/* have we gotten any data yet? */
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

METHODDEF(void)
init_source (j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;

	/* We reset the empty-input-file flag for each image,
	 * but we don't clear the input buffer.
 	 * This is correct behavior for reading a series of images from one source.
 	 */

	src->start_of_file = TRUE;
}

METHODDEF(boolean)
fill_input_buffer (j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;
	size_t nbytes;

	nbytes = JFREAD(src->infile, src->buffer, INPUT_BUF_SIZE);

	if (nbytes <= 0)
	{
		/* Treat empty input file as fatal error */
		if (src->start_of_file)
			ERREXIT(cinfo, JERR_INPUT_EMPTY);

		WARNMS(cinfo, JWRN_JPEG_EOF);

		/* Insert a fake EOI marker */
		src->buffer[0] = (JOCTET) 0xFF;
		src->buffer[1] = (JOCTET) JPEG_EOI;
		nbytes = 2;
	}

	src->pub.next_input_byte = src->buffer;
	src->pub.bytes_in_buffer = nbytes;
	src->start_of_file = FALSE;

	current_sector++;
	return TRUE;
}

METHODDEF(void)
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;

	if (num_bytes > 0)
	{
		while (num_bytes > (long) src->pub.bytes_in_buffer)
		{
			num_bytes -= (long) src->pub.bytes_in_buffer;
			(void) fill_input_buffer(cinfo);
		}

		src->pub.next_input_byte += (size_t) num_bytes;
		src->pub.bytes_in_buffer -= (size_t) num_bytes;
	}
}

METHODDEF(void)
term_source (j_decompress_ptr cinfo)
{
  /* no work necessary here */
}


void jpeg_stdio_src(j_decompress_ptr cinfo, FILE * infile)
{
	current_sector = 0;
	fragment_sector = 0;
	scanline_last = 0;
	my_src_ptr src;

	if (cinfo->src == NULL )
	{
		cinfo->src = (struct jpeg_source_mgr *) (*cinfo->mem->alloc_small)(
				(j_common_ptr) cinfo, JPOOL_PERMANENT, sizeof(my_source_mgr));

		src = (my_src_ptr) cinfo->src;
		src->buffer = (JOCTET *) (*cinfo->mem->alloc_small)(
				(j_common_ptr) cinfo, JPOOL_PERMANENT,
				INPUT_BUF_SIZE * sizeof(JOCTET));
	}

	src = (my_src_ptr) cinfo->src;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.term_source = term_source;
	src->infile = infile;
	src->pub.bytes_in_buffer = 0;
	src->pub.next_input_byte = NULL;
}

void error_exit(j_common_ptr cinfo)
{
	error_ptr error = (error_ptr)cinfo->err;

	if (!edge_detection)
	{
		//displaying the message by passing cinfo as a parameter to output_message
		(cinfo->err->output_message)(cinfo);
	}

	//jump to the caller
	longjmp(error->setjmp_buffer, 1);
}

void output_message(j_common_ptr cinfo)
{
	char buffer[JMSG_LENGTH_MAX];

	//passing buffer to format message function of cinfo
	(*cinfo->err->format_message)(cinfo, buffer);

	//print buffer and mark the current file as fragmented
	fragmented = TRUE;
	fragment_sector = current_sector;
	scanline_last = scanlines;
	//printf("%s - %s at scanline:%d, sector %d\n", current_file, buffer, scanlines, current_sector);
}


/*
 * Computes the presence of an edge
 */
float edge_detector(JSAMPLE * scanline_check, JSAMPLE * scanline_prev, JSAMPLE * scanline_next, int scan_length)
{
	float integral_value = 0;
	float pixel_approximation, difference;
	int pixel;

	for (pixel = 0; pixel < scan_length; pixel++)
	{
		uint a = scanline_prev[pixel];
		uint b = scanline_check[pixel];
		uint c = scanline_next[pixel];

		pixel_approximation = (a + c) / (float)2;
		difference = fabs(b - pixel_approximation);
		integral_value += difference;
	}

	return integral_value /scan_length;
}

/*
 * This function is used to try to decode a partiular jpeg file. It also checks
 * if the jpeg is a fully recovered jpeg image or if the image is fragmented.
 * Parameters: file_path      - the path of the file image
 * 			   edge_detection - whether or not edge detection is applied on the decoded image
 */
decoder_result decode_jpeg(char * file_path, bool edge_det, IplImage * thumbnail)
{
	decoder_result result;
	result.decoder_type = ERROR;
	result.decoder_sector = -1;
	result.decoder_scanline = -1;
	result.thumb_sector = -1;
	result.thumb_scanline = -1;

	float prev_mae = -1;
	float prev_mse = -1;
	int prev_sector = -1;
	int prev_scanline = -1;

	bool found_threshold = FALSE;

	int ret = SUCCESS;
	fragmented = FALSE;
	current_file = file_path;
	scanlines = 0;
	float edge = 0;
	bool stop = FALSE;
	edge_detection = edge_det;

	FILE * file_input = NULL;

	struct jpeg_decompress_struct cinfo;
	struct error_mgr jerr;

	/*output row buffer*/
	JSAMPARRAY buffer;

	/*physical row width in image buffer*/
	int row_width;

	/*initialize the JPEG decompression object with default error handling*/
	cinfo.err = jpeg_std_error(&jerr.public);
	jerr.public.error_exit = error_exit;
	jerr.public.output_message = output_message;

	if (setjmp(jerr.setjmp_buffer))
	{
		jpeg_destroy_decompress(&cinfo);
		fclose(file_input);
		result.decoder_type = ERROR;
		result.decoder_sector = fragment_sector;
		result.decoder_scanline = scanline_last;
		return result;
	}

	jpeg_create_decompress(&cinfo);

	file_input = fopen(file_path, "rb");

	if (file_input == NULL)
		fprintf(stderr, "Unable to open %s for decoding\n", file_path);

	/*Specifying data source for decompression*/
	jpeg_stdio_src(&cinfo, file_input);

	/*Read file header and set default decompression parameters*/
	jpeg_read_header(&cinfo, TRUE);

	/* Once the jpeg start decompress is finished the correct scaled output image
	 * dimensions and the output colour map will be available
	 */
	jpeg_start_decompress(&cinfo);

	/*number of components should be always 3 or 1*/
	row_width = cinfo.output_width * cinfo.output_components;
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_width, 1);

	while (cinfo.output_scanline < cinfo.output_height && !stop)
	{
		scanlines += jpeg_read_scanlines(&cinfo, buffer, 1);

		/*Compare scanline with the corresponding thumbnail scanline*/

		if (thumbnail != NULL && scanlines < cinfo.output_height)
		{
			int i;
			float absolute_error = 0;
			float squared_error = 0;

			unsigned char * scanline_thumbnail = get_scanlines(thumbnail, scanlines, 1);

			#ifdef DECODER_THUMBNAIL_DEBUG
				int j;

				for (j = 0; j < 30; j++)
				{
					int pixel_thumbnail = scanline_thumbnail[j];
					fprintf(file_logs, "%d ", pixel_thumbnail);
				}

				fprintf(file_logs,"\n");

				for (j = 0; j < 30; j++)
				{
					int pixel_image = (*buffer)[j];
					fprintf(file_logs, "%d ", pixel_image);
				}

				fprintf(file_logs,"\n");
			#endif

			/*Compute the Mean Square Error between thumbnail and recovering image*/
			for (i = 0; i < row_width; i++)
			{
				int pixel_thumbnail = scanline_thumbnail[i];
				int pixel_image = (*buffer)[i];

				float pixel_difference = pixel_thumbnail - pixel_image;

				absolute_error += abs(pixel_difference);
				squared_error += pow(pixel_difference, 2);
			}

			float mae = absolute_error / row_width;
			float mse = squared_error / row_width;
			free(scanline_thumbnail);

			if (prev_mae != -1 && prev_mse != -1)
			{
				float diff_mae = mae - prev_mae;

				if (diff_mae > 15 && found_threshold == FALSE)
				{

					#ifdef DECODER_THUMBNAIL_DEBUG
						fprintf(file_logs, "%s : ***************************************** - %d\n", file_path, prev_sector);
					#endif

					result.thumb_sector = prev_sector - 1;
					result.thumb_scanline = prev_scanline;
					found_threshold = TRUE;
				}
			}

			prev_mae = mae;
			prev_mse = mse;
			prev_sector = current_sector;
			prev_scanline = scanlines;

			#ifdef DECODER_THUMBNAIL_DEBUG
				fprintf(file_logs, "%d\t%d\t%.2f\t%.2f\n", scanlines, current_sector, mae, mse);
			#endif
		}

		if (edge_detection)
		{
			switch (scanlines)
			{
				case 1:
					scanline_prev = (JSAMPLE *)malloc(sizeof(JSAMPLE) * (row_width + 1));
					memcpy(scanline_prev, buffer[0], row_width);
					break;

				case 2:
					scanline_mid = (JSAMPLE *)malloc(sizeof(JSAMPLE) * (row_width + 1));
					memcpy(scanline_mid, buffer[0], row_width);
					break;

				case 3:
					scanline_next = (JSAMPLE *)malloc(sizeof(JSAMPLE) * (row_width + 1));
					memcpy(scanline_next, buffer[0], row_width);
					break;

				default:
					/*compute integral for edge detection and then update positions*/
					edge = edge_detector(scanline_mid, scanline_prev, scanline_next, row_width);

					if(edge >= 10)
					{
						fprintf(file_logs,"%s - Edge detected at scanline %d (%.2f)\n", file_path, scanlines - 2, edge);
						stop = TRUE;
					}

					memcpy(scanline_prev, scanline_mid, row_width);
					memcpy(scanline_mid, scanline_next, row_width);
					memcpy(scanline_next, buffer[0], row_width);
					break;
			}
		}
	}

	//finish decompression
	jpeg_finish_decompress(&cinfo);

	//deallocation of memory
	jpeg_destroy_decompress(&cinfo);

	if (edge_detection)
	{
		free(scanline_prev);
		free(scanline_mid);
		free(scanline_next);
	}

	fclose(file_input);

	if(fragmented)
		ret = FRAGMENTED;

	result.decoder_type = ret;
	result.decoder_sector = fragment_sector;
	result.decoder_scanline = scanline_last;
	return result;
}
