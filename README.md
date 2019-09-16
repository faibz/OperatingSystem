# OperatingSystem

## Setup
### Windows

1. Install [Cygwin](https://cygwin.com/install.html).

When you get to the Select Packages screen, click on the small icon to the right of Devel. The text to the right of it should change to Install instead of Default. You can now continue with the rest of the install, accepting the rest of the defaults.

Note that Cygwin will take a long time to install, possibly well over an hour, since it pulls all of the tools down over the Internet.

2. Install [ImDisk](http://www.ltr-data.se/opencode.html/#ImDisk).

3. Install [Bochs 2.6.9](https://sourceforge.net/projects/bochs/files/bochs/2.6.9).

4. In Cygwin, navigate to the project directory and compile with 'make'.

To access a drive with its letter, use 'cd /cygdrive/[drive-letter]'.

5. In Bochs, load the 'bochsrc.bxrc' file and start the virtual machine.

'bochsrc.bxrc' is essentially a configuration for the virtual machine, which also points to the correct disk image.

## Building and running

1. Run "make" in Cygwin.

2. Run bochsrc.bxrc

## Credit

Thanks to [amrwc](https://github.com/amrwc) for letting me use his installation guide.
