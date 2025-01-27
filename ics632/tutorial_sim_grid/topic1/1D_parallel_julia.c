#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <mpi.h>

int compute_julia_pixel(int x, int y, int width, int height, float tint_bias, unsigned char *rgb);
int write_bmp_header(FILE *f, int width, int height);


/*
 * compute_julia_pixel(): compute RBG values of a pixel in a 
 *                        particular Julia set image.
 *
 *  In:
 *	(x,y):            pixel coordinates	
 *	(width, height):  image dimensions
 *	tint_bias:	  a float to "tweak" the tint (1.0 is "no additional tint")
 *  Out:
 *	rgb: an already-allocated 3-byte array into which R, G, and B 
 *	     bytes are written.
 *
 *  Return:
 *  	0 in success, -1 on failure
 *
 */


int compute_julia_pixel(int x, int y, int width, int height, float tint_bias, unsigned char *rgb) {

  // Check coordinates
  if ((x < 0) || (x >= width) || (y < 0) || (y >= height)) {
    fprintf(stderr,"Invalid (%d,%d) pixel coordinates in a %d x %d image\n", x, y, width, height);
    return -1;
  }

  // "Zoom in" to a particular section of the Mandelbrot set
  float X_MIN = -1.6, X_MAX = 1.6, Y_MIN = -0.9, Y_MAX = +0.9;
  float float_y = (Y_MAX - Y_MIN) * (float)y / height + Y_MIN ;
  float float_x = (X_MAX - X_MIN) * (float)x / width  + X_MIN ;

  // Point that defines the Julia set
  float julia_real = -.79;
  float julia_img = .15;

  // Maximum of iteration before we determine "did not converge"
  int max_iter = 300;

  // Computing the complex series convergence
  float real=float_y, img=float_x;
  int num_iter = max_iter;
  while (( img * img + real * real < 2 * 2 ) && ( num_iter > 0 )) {
    float xtemp = img * img - real * real + julia_real;
    real = 2 * img * real + julia_img;
    img = xtemp;
    num_iter--;
  }
  //
  // Paint pixels based on how many iterations were used, using some funky colors
  float color_bias = (float) num_iter / max_iter;

  rgb[0] = (num_iter == 0 ? 200 : - 500.0 * pow(tint_bias, 1.2) *  pow(color_bias, 1.6)); 
  rgb[1] = (num_iter == 0 ? 100 : -255.0 *  pow(color_bias, 0.3));
  rgb[2] = (num_iter == 0 ? 100 : 255 - 255.0 * pow(tint_bias, 1.2) * pow(color_bias, 3.0));

  return 0;
}


/* write_bmp_header():
 *
 *   In:
 *      f: A file open for writing ('w') 
 *      (width, height): image dimensions
 *   
 *   Return:
 *      0 on success, -1 on failure
 *
 */

int write_bmp_header(FILE *f, int width, int height) {

  unsigned int adjusted_width = width + (width % 4 == 0 ? 0 : (4 - width % 4));

  // Define all fields in the bmp header
  char id[2] = "BM";
  unsigned int filesize = 54 + (int)(adjusted_width * height * 3 * sizeof(char));
  short reserved[2] = {0,0};
  unsigned int offset = 54;

  unsigned int size = 40;
  unsigned short planes = 1;
  unsigned short bits = 24;
  unsigned int compression = 0;
  unsigned int image_size = width * height * 3 * sizeof(char);
  int x_res = 0;
  int y_res = 0;
  unsigned int ncolors = 0;
  unsigned int importantcolors = 0;

  // Write the bytes to the file, keeping track of the
  // number of written "objects"
  size_t ret = 0;
  ret += fwrite(id, sizeof(char), 2, f);
  ret += fwrite(&filesize, sizeof(int), 1, f);
  ret += fwrite(reserved, sizeof(short), 2, f);
  ret += fwrite(&offset, sizeof(int), 1, f);
  ret += fwrite(&size, sizeof(int), 1, f);
  ret += fwrite(&width, sizeof(int), 1, f);
  ret += fwrite(&height, sizeof(int), 1, f);
  ret += fwrite(&planes, sizeof(short), 1, f);
  ret += fwrite(&bits, sizeof(short), 1, f);
  ret += fwrite(&compression, sizeof(int), 1, f);
  ret += fwrite(&image_size, sizeof(int), 1, f);
  ret += fwrite(&x_res, sizeof(int), 1, f);
  ret += fwrite(&y_res, sizeof(int), 1, f);
  ret += fwrite(&ncolors, sizeof(int), 1, f);
  ret += fwrite(&importantcolors, sizeof(int), 1, f);

  // Success means that we wrote 17 "objects" successfully
  return (ret != 17);
}

/*
 * main() function
 */

int main(int argc, char **argv) {

  // Parse command-line arguments
  int height;
  MPI_Init(&argc, &argv);
  if ((argc != 2) || (sscanf(argv[1],"%d",&height) != 1)) {
    fprintf(stderr,"Usage: %s <height (int)>\n",argv[0]);
    exit(1);
  }

  int myrank, num_procs;
  MPI_Comm_rank(MPI_COMM_WORLD, &myrank);
  MPI_Comm_size(MPI_COMM_WORLD, &num_procs);
  int remainder =height % num_procs;
  int start = myrank*height/num_procs; 
  int stop = (myrank+1)*height/num_procs+(myrank<remainder)-1;
  //printf("[Process %d out of %d]: I should compute pixel rows %d to %d, for a total of %d rows\n", myrank, num_procs, start, stop, height);

  // Allocate array
  int width = height * 2;
  //unsigned char *pixels = (unsigned char *)calloc(height * width * 3, sizeof(char));
  unsigned char *pixels = (unsigned char *)calloc((stop-start+1) * width * 3, sizeof(char));

  // Compute all pixels
  int y,x;
  for (y=0; y < stop-start+1; y++) {
    for (x=0; x < width; x++) {
      compute_julia_pixel(x, start+y, width, height, 1.0, &(pixels[y * 3 * width + x * 3]));
    }
  }


  // Open file for writing
  FILE *f;
  int data[1];
  data[0] = 1;
  MPI_Status status;
  if(myrank==0){
     f = fopen("./julia.bmp", "w");

     // Write header
     if (write_bmp_header(f, width, height)) {
        perror("Error writing header: ");
        exit(0);
     }
  }else{
     MPI_Recv(data,1,MPI_INT, myrank -1,0,MPI_COMM_WORLD, &status);
     f = fopen("./julia.bmp", "a");
  }

  // Write the pixels
  int p;
  for (y=0; y < stop-start+1; y++) {
    for (x=0; x < width; x++) {
         fwrite(&(pixels[y * 3 * width + x * 3]), sizeof(char), 3, f);

         // padding in case of an even number of pixels per row
         unsigned char padding[3] = {0,0,0};
         fwrite(padding, sizeof(char), ((width * 3) % 4), f);
    }
  }

  // Close the file
  fclose(f);

  if (myrank != num_procs-1) {
     MPI_Send(data,1,MPI_INT, myrank+1,0,MPI_COMM_WORLD);
  }

  MPI_Finalize();
  return 0;
}


