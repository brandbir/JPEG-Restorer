----------------------------------------------------------------------------------------
- Test set: external/test_sets/disks/DFRWS-2006.raw

- Output directory: external/carver_output/results/DFRWS-2006(2)

- Scenario: Recovering JPEG files by Sequential + Fragmented Carving

- Fragmented Carving: 4 specific fragmented files were recovered by applying fragmented carving to each
  specific fragmeneted JPEG file. The full recovery process can be performed by invoking the -s a argument
  or by performing the Fragmented Carving to each respective file.

- Program arguments: 
	1. external/test_sets/disks/DFRWS-2006.raw external/carver_output/results/DFRWS-2006(2) -s a

----------------------------------------------------------------------------------------
Output structure (automatic generated)

1. corrupted: contains 4 corrupted JPEG files

2. decoded
   2.1 fragmented: contains 4 fragmented/partially-decodable JPEG files
   2.2 recovered: contains 14 fully-recovered JPEG files

3. logs: log files that may be used during the carving process

4. scaled*.jpg: 1 scaled JPEG thumbnail which was used for the proposed Fragmented Carving

5. recovered*.jpg: 4 fully-recovered JPEG files out of the 4 fragmented files found in 2.1

----------------------------------------------------------------------------------------














