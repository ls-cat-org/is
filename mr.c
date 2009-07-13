#include "is.h"

static jmp_buf j_jumpHere;

extern void marTiff2jpeg( isType *is);
extern void marTiff2profile( isType *is);

void marTiff( isType *is) {
  if( strcmp( is->cmd, "jpeg") == 0) {
    marTiff2jpeg( is);
  } else {
    if( strcmp( is->cmd, "profile") == 0)
    marTiff2profile( is);
  }
}






unsigned short nearestValue( isType *is, unsigned short *buf, double k, double l) {
  return *(buf + (int)(k+0.5)*(is->inWidth)+(int)(l+0.5));
}


/*
** returns the maximum value of ha xa by ya box centered on d,l
*/
unsigned short maxBox( isType *is, unsigned short *buf, double k, double l, int yal, int yau, int xal, int xau) {
  int m, n;
  unsigned short d, d1;

  d = 0;

  for( m=k-yal; m < k+yau; m++) {
    for( n=l-xal; n<l+xau; n++) {
      d1 = *(buf + m*(is->inWidth) + n);
      d = (d>d1 ? d : d1);
    }
  }

  return d;
}

unsigned short *marTiffRead( isType *is) {
  TIFF *tf;
  tsize_t sls;
  unsigned short *buf;
  int i;

  TIFFSetErrorHandler( NULL);		// surpress annoying error messages 
  TIFFSetWarningHandler( NULL);		// surpress annoying warning messages 
  tf = TIFFOpen( is->fn, "r");		// open the file
  if( tf == NULL) {
    fprintf( stderr, "marTiffRead failed to open file '%s'\n", is->fn);
    return NULL;
  }
  TIFFGetField( tf, TIFFTAG_IMAGELENGTH,   &(is->inHeight));
  TIFFGetField( tf, TIFFTAG_IMAGEWIDTH,    &(is->inWidth));

  sls = TIFFScanlineSize( tf);
  buf  = malloc( sls * is->inHeight);
  if( buf == NULL) {
    fprintf( stderr, "marTiffRead: Out of memory.  malloc(%d) failed\n", sls * is->inHeight);
    return NULL;
  }
    
  //
  // read the image
  //

  for( i=0; i<is->inHeight; i++) {
    TIFFReadScanline( tf, buf + i*(is->inWidth), i, 0);
  }
  
  //
  // we are done
  //
  TIFFClose( tf);
  
  return buf;

}


void jerror_handler( j_common_ptr cp) {
  fprintf( stderr, "Is that socket still there?  I think not!\n");  
  longjmp( j_jumpHere, 1);
}


void marTiff2jpeg( isType *is ) {
  struct jpeg_compress_struct cinfo;
  int cinfoSetup;
  struct jpeg_error_mgr jerr;
  unsigned short *buf;
  unsigned char  *bufo;
  unsigned char  *bp;
  unsigned short d;
  unsigned char dout;
  unsigned int rslt;
  int i, j;
  int jmin, jmax;
  double k, l;
  int ya, xa;
  int yal, yau, xal, xau;
  JSAMPROW jsp[1];


  cinfoSetup = 0;
  buf = NULL;
  bufo = NULL;

  if( setjmp( j_jumpHere)) {
    if( cinfoSetup)
      jpeg_destroy_compress( &cinfo);
    if( buf != NULL)
      free( buf);
    if( bufo != NULL)
      free( bufo);

    return;
  }
  

  //
  // get the data
  buf = marTiffRead( is);

  cinfo.err = jpeg_std_error(&jerr);
  cinfo.err->error_exit = jerror_handler;
  jpeg_create_compress(&cinfo);
  cinfoSetup = 1;

  jpeg_stdio_dest(&cinfo, is->fout);
  cinfo.image_width = is->xsize;		/* image width and height, in pixels */
  cinfo.image_height = is->ysize;

  cinfo.input_components = 3;		/* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB;	/* colorspace of input image */

  jpeg_set_defaults(&cinfo);
  jpeg_set_quality( &cinfo, 100, TRUE);

  jpeg_start_compress(&cinfo, TRUE);

  bufo = calloc( 3 * is->ysize * is->xsize, sizeof(unsigned char) );
  if( bufo == NULL) {
    fprintf( stderr, "marTiff2jpeg: Out of memory.  calloc(%d) failed\n", 3*is->ysize*is->xsize*sizeof( unsigned char));
    return;
  }

  //
  // size of rectangle to search for the maximum pixel value
  // yal and xal are subtracted from ya and xa for the lower bound of the box and
  // yau and xau are added to ya and xa for the upper bound of the box
  //
  ya = (is->height)/(is->ysize);
  xa = (is->width)/(is->xsize);
  yal = yau = ya/2;
  if( (yal + yau) < ya)
    yau++;

  xal = xau = xa/2;
  if( (xal+xau) < xa)
    xau++;

  //
  // compute the range of j, ignoring for now the j's that, considering the box, would lead to pixels off the input image
  //

  jmin = -(is->x)*(is->xsize)/(is->width) + xal;
  jmax = ((is->inWidth)-(is->x)) * (is->xsize)/(is->width) - xau + 1;
  if( jmin < 0)
    jmin = 0;
  if( jmax > is->xsize)
    jmax = is->xsize;

  //
  // loop over pixels in the output image
  //
  // i index over output image height (is->ysize)
  // j index over output image width  (is->xsize)
  //
  // double k maps i into the input image
  // double l maps j into the input image
  //

  for( i=0; i< is->ysize; i++) {
    //
    // map pixel vert index to pixel index in input image
    //
    k = (i * is->height)/(double)(is->ysize) + is->y;

    if( k-yal >= -0.5 && k+yau < (is->inHeight)-0.5) {
      for( j=jmin; j<jmax; j++) {
	//
	// map pixel horz index to pixel index in intput image
	//
	l = j * is->width/(double)(is->xsize) + is->x;
	//if( (int)(l+0.5) < 0  || (int)(l+0.5) >= is->inWidth)
	//continue;

	//
	// default pixel value is 0;
	//
	d = 0;
      
	if( ya <= 1 && xa <= 1) {
	  //
	  //  If we are on the orginal image, get the nearest pixel value
	  //
	  d = nearestValue( is, buf, k, l);
	} else {
	  //
	  // Look around for the maximum value when the ouput image is
	  // being reduced
	  //
	  d = maxBox( is, buf, k, l, yal, yau, xal, xau);
	}
      
	if( d <= is->wval) {
	  dout = 0;
	} else {
	  if( d >= is->contrast) {
	    dout = 255;
	  } else {
	    rslt = (d - is->wval) * 255;
	    dout = rslt/(is->contrast - is->wval);
	  }
	}
      
	bp = bufo + 3*i*(is->xsize) + 3*j;
	if( d==65535) {
	  *(bp++) = 255;
	  *(bp++) = 0;
	  *bp     = 0;
	} else {
	  *(bp++)     = 255 - dout;
	  *(bp++)     = 255 - dout;
	  *(bp)       = 255 - dout;
	}
      }
    }
    
    jsp[0] = bufo + 3*i*(is->xsize);
    jpeg_write_scanlines(&cinfo, jsp, 1);
  }
  jpeg_finish_compress(&cinfo);

  //
  // don't forget to free the memory!
  free( buf);
  free( bufo);
}



void marTiff2profile( isType *is) {
  double k, l;		// double version of row (k), column (l) indices in the input image
  double mk, bk, ml, bl;  // slope and offset of map from s to k and l
  int s;		// parametric 'index' along line
  int smin, smax;	// limit of s
  int n;		// number of points to plot
  unsigned short *buf;	// our image buffer
  unsigned short *maxs; // array of maxima from the images
  unsigned short mx, mn;  // max and min for graph scaling

  buf = marTiffRead( is);

  // compute distance in input image to traverse and add one to get number of points from one end to the other
  n = sqrt( (double)(is->pbx - is->pax)*(is->pbx - is->pax) + (is->pby - is->pay)*(is->pby - is->pay)) + 1;

  maxs = (unsigned short *)calloc( n, sizeof( unsigned short));
  if( maxs == NULL) {
    fprintf( stderr, "marTiff2profile: out of memory %d bytes\n", n * sizeof( unsigned short));
    return;
  }
  
  smin = 0;
  smax = n;

  mx = 0;
  mn = -1;

  mk = (double)(is->pbx - is->pax)/(double)n;
  bk = is->pax;

  ml = (double)(is->pby - is->pay)/(double)n;
  bl = is->pay;
  
  for( s=smin; s<smax; s++) {
    k = mk * s + bk;
    l = ml * s + bl;
    
    if( (int)(k+0.5) >= 0 && (int)(k+0.5) < is->inHeight && (int)(l+0.5) >=0 && (int)(l+0.5) < is->inWidth) {
      maxs[s] = nearestValue( is, buf, k, l);
    } else {
      maxs[s] = 0;
    }
    mx = (maxs[s] > mx) ? maxs[s] : mx;
    mn = (maxs[s] < mn) ? maxs[s] : mn;
  }
  
  fprintf( is->fout, "<data xMin=\"%d\" xMax=\"%d\" yMin=\"%d\" yMax=\"%d\">\n", smin, smax, mn, mx);
  for( s=smin; s<smax; s++) {
    fprintf( is->fout, "<point x=\"%d\" y=\"%d\"/>\n", s, maxs[s]);
  }
  fprintf( is->fout, "</data>\n");
}
