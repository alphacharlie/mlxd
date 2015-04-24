mlxd
==========

A simple demonstration daemon for the MLX90620/90621 temperature sensors. Based upon the PIIR application by Mike Strean.

## Installation

  Requires Mike McCauley's BCM2835 library. To build, Unzip and run 'make'.

## Usage

  Run mlxd as root. It will create the file /var/run/mlxd.sock. This FIFO conatins the temperature data as an unsigned short int in hundredths of a degree Kelvin.

  To test run the mlxview.py script. (Requires the usual math/imaging libraries, cv2, skimage, numpy, etc.)

## Contributing
  
  This code is a demo for educational purposes. Bugs can be reported to info@bofinry.org

## History
  0.1.0 _ Initial release

## Acknopwledgements

  THe daemon portion of this demo is based upon the PIIR application, http://github.com/strean/piir.

  As a programmer I stand on the shoulders of those who came before me as my code lies on top of the libraries they wrote. Thousands of people whose names I will never know helped write this...
