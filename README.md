# JPEG-Restorer
JPEG Restorer - A file carving technique developed in C exploiting the syntactic structure of the JPEG file format while using thumbnail affinity for semantic analysis.

*Overview*:
File carving tools carry out file recovery whenever the file-system meta-data is not available, which makes them a valuable addition to the cyber crime investigator's toolkit. Existing file carvers either cannot handle fragmented files or require a probabilistic model derived using a number of training images. This training data may not always be feasible to aggregate or its sheer size could undermine practicality. Similar to existing techniques, our method exploits both the JPEG syntax and semantic-based analysis steps in order to distinguish the correct fragments required for recovering images. The thumbnail affinity-based semantic analysis constitutes the novel aspect of this approach. Comparative evaluation using three widely used benchmark test sets show that our carver compares with the state-of-the-art commercial tool that requires an a-priori model while beating a number of popular forensic tools. This outcome demonstrates the successful replacement of the probabilistic model with thumbnail affinity, rendering this technique the right complement for existing carvers in situations where thumbnail information is readily available.

---
## User Manual:

### Part A: COMPILATION PROCESS

1. Download the ``test_set`` folder from: `https://drive.google.com/open?id=0B51b8cbLqEBHNnRqOXp4QkY5dW8`
1. Locate JPEG-Restorer/Debug

2. Make sure that the following dependencies and open-source libraries are available on your machine
  - jpeg
  - glib-2.0
  - opencv
  - tsk3

3. Execute the following command to compile the source files: `make all -f makefile`

4. If all dendency paths are specified according to the makefile, the Carver executable should be generated
---

### Part B: EXECUTING THE PROTOTYPIC FILE CARVER

1. COMMAND LINE ARGUMENTS
  - Two mandatory arguments need to be specified in order to carve files. These are:
    - "disk"    : The path of the disk image from where files need to be carved (relative to the executable)
    - "out_dir" : The path of the output directory where the carved files need to be stored (relative to the executable)

  - Optional arguments
      - -t `<threshold>` : Probability Distribution Intersection threshold for discriminating entropy and   non-entropy encoded segments during the Sequential Carving. Based on experimental analysis this is set to 0.2.
      - -i `<iterations>`: The maximum number of iterations possible to recover a fragmented file. By default this is set to 3000 but can be altered in a very highly fragmented cases. However, the more iterations used, the more time consuming the carving process will be if fragmented images cannot be recovered.
      - -s `<JPEG file>`:  The name of a fragmented file (recovered in Sequential mode) that requires further processing
      - -s `a`:  This will try to recover all the fragmented files automatically after the Sequential Carving. On the contrary, the -s <JPEG file> is intended to reconstruct specific fragmented files so that the computational expensive process will be used wisely for the interested partially-recovered JPEG files.


2. RUNNING THE JPEGCarver (Recovering files from the DFRWS-2006 test set)

  - Locate the executable file found in ``/JPEG-Restorer/Debug``
  - Create a new directory for placing the carved JPEG files (eg., create a new directory and name it "output" in: ``/JPEG-Restorer/Debug``
  - Execute the carver by invoking the following command from the terminal:

    `./Carver <disk> <out_dir>`

    __Example:__  `./Carver ../external/test_sets/disks/DFRWS-2006.raw output/2006`

    The above command reads the DFRWS-2006 test sets and places the carved files in output/2006 (Make sure that the parent folder is created in he previous step.

    The following should be displayed on the console:

    ```
    JPEGCarver Initialization...
  	Setting up environment...
   	Creating directory for the carved images
    Creating Logs Directory...
  	-----------------------------------------------------------------------
  	Preprocessing...
  	-----------------------------------------------------------------------
  	Extracting JPEG images from ../external/test_sets/disks/DFRWS-2006.raw...
  	-----------------------------------------------------------------------
  	Analyzing images...
  	Finding embedded JPEG images
  	Finding fully-recovered JPEG images
  	Finding fragmented and corrupted JPEG images
  	-----------------------------------------------------------------------
   	Carving Results:
  	Entopy Threshold used                         : 0.20
  	Number of sectors traversed                   : 97655
  	MB Traversed                                  : 47.00
  	Sector Size                                   : 512
  	Cluster Size                                  : 512
  	Total JPEG headers found                      : 14
  	Total JPEG footers found                      : 12

  	JPEGs embedded                                : 5/19
  	JPEGs corrupted                               : 1/19
  	JPEGs fragmented                              : 4/19
  	JPEGs fully recovered                         : 14/19
  	JPEGs smart-carved                            : 0/19
  	-----------------------------------------------------------------------
  	Execution time                                : 3.40 sec / 0.06 mins
  	-----------------------------------------------------------------------
    ```

   - The carved files are supposed to be automatically sorted in:
    - corrupted
    - decoded
    - fragmented
    - recovered (Recovering fragmented files from the DFRWS-2006 test set)

  - To recover fragmented JPEG files the following command needs to be executed:
    - `./Carver <disk> <out_dir> -s a`           --> to recover all the fragmented JPEG files OR
    - `./Carver <disk> <out_dir> -s <JPEG file>` --> to recover a specific fragmented file


  Example: `./Carver ../external/test_sets/disks/DFRWS-2006.raw output/2006 -s a`

  The above will replaces the previous automatically created carving directory and recovers all
  the fragmented and non fragmented JPEG files (this may take a couple of minutes).
  The following should be displayed on the console:

  ```
  JPEGCarver Initialization...
  Setting up environment...
  Creating directory for the carved images
  Directory output/2006 is going to be replaced
  Creating Logs Directory...
  -----------------------------------------------------------------------
  Preprocessing...
  -----------------------------------------------------------------------
  Extracting JPEG images from ../external/test_sets/disks/DFRWS-2006.raw...
  -----------------------------------------------------------------------
  Analyzing images...
  Finding embedded JPEG images
  Finding fully-recovered JPEG images
  Finding fragmented and corrupted JPEG images
  SmartCarving output/2006/carve14.jpg...
  SmartCarving output/2006/carve7.jpg...
  SmartCarving output/2006/carve10.jpg...
  SmartCarving output/2006/carve6.jpg...
  -----------------------------------------------------------------------
  Carving Results:
  Entopy Threshold used                         : 0.20
  Number of sectors traversed                   : 97655
  MB Traversed                                  : 47.00
  Sector Size                                   : 512
  Cluster Size                                  : 512
  Total JPEG headers found                      : 14
  Total JPEG footers found                      : 12

  JPEGs embedded                                : 5/19
  JPEGs corrupted                               : 1/19
  JPEGs fragmented                              : 4/19
  JPEGs fully recovered                         : 14/19
  JPEGs smart-carved                            : 4/19
  -----------------------------------------------------------------------
  Execution time                                : 380.63 sec / 6.34 mins
  -----------------------------------------------------------------------
  ```

  -  In order to alter the default threshold and the the maximum number of iterations that is used by the JPEG-Restorer the following command can be utilised:

  ```Example ./Carver ../external/test_sets/disks/DFRWS-2006.raw output/2006 -s a -i 20000 -t 0.5```
