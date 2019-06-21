/*
 * runner.c
 *
 *  Created on: 27 Nov 2014
 *      Author: Brandon Birmingham
 */

#include "carver.h"
#include <string.h>
#include <unistd.h>
int main(int argc, char * argv[])
{

	char * image = argv[1];
	output_folder_name = argv[2];

	int c;

	while ((c = getopt (argc, argv, "s:t:i:")) != -1)
	{
		switch (c)
		{
			case 's':
				smart_carving_flag = TRUE;
				file_to_smart_carve = optarg;
				break;

			case 't':
				threshold_entropy = atof(optarg);
				break;

			case 'i':
				iterations_to_decode = atoi(optarg);
				break;

			default:
				abort ();
		}
	}

	return carve(image);
	return 0;
}
