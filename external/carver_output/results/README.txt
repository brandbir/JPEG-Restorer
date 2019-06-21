----------------------------------------------------------------------------------------
- Test set: external/test_sets/disks/DFRWS-2007.raw

- Output directory: external/carver_output/results/DFRWS-2007(2)

- Scenario: Recovering JPEG files by Sequential and Fragmented Carving

- Fragmented Carving: 2 specific files individually (carve86 + carve90) 
  The other fragmented JPEG files were not sucessfully recovered after letting the file carver for several amount of time.
  carve86.jpg and carve90.jpg were recovered by invoking the additional -s and -i arguments as specified below.

- Program arguments: 
	1. external/test_sets/disks/DFRWS-2007.raw external/carver_output/results/DFRWS-2007(2) -i 20000 -s carve86
	2. external/test_sets/disks/DFRWS-2007.raw external/carver_output/results/DFRWS-2007(2) -i 20000 -s carve90
----------------------------------------------------------------------------------------
Output structure

1. corrupted: contains 158 corrupted JPEG files

2. decoded
   2.1 fragmented: contains 14 fragmented/partially-decodable JPEG files
   2.2 recovered: contains 102 fully-recovered JPEG files

3. logs: log files that may be used during the carving process

4. scaled*.jpg: 8 scaled JPEG thumbnail which was used for the proposed Smart-Carving

5. recovered*.jpg: 2 recovered JPEG out of the 14 fragmented files found in 2.1

----------------------------------------------------------------------------------------
Console output when executed on an intel Core i5 2.5GHz machine (OS: Ubuntu 14.04) : 

Sequential Carving + Smart-Carving Carve86.jpg with command arguments: 
=> external/test_sets/disks/DFRWS-2007.raw external/carver_output/results/DFRWS-2007(2) -i 20000 -s carve86
   where -i  20000   -> number of iterations to be performed to recover a single JPEG
         -s carve86  -> smart carving carve86.jpg that is found in the fragmented (2.2) folder

	JPEGCarver Initialization...
	Setting up environment...
	Creating directory for the carved images
	Creating Logs Directory...
	-----------------------------------------------------------------------
	Preprocessing...
	-----------------------------------------------------------------------
	Extracting JPEG images from external/test_sets/disks/DFRWS-2007.raw...
	-----------------------------------------------------------------------
	Analyzing images...
	Finding embedded JPEG images
	Finding fully-recovered JPEG images
	Finding fragmented and corrupted JPEG images
	SmartCarving external/carver_output/results/DFRWS-2007(2)/carve86.jpg...
	-----------------------------------------------------------------------
	Carving Results:
	Entopy Threshold used                         : 0.20
	Number of sectors traversed                   : 677677
	MB Traversed                                  : 330.00
	Sector Size                                   : 512
	Cluster Size                                  : 512
	Total JPEG headers found                      : 154
	Total JPEG footers found                      : 149

	JPEGs embedded                                : 120/274
	JPEGs corrupted                               : 158/274
	JPEGs fragmented                              : 14/274
	JPEGs fully recovered                         : 102/274
	JPEGs smart-carved                            : 1/274
	-----------------------------------------------------------------------

Sequential Carving + Smart-Carving Carve90.jpg with command arguments: 
=> external/test_sets/disks/DFRWS-2007.raw external/carver_output/results/DFRWS-2007(2) -i 20000 -s carve90
   where -i 20000   -> number of iterations to be performed to recover a single JPEG
         -s carve90 -> smart carving carve90.jpg that is found in the fragmented (2.2) folder

	JPEGCarver Initialization...
	Setting up environment...
	Creating directory for the carved images
	Directory external/carver_output/results/DFRWS-2007(2) is going to be replaced
	Creating Logs Directory...
	-----------------------------------------------------------------------
	Preprocessing...
	-----------------------------------------------------------------------
	Extracting JPEG images from external/test_sets/disks/DFRWS-2007.raw...
	-----------------------------------------------------------------------
	Analyzing images...
	Finding embedded JPEG images
	Finding fully-recovered JPEG images
	Finding fragmented and corrupted JPEG images
	SmartCarving external/carver_output/results/DFRWS-2007(2)/carve90.jpg...
	-----------------------------------------------------------------------
	Carving Results:
	Entopy Threshold used                         : 0.20
	Number of sectors traversed                   : 677677
	MB Traversed                                  : 330.00
	Sector Size                                   : 512
	Cluster Size                                  : 512
	Total JPEG headers found                      : 154
	Total JPEG footers found                      : 149

	JPEGs embedded                                : 120/274
	JPEGs corrupted                               : 158/274
	JPEGs fragmented                              : 14/274
	JPEGs fully recovered                         : 102/274
	JPEGs smart-carved                            : 1/274
	-----------------------------------------------------------------------


