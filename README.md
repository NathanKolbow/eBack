# eBack
A Linux utility meant to make large-scale, local external backups easier; leverages multiple drives in parallel for larger, faster data transfers.  The utility also does not cross filesystem boundaries, so you don't have to worry about overflow from things like NFS's or miscellaneous bind mounts.  Some support for btrfs file systems comes built-in (see **How It Works**).

## Installation

``` block
git clone https://github.com/NathanKolbow/eBack.git
cd eBack
sudo ./install
```
The main eback bash script depends on various programs, most of which come prepackaged with Xenial, but some miscellaneous dependencies may need to be installed.  This utility is only meant for Ubuntu Xenial, so proper installation is not guaranteed on any other OS.

## Usage
#### Command-line arguments:

* -f: forces the program to rescan for devices
* -s: forces the program to skip scanning for devices
* -B: performs a backup based on the existing configuration
* -R: opens an interactive session that helps with file recovery

-R, -f and -s lead to an interactive shell, but -B simply performs the configured backup before quitting.

#### Shell commands:

* A <device #>: Add a device to the backup system with the device # listed from the L command
* R <device #>: Remove a device from the backup system with the device # listed from the D command
* L: Lists the devices that are currently visible to eBack
* D: Lists the devices that are currently registered in the backup system
* a <directory>: Adds a directory to the backup system
* r <directory>: Removes a directory from the backup system
* s: Rescan available external devices
* h: Displays the info message
* l: Prints when the last backup occurred
* q: Quits the shell
  
#### Formatting:

Devices can only be added to the eback system if they are properly formatted (via e_format).  This formatting completely clears the provided disk segment and allows eback to recognize that segment.  This is accomplished with the command *e_format /dev/\<device\>*.


## How It Works
Two main lists are created via the interactive shell:
1. A list of directories to be backed up
2. A list of devices (stored in the form of UUIDs) that may be used for backups

When the list of directories is backed up with *eback -B*, each of the directories in list (1) have their contents copied across each device in list (2); as mentioned above, directory contents will only be recursively copied if those contents exist on the same filesystem as their parent directory.

Each of the lists can be adjusted at will (either through the interactive shell, or manually in the /etc/ config files), but data will **not** be removed from the devices as a result of changing the lists.  Existing data will only be changed on the backup drives when a file is overwritten by a new version of itself; this means that devices may fill up past the capacity of the directory that they are backing up, so manual maintenance of drives may be required every so often.

This utility uses sendfile(...) to optimize transfer speeds, which varies by OS, so the utility is not meant to be portable outside of Ubuntu Xenial.


#### btrfs Caveat
btrfs has very particular and unique ways of handling things, so for various reasons the following conditions exist:
1. If you backup a directory that is **inside of** a btrfs volume of subvolume, separate subvolumes deeper within the directory's tree will **not** be backed up
2. If you backup a directory that is the **head** of a btrfs **volume**, all subvolumes deeper within the directory's tree **will** be backed up


## Utilities
The main functions of this program are accessed through the eback script, but the following utilities are also included:
1. e_format - Formats the given drive to XFS and creates a stamp that lets eback know the drive can be used in the backup system.
2. e_verify - Mostly for internal use, but you can use it to double-check that the format command worked on any given drive.
