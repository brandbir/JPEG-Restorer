/*
 * runner.c
 *
 *  Created on: 27 Nov 2014
 *      Author: Brandon
 */

#include "carver.h"
int main(int argc, char * argv[])
{
	float entropy_threshold = 0.2;
	return carve(argv[1], argv[2], entropy_threshold);
}
