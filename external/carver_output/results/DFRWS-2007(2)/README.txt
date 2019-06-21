----------------------------------------------------------------------------------------
- Test set: external/test_sets/disks/DFRWS-2007.raw

- Output directory: external/carver_output/results/DFRWS-2007(2)

- Scenario: Recovering JPEG files by Sequential + Fragmented Carving

- Fragmented Carving: 2 specific fragmented files were recovered by applying fragmented carving to each
  specific fragmeneted JPEG file. The full recovery process can be performed by invoking the -s a argument, 
  but for for carving speed-up the user is allowed to carve independent files. For this case, the 2 successfully
  carved JPEG files were recovered by the the following arguments.

- Program arguments: 
	1. external/test_sets/disks/DFRWS-2007.raw external/carver_output/results/DFRWS-2007(2) -i 20000 -s carve86
	2. external/test_sets/disks/DFRWS-2007.raw external/carver_output/results/DFRWS-2007(2) -i 20000 -s carve90

Note: For this test set, the timeout to recover a single JPEG file was increased to 20,000 due to the
file fragmentation complexity found in this particular test set.

----------------------------------------------------------------------------------------
Output structure (automatic generated)

1. corrupted: contains 158 corrupted JPEG files

2. decoded
   2.1 fragmented: contains 14 fragmented/partially-decodable JPEG files
   2.2 recovered: contains 102 fully-recovered JPEG files

3. logs: log files that may be used during the carving process

4. scaled*.jpg: 8 scaled JPEG thumbnail which was used for the proposed Smart-Carving

5. recovered*.jpg: 2 fully-recovered JPEG files out of the 14 fragmented files found in 2.1

----------------------------------------------------------------------------------------






