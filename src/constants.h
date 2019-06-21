/*
 * constants.h
 *
 *  Created on: 19 Feb 2015
 *      Author: Brandon
 */

#ifndef CONSTANTS_H_
#define CONSTANTS_H_

//#define LOG					   1
//#define DECODER_THUMBNAIL_DEBUG  1		//analyze pixel comparison with thumbnail when available during decoding

#define SUCCESS					 0			// success from a function
#define ERROR					-1			// error from a function
#define FRAGMENTED				-2			// fragmented image
#define THUMBNAIL_FOUND			 1			// correspoding thumbnail of an image is found
#define THUMBNAIL_NOT_FOUND		 0			// corresponding thumbnail of an image was not found
#define UKNOWN					 0			// variable for representing an unknown property

#define BYTES_RANGE				 256		// range of byte values: 0 - 255

#define FILE_CLOSED				 0			// file is closed
#define FILE_OPENED				 1			// file is opened for writing
#define FILE_TO_BE_CLOSED		 2			// file is prepared to be closed

#define MARKER_NONE				-1			// no marker
#define SECTOR_SIZE				 512		// the number of bytes of a sector

#define CENTROID_SMOOTHING		 1.0		// smoothing factor for variance centroid
#define CENTROID_JPEG			 3			// centroid of jpeg position

#define CENTROID_JPEG_DIST		 1300		// JPEG fragments should have a distance < 1300 from the JPEG centroid

#define JPEG_DIMENSION_MAX		 100000000	// Maximum JPEG dimension allowed in pixels
#define JPEG_DIMENSION_UKNOWN	 -1			// Uknown JPEG dimension

#define MAE_CHECK_FIRST_CLUSTER	 1			// Mean Absolute Error Check for first cluster
#define MAE_CHECK_OTHER_CLUSTER	 2			// Mean Absolute Error Check for other clusters apart the first one
#define MAE_NO_CHECK			 0			// Mean Absolute Error No Check

#endif /* CONSTANTS_H_ */
