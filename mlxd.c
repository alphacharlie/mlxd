/*
   simple demonstration daemon for the MLX90620 16x4 thermopile array

   Copyright (C) 2015 Chuck Werbick

   Based upon the program 'piir' by Mike Strean 

   Copyright (C) 2013 Mike Strean

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  

*/
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>
#include <signal.h>
#include <bcm2835.h>

#define VERSION "0.1.0"
#define EXIT_FAILURE 1

char *xmalloc ();
char *xrealloc ();
char *xstrdup ();

float temperatures[64];
unsigned short temperaturesInt[64];
static int usage (int status);

/* The name the program was run with, stripped of any leading path. */
char *program_name;

/* getopt_long return codes */
enum {DUMMY_CODE=129
};

/* Option flags and variables */


static struct option const long_options[] =
{
  {"help", no_argument, 0, 'h'},
  {"version", no_argument, 0, 'V'},
  {NULL, 0, NULL, 0}
};

static int decode_switches (int argc, char **argv);
int mlx90620_init ();
int mlx90620_read_eeprom ();
int mlx90620_write_config (unsigned char *lsb, unsigned char *msb);
int mlx90620_read_config (unsigned char *lsb, unsigned char *msb);
int mlx90620_write_trim (char t);
char mlx90620_read_trim ();
int mlx90620_por ();
int mlx90620_set_refresh_hz (int hz);
int mlx90620_ptat ();
int mlx90620_cp ();
float mlx90620_ta ();
int mlx90620_ir_read ();


char EEPROM[256];
signed char ir_pixels[128];

char mlxFifo[] = "/var/run/mlx90620.sock";


void got_sigint(int sig) {
    unlink(mlxFifo); 
    bcm2835_i2c_end();
    exit(0);
   
}


main (int argc, char **argv)
{
    signal(SIGINT, got_sigint);
    int fd;

    mkfifo(mlxFifo, 0666);

    int x;
    int i, j;

    float to;
    float ta;
    int vir;
    int vcp;
    float alpha;
    float vir_compensated;
    float vcp_off_comp, vir_off_comp, vir_tgc_comp;

    /* IR pixel individual offset coefficient */
    int ai;
    /* Individual Ta dependence (slope) of IR pixels offset */
    int bi;
    /* Individual sensitivity coefficient */
    int delta_alpha;
    /* Compensation pixel individual offset coefficients */
    int acp;
    /* Individual Ta dependence (slope) of the compensation pixel offset */
    int bcp;
    /* Sensitivity coefficient of the compensation pixel */
    int alphacp;
    /* Thermal Gradient Coefficient */
    int tgc;
    /* Scaling coefficient for slope of IR pixels offset */
    int bi_scale;
    /* Common sensitivity coefficient of IR pixels */
    int alpha0;
    /* Scaling coefficient for common sensitivity */
    int alpha0_scale;
    /* Scaling coefficient for individual sensitivity */
    int delta_alpha_scale;
    /* Emissivity */
    float epsilon;


    program_name = argv[0];

    i = decode_switches (argc, argv);


    printf("\n");

    if ( mlx90620_init() ) {
        printf("OK, MLX90620 init\n");
    } else {
        printf("MLX90620 init failed!\n");
        exit(1);
    }
    ta = mlx90620_ta();
    // If calibration fails then TA will be WAY too high. check and reinitialize if that happens
    while (ta > 350) 
    {
    	printf("Ta out of bounds! Max is 350, reading: %4.8f C\n", ta);
    	//out of bounds, reset and check again
    	mlx90620_init();
    	ta = mlx90620_ta();
    	usleep(10000);
    }

    printf("Ta = %4.8f C %4.8f F\n\n", ta, ta * (9.0/5.0) + 32.0);

    /* To calc parameters */

    vcp = mlx90620_cp();
    acp = (signed char)EEPROM[0xD4];
    bcp = (signed char)EEPROM[0xD5];
    alphacp = ( EEPROM[0xD7] << 8 ) | EEPROM[0xD6];
    tgc = (signed char)EEPROM[0xD8];
    bi_scale = EEPROM[0xD9];
    alpha0 = ( EEPROM[0xE1] << 8 ) | EEPROM[0xE0];
    alpha0_scale = EEPROM[0xE2];
    delta_alpha_scale = EEPROM[0xE3];
    epsilon = (( EEPROM[0xE5] << 8 ) | EEPROM[0xE4] ) / 32768.0;



    /* do the work */

    do {

        /* POR/Brown Out flag */

        while (!mlx90620_por) {
            sleep(1);
            mlx90620_init();
        }

        if ( !mlx90620_ir_read() ) exit(0);
        for ( i = 0; i < 4; i++ ) {
            for ( j = 0; j < 16; j++ ) {

                x = ((j * 4) + i); /* index */
                vir = ( ir_pixels[x*2+1] << 8 ) | ir_pixels[x*2];
                ai = (signed char)EEPROM[x];
                bi = (signed char)EEPROM[0x40 + x];
                delta_alpha = EEPROM[0x80 + x];

    		/* Calculate To */

        	vcp_off_comp = (float)vcp - ( acp + (bcp / pow(2,EEPROM[217])) * (ta - 25.0)); //256
        	vir_off_comp = (float)vir - ( ai + (bi / pow(2,EEPROM[217])) * (ta - 25.0)); //* 256
        	vir_tgc_comp = vir_off_comp - (tgc / 32) * vcp_off_comp;
        	vir_compensated = vir_tgc_comp / epsilon;
	        alpha = ((alpha0 - (tgc / 32.0) * alphacp) / pow(2, alpha0_scale)) + delta_alpha / pow(2, delta_alpha_scale);
	        to = pow(((vir_compensated / alpha) + pow((ta + 273.15), 4)), 1/4.0) - 273.15;
	        temperaturesInt[x] = (unsigned short)((to + 273.15) * 100.0) ; //give back as Kelvin (hundtredths of degree) so it can be unsigned...
	        temperatures[x] = to;

            }

        }

        fd = open(mlxFifo, O_WRONLY);
        write(fd, temperaturesInt, sizeof(temperaturesInt));
        close(fd);
        //printf("Updated Temperatures!\n");
        usleep(100000);
    } while (1);

    unlink(mlxFifo);

    exit (0);
}


/* Init */

int
mlx90620_init()
{
    if (!bcm2835_init()) return 0;
    bcm2835_i2c_begin();
    bcm2835_i2c_set_baudrate(25000);
    
    //sleep 5ms per datasheet
    usleep(5000);
    if ( !mlx90620_read_eeprom() ) return 0;
    if ( !mlx90620_write_trim( EEPROM[0xF7] ) ) return 0;
    if ( !mlx90620_write_config( &EEPROM[0xF5], &EEPROM[0xF6] ) ) return 0;
    
    mlx90620_set_refresh_hz( 4 );

    unsigned char lsb, msb;
    mlx90620_read_config( &lsb, &msb );

    return 1;
}

/* Read the whole EEPROM */

int
mlx90620_read_eeprom()
{
    const unsigned char read_eeprom[] = {
        0x00 // command
    };

    bcm2835_i2c_begin();
    bcm2835_i2c_setSlaveAddress(0x50);
    if (
        bcm2835_i2c_write_read_rs((char *)&read_eeprom, 1, EEPROM, 256)
        == BCM2835_I2C_REASON_OK
        ) return 1;

    return 0;
}

/* Write device configuration value */

int
mlx90620_write_config(unsigned char *lsb, unsigned char *msb)
{
    unsigned char lsb_check = lsb[0] - 0x55;
    unsigned char msb_check = msb[0] - 0x55;

    unsigned char write_config[] = {
        0x03, // command
        lsb_check,
        lsb[0],
        msb_check,
        msb[0]
    };

    bcm2835_i2c_begin();
    bcm2835_i2c_setSlaveAddress(0x60);
    if (
        bcm2835_i2c_write((const char *)&write_config, 5)
        == BCM2835_I2C_REASON_OK
        ) return 1;

    return 0;
}

/* Reading configuration */

int
mlx90620_read_config(unsigned char *lsb, unsigned char *msb)
{
    unsigned char config[2];

    const unsigned char read_config[] = {
        0x02, // command
        0x92, // start address
        0x00, // address step
        0x01  // number of reads
    };

    bcm2835_i2c_begin();
    bcm2835_i2c_setSlaveAddress(0x60);
    if (
        !bcm2835_i2c_write_read_rs((char *)&read_config, 4, config, 2)
        == BCM2835_I2C_REASON_OK
        ) return 0;

    *lsb = config[0];
    *msb = config[1];
    return 1;
}

/* Write the oscillator trimming value */

int
mlx90620_write_trim(char t)
{
    unsigned char trim[] = {
        0x00, // MSB
        t     // LSB
    };
    unsigned char trim_check_lsb = trim[1] - 0xAA;
    unsigned char trim_check_msb = trim[0] - 0xAA;
    
    unsigned char write_trim[] = {
        0x04, // command
        trim_check_lsb,
        trim[1],
        trim_check_msb,
        trim[0]
    };
    
    bcm2835_i2c_begin();
    bcm2835_i2c_setSlaveAddress(0x60);
    if (
        bcm2835_i2c_write((char *)&write_trim, 5)
        == BCM2835_I2C_REASON_OK
        ) return 1;
    
    return 0;
}

/* Read oscillator trimming register */

char
mlx90620_read_trim()
{
    unsigned char trim_bytes[2];

    const unsigned char read_trim[] = {
        0x02, // command
        0x93, // start address
        0x00, // address step
        0x01  // number of reads
    };
    
    bcm2835_i2c_begin();
    bcm2835_i2c_setSlaveAddress(0x60);
    if (
        bcm2835_i2c_write_read_rs((char *)&read_trim, 4, trim_bytes, 2)
        == BCM2835_I2C_REASON_OK
        ) return 1;
    
    return trim_bytes[0];
}

/* Return POR/Brown-out flag */

int
mlx90620_por()
{
    unsigned char config_lsb, config_msb;

    mlx90620_read_config( &config_lsb, &config_msb );
    return ((config_msb & 0x04) == 0x04);
}

/* Set IR Refresh rate */

int
mlx90620_set_refresh_hz(int hz)
{
    char rate_bits;
    
    switch (hz) {
        case 512:
            rate_bits = 0b0000;
            break;
        case 256:
            rate_bits = 0b0110;
            break;
        case 128:
            rate_bits = 0b0111;
            break;
        case 64:
            rate_bits = 0b1000;
            break;
        case 32:
            rate_bits = 0b1001;
            break;
        case 16:
            rate_bits = 0b1010;
            break;
        case 8:
            rate_bits = 0b1011;
            break;
        case 4:
            rate_bits = 0b1100;
            break;
        case 2:
            rate_bits = 0b1101;
            break;
        case 1:
            rate_bits = 0b1110; // default
            break;
        case 0:
            rate_bits = 0b1111; // 0.5 Hz
            break;
        default:
            rate_bits = 0b1110;
    }

    unsigned char config_lsb, config_msb;
    if ( !mlx90620_read_config( &config_lsb, &config_msb ) ) return 0;
    config_lsb = rate_bits;
    if ( !mlx90620_write_config( &config_lsb, &config_msb ) ) return 0;

    return 1;
}

/* Return PTAT (Proportional To Absolute Temperature) */

int
mlx90620_ptat()
{
    int ptat;
    unsigned char ptat_bytes[2];

    const unsigned char read_ptat[] = {
        0x02, // command
        0x90, // start address
        0x00, // address step
        0x01  // number of reads
    };

    bcm2835_i2c_begin();
    bcm2835_i2c_setSlaveAddress(0x60);
    if (
        !bcm2835_i2c_write_read_rs((char *)&read_ptat, 4, (char *)&ptat_bytes, 2)
        == BCM2835_I2C_REASON_OK
        ) return 0;

    ptat = ( ptat_bytes[1] << 8 ) | ptat_bytes[0];
    return ptat;
}

/* Compensation pixel read */

int
mlx90620_cp()
{
    int cp;
    signed char VCP_BYTES[2];

    const unsigned char compensation_pixel_read[] = {
        0x02, // command
        0x91, // start address
        0x00, // address step
        0x01  // number of reads
    };

    bcm2835_i2c_begin();
    bcm2835_i2c_setSlaveAddress(0x60);
    if (
        !bcm2835_i2c_write_read_rs((char *)&compensation_pixel_read, 4, (char *)&VCP_BYTES, 2)
        == BCM2835_I2C_REASON_OK
        ) return 0;

    cp = ( VCP_BYTES[1] << 8 ) | VCP_BYTES[0];
    return cp;
}

/* calculation of absolute chip temperature */

float
mlx90620_ta()
{
    int ptat = mlx90620_ptat();
    int vth = ( EEPROM[0xDB] << 8 ) | EEPROM[0xDA];
    float kt1 = (( EEPROM[0xDD] << 8 ) | EEPROM[0xDC]) / 1024.0;
    float kt2 = (( EEPROM[0xDF] << 8 ) | EEPROM[0xDE]) / 1048576.0;
    
    return ((-kt1 + sqrt( kt1*kt1 - (4 * kt2) * (vth - ptat) )) / (2 * kt2) ) + 25.0;
}

/* IR data read */

int
mlx90620_ir_read()
{
    const unsigned char ir_whole_frame_read[] = {
        0x02, // command
        0x00, // start address
        0x01, // address step
        0x40  // number of reads
    };

    bcm2835_i2c_begin();
    bcm2835_i2c_setSlaveAddress(0x60);
    if (
        bcm2835_i2c_write_read_rs((char *)&ir_whole_frame_read, 4, ir_pixels, 128)
        == BCM2835_I2C_REASON_OK
        ) return 1;

    return 0;
}


/* Set all the option flags according to the switches specified.
   Return the index of the first non-option argument.  */

static int
decode_switches (int argc, char **argv)
{
  int c;


  while ((c = getopt_long (argc, argv, 
			   "h"	/* help */
			   "V",	/* version */
			   long_options, (int *) 0)) != EOF)
    {
      switch (c)
	{
	case 'V':
	  printf ("mlx %s\n", VERSION);
      exit (0);

	case 'h':
	  usage (0);

	default:
	  usage (EXIT_FAILURE);
	}
    }

  return optind;
}


static int
usage (int status)
{
  printf ("%s - \
\n", program_name);
  printf ("Usage: %s [OPTION]... [FILE]...\n", program_name);
  printf ("\
Options:\n\
  -h, --help                 display this help and exit\n\
  -V, --version              output version information and exit\n\
");
  exit (status);
}
