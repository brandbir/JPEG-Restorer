/*
 * jpegsize.h
 *
 *  Created on: 22 Mar 2015
 *      Author: brandon
 */

#ifndef JPEGSIZE_H_
#define JPEGSIZE_H_

#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/core/core.hpp>
#include <stdio.h>
typedef struct
{
	char * path;
	IplImage * image;
} Thumbnail;

int scanhead(FILE *, unsigned int *, unsigned int *);
char * enlarge_thumbnail(char *, int, int);
float compare_region_with_thumbnail(IplImage *, IplImage *,  int, int);
unsigned char * get_scanlines(IplImage *, int, int);
Thumbnail get_thumbnail(char *);


#endif /* JPEGSIZE_H_ */
