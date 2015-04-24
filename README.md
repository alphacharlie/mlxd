mlxd
==========

A simple demonstration daemon for the MLX90620/90621 temperature sensors. Based upon the PIIR application by Mike Strean.

## Installation

  Requires Mike McCauley's BCM2835 library. To build, Unzip and run 'make'. The Python demo requires numpy, skimage, cv2 and some other stuff. See the mlxview.py demo 'import' lines for details.

## Usage

  Run mlxd as root. It will create the file /var/run/mlxd.sock. This FIFO conatins a buffer of the temperature data as an unsigned short int in hundredths of a degree Kelvin. To use it read the FIFO and consume in 128 byte chunks. See mlxview.py for an example.

  To test run the mlxview.py script. (Requires the usual math/imaging libraries, cv2, skimage, numpy, etc.)

## Contributing
  
  This code is a demo for educational purposes. Bugs can be reported to info@bofinry.org

## History
  0.1.0 - Initial release

## Acknowledgements

  The daemon portion of this demo is based upon the PIIR application by Mike Strean -http://github.com/strean/piir.

  The routine used to align the IR image comes from Dr. Neil Yager's excellent article on disguise detection - http://www.aicbt.com/disguise-detection/
  
  Also, as a programmer I stand on the shoulders of all those who came before me just as my code sits on top of the libraries they wrote. Thousands of people whose names I will never know helped write this... ;-)
