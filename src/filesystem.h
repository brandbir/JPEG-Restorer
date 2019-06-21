/*
 * filesystem.h
 *
 *  Created on: 14 Mar 2015
 *      Author: brandon
 */

#ifndef FILESYSTEM_H_
#define FILESYSTEM_H_

int fs_get_value(FILE *, const char *);
int fs_get_header_case_info_size (FILE *);
FILE * fs_discard_crc(FILE *, long, int);


#define FS_CLUSTER_SIZE			"Cluster Size"					//cluster size in bytes
#define FS_SECTOR_SIZE			"Sector Size"					//sector size in bytes
#define FS_CLUSTER_AREA_START	"Cluster Area"					//sector at which cluster area starts
#define FS_SECTORS_BEFORE_FS	"Sectors before file system"	//sectors encountered before filesystem begins

#endif /* FILESYSTEM_H_ */
