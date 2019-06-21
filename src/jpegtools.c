/*
 * jpegsize.c
 *
 *  Created on: 22 Mar 2015
 *      Author: Brandon Birmingham
 */

/* portions derived from IJG code */

#include "globals.h"
#include "constants.h"
#include "jpegtools.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/core/core.hpp>

#define readbyte(a,b) do if(((a)=getc((b))) == EOF) return 0; while (0)
#define readword(a,b) do { int cc_=0,dd_=0; \
						   if((cc_=getc((b))) == EOF \
						   || (dd_=getc((b))) == EOF) return 0; \
                           (a) = (cc_<<8) + (dd_); \
                         } while(0)


int scanhead(FILE * infile, unsigned int * image_width, unsigned * image_height)
{
	int marker = 0;
	int dummy = 0;

	if (getc(infile) != 0xFF || getc(infile) != 0xD8)
		return 0;

	for (;;)
	{
		int discarded_bytes = 0;
		readbyte(marker, infile);

		while (marker != 0xFF)
		{
			discarded_bytes++;
			readbyte(marker, infile);
		}

		do
			readbyte(marker, infile);
		while (marker == 0xFF);

		if (discarded_bytes != 0)
			return 0;

		switch (marker)
		{
			case 0xC0:
			case 0xC1:
			case 0xC2:
			case 0xC3:
			case 0xC5:
			case 0xC6:
			case 0xC7:
			case 0xC9:
			case 0xCA:
			case 0xCB:
			case 0xCD:
			case 0xCE:
			case 0xCF:
			{
				readword(dummy, infile); /* usual parameter length count */
				readbyte(dummy, infile);
				readword((*image_height), infile);
				readword((*image_width), infile);
				readbyte(dummy, infile);

				return 1;
				break;
			}
			case 0xDA:
			case 0xD9:
				return 0;
			default:
			{
				int length;

				readword(length, infile);

				if (length < 2)
					return 0;

				length -= 2;

				while (length > 0)
				{
					readbyte(dummy, infile);
					length--;
				}
			}

			break;
		}
	}

	return SUCCESS;
}

/**
 * This function is used to compare image region with the corresponding thumbnail region
 * Parameters: image         - used to extract the image region
 *             thumb         - used to extract the thumbnail region
 *             scanline_beg  - scanline from which the region starts
 *             num_scanlines - number of scanlines of the required region
 *
 * Returns pixel_mae - mean absolute error between the thumbnail and the corresponding
 *                     image region
 */
float compare_region_with_thumbnail(IplImage * image, IplImage * thumb,  int scanline_beg, int num_scanlines)
{
	float pixel_mae = ERROR;

	if (num_scanlines > 0 && image != NULL && thumb != NULL)
	{
		fprintf(file_logs, "Comparing from %d to %d\n", scanline_beg, scanline_beg + num_scanlines);

		cvSetImageROI(image, cvRect(0, scanline_beg, image->width, num_scanlines));
		cvSetImageROI(thumb, cvRect(0, scanline_beg, thumb->width, num_scanlines));

		/*cvNamedWindow("image", CV_WINDOW_NORMAL);
		cvShowImage("image", image);
		cvWaitKey(0);*/


		int start_offset = image->width * image->nChannels * scanline_beg;
		int end_offset = num_scanlines * image->width * image->nChannels + start_offset;

		/*Scaled Thumbnail and image for recovering should be equal*/
		if (image->imageSize == thumb->imageSize)
		{
			int width = image->roi->width;
			int height = image->roi->height;
			int channels = image->nChannels;

			float total_pixels = width * height * channels;

			int i;
			float pixel_difference = 0;

			/*Computing Mean Absolute Error*/
			for (i = start_offset; i < end_offset; i++)
			{
				int pixel_image = image->imageData[i];
				int pixel_thumb = thumb->imageData[i];

				pixel_difference += abs(pixel_image - pixel_thumb);
			}

			pixel_mae = pixel_difference/total_pixels;
		}
		else
		{
			fprintf(stderr, "Image and thumbnail size should match\n");
		}
	}

	return pixel_mae;
}

unsigned char * get_scanlines(IplImage * image, int start, int num_of_scanlines)
{
	if (image->nChannels == 3)
		cvCvtColor(image, image, CV_BGR2RGB);

	/*if (image != NULL)
	{
		cvNamedWindow("thumbnail", CV_WINDOW_NORMAL);
		cvShowImage("thumbnail", image);
		cvWaitKey(0);
	}*/
	cvSetImageROI(image, cvRect(0, start, image->width,num_of_scanlines));

	int width = image->roi->width;
	int height = image->roi->height;
	int channels = image->nChannels;
	int size = width * height * channels;

	unsigned char * scanlines = (unsigned char *) malloc(sizeof(char) * (size + 1));
	int begin_offset = (start - 1) * width * channels;
	int size_to_read = num_of_scanlines * width * channels;
	memcpy(scanlines, (image->imageData) + begin_offset, size_to_read);

	if (image->nChannels == 3)
		cvCvtColor(image,image,CV_RGB2BGR);

	cvRelease((void *) image->roi);
	return scanlines;
}

/**
 * This function is used to fist find the corresponding thumbnail of an image
 * that needs to be recovered after fragmentation. If a correspondind thumbnail
 * is found, the thumbnail is scaled according to recoverable image dimensionality
 *
 * Parameters: fragmented_image_path - the name of the image that needs to be recovered
 *             width - the width of the recoverable image
 *             height - the height of the recoverable image
 *
 * Returns: file path of the corresponding thumbnail path
 */
char * enlarge_thumbnail(char * fragmented_image_path, int width, int height)
{
	/*If dimensions are invalid a NULL pointer will be immediately returned*/
	if (width == JPEG_DIMENSION_UKNOWN  || height == JPEG_DIMENSION_UKNOWN ||
		abs(width) > JPEG_DIMENSION_MAX || abs(height) > JPEG_DIMENSION_MAX)
		return NULL;

	int out_folder = strlen(output_folder_name);
	int start_offset = out_folder + 6;
	char * ptr_frag = strchr(fragmented_image_path, '.');

	int end_offset = ptr_frag - fragmented_image_path;

	char fragmented_image_char[end_offset-start_offset + 1];
	memcpy(fragmented_image_char, fragmented_image_path + start_offset, end_offset-start_offset);

	int fragmented_image = atoi(fragmented_image_char);
	int thumbnail_image;

	DIR	* folder;
	struct dirent *dir;

	char * thumbnail_path = (char *)malloc(sizeof(char) * 100);
	char folder_recovered[100];
	sprintf(folder_recovered, "%s%s", output_folder_name, "/decoded/recovered");

	//sprintf(folder_recovered, "%s", output_folder_name);

	char * file_name_recovered;

	folder = opendir(folder_recovered);

	bool thumbnail_found = FALSE;

	if (folder)
	{
		/*Finding corresponding thumbnail JPEG image*/
		while((dir = readdir(folder)) != NULL)
		{
			file_name_recovered = dir -> d_name;
			char * ptr_thumb = strchr(file_name_recovered, '-');

			if (ptr_thumb != NULL)
			{
				int start_offset = 5;

				int end_offset = ptr_thumb - file_name_recovered;

				char thumbnail[end_offset-start_offset + 1];
				memcpy(thumbnail, file_name_recovered + start_offset, end_offset-start_offset);

				thumbnail_image = atoi(thumbnail);

				if (thumbnail_image == fragmented_image)
				{
					thumbnail_found = TRUE;
					break;
				}
			}
		}
	}

	if (thumbnail_found)
	{
		char * enlarged_thumbnail = (char *) (malloc(sizeof(char) * 100));
		sprintf(enlarged_thumbnail, "%s/%s%d%s", output_folder_name, "scaled", thumbnail_image, ".jpg");

		sprintf(thumbnail_path, "%s/%s", folder_recovered, file_name_recovered);

		IplImage * source = cvLoadImage(thumbnail_path, CV_LOAD_IMAGE_UNCHANGED);
		IplImage * destination = cvCreateImage(cvSize(width, height),source->depth, source->nChannels);
		cvResize(source, destination, CV_INTER_CUBIC);

		/*Parameter for image quality (0 - 100)*/
		int p[3];
		p[0] = CV_IMWRITE_JPEG_QUALITY;
		p[1] = 100;
		p[2] = 0;

		cvSaveImage(enlarged_thumbnail, destination, p);

		cvRelease((void *)&source);
		cvRelease((void *)&destination);

		thumbnail_path = enlarged_thumbnail;
	}
	else
	{
		thumbnail_path = NULL;
	}

	return thumbnail_path;
}

/**
 * Get the corresponding thumbnail of an image
 * Parameters: image_path - the path of the image for which thumbnail needs to
 * 							be retreived
 * Returns: thumbnail image if thumbnail with a valid dimensions exist or NULL otherwise
 */
Thumbnail get_thumbnail(char * image_path)
{
	Thumbnail thumb;


	/* Get the corresponding thumbnail if found */
	unsigned int * width = (unsigned int *) malloc(sizeof(unsigned int));
	unsigned int * height = (unsigned int *) malloc(sizeof(unsigned int));

	FILE * fragmented_file = fopen(image_path, "rw");
	IplImage * thumbnail = NULL;

	scanhead(fragmented_file, width, height);
	char * thumbnail_path = enlarge_thumbnail(image_path, *width, *height);

	if (thumbnail_path != NULL)
		thumbnail = cvLoadImage(thumbnail_path, CV_LOAD_IMAGE_UNCHANGED);

	if (fragmented_file != NULL)
		fclose(fragmented_file);

	thumb.image = thumbnail;
	thumb.path = thumbnail_path;
	return thumb;
}
