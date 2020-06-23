# eBack
A Linux utility meant to leverage multiple external drives in parallel in order to perform faster large-scale data backups.

## Usage
<b>Command-line arguments:</b>

* -f: forces the program to rescan for devices
* -s: forces the program to skip scanning for devices
* -B: performs a backup based on the existing configuration

-f and -s lead to an interactive shell, but -B simply performs the configured backup before quitting.

<b>Shell commands:</b>

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


## How It Works
< To-Do >


## Utilities
The main functions of this program are accessed through the eback script, but the following utilities are also included:
1. format - Formats the given drive to XFS and creates a stamp that lets eback know the drive can be used in the backup system.
2. verify - Mostly for internal use, but you can use it to double-check that the format command worked on any given drive.
