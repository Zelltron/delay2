#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <inttypes.h>

#include <frei0r.hpp>




#define QUEUEDEPTH 71 /* was 76 */
#define MODES 4

// freej compat facilitator
typedef struct {
  int16_t x; ///< x axis position coordinate
  int16_t y; ///< y axis position coordinate
  uint16_t w; ///< width of frame in pixels
  uint16_t h; ///< height of frame in pixels
  uint8_t bpp; ///< bits per pixel
  uint16_t pitch; ///< width of frame in bytes
  uint32_t size; ///< size of the whole frame in bytes
} ScreenGeometry;

class DelayGrab: public frei0r::filter {

public:
  DelayGrab(int wdt, int hgt);
  ~DelayGrab();

  virtual void update(double time,
                      uint32_t* out,
                      const uint32_t* in);


private:

  ScreenGeometry geo;

  void _init(int wdt, int hgt);

  void createDelaymap(int mode);
  void set_blocksize(int bs);
  int isqrt(unsigned int x);

  /* cheap & fast randomizer (by Fukuchi Kentarou) */
  uint32_t randval;
  uint32_t fastrand() { return (randval=randval*1103515245+12345); };
  void fastsrand(uint32_t seed) { randval = seed; };

  int x,y,i,xyoff,v;
  uint8_t *imagequeue,*curqueue;
  int curqueuenum;
  uint32_t *curdelaymap;
  uint8_t *curpos,*curimage;
  int curposnum;
  void *delaymap;

/* initialized from the init */
  int delaymapwidth;  /* width/blocksize */
  int delaymapheight; /* height/blocksize */
  int delaymapsize;   /* delaymapheight*delaymapwidth */

  int blocksize;
  int block_per_pitch;
  int block_per_bytespp;
  int block_per_res;

  int current_mode;

};

void DelayGrab::_init(int wdt, int hgt) {
  geo.w = wdt;
  geo.h = hgt;
  geo.bpp = 32;
  geo.size = geo.w*geo.h*(geo.bpp/8);
  geo.pitch = geo.w*(geo.bpp/8);
}




DelayGrab::DelayGrab(int wdt, int hgt) {

  delaymap = NULL;
  _init(wdt, hgt);

  imagequeue = (uint8_t *) malloc(QUEUEDEPTH*(geo.size));

  /* starting mode */
  current_mode = 3;
  /* starting blocksize */
  set_blocksize(2);

  curqueue=imagequeue;
  curqueuenum=0;

  fastsrand(::time(NULL));
}

DelayGrab::~DelayGrab() {
  if(delaymap) free(delaymap);
  free(imagequeue);
}



void DelayGrab::update(double time,
                       uint32_t* out,
                       const uint32_t* in) {

  /* Update queue pointer */
  if (curqueuenum==0) {
    curqueuenum=QUEUEDEPTH-1;
    curqueue = imagequeue;
    curqueue += (geo.size*(QUEUEDEPTH-1));
  } else {
    curqueuenum--;
    curqueue -= geo.size;
  }

   /* Copy image to queue */
  memcpy(curqueue,in,geo.size);

     /* Copy image blockwise to screenbuffer */
  curdelaymap= (uint32_t *)delaymap;
  for (y=0; y<delaymapheight; y++) {
    for (x=0; x<delaymapwidth; x++) {

      curposnum=((curqueuenum + (*curdelaymap)) % QUEUEDEPTH);

      xyoff= (x*block_per_bytespp) + (y*block_per_pitch);
      /* source */
      curpos= imagequeue;
      curpos += (geo.size*curposnum);
      curpos += xyoff;
      /* target */
      curimage = (uint8_t *)out;
      curimage += xyoff;
      /* copy block */
      for (i=0; i<blocksize; i++) {
	memcpy(curimage,curpos,block_per_res);
	curpos += geo.pitch;
	curimage += geo.pitch;
      }
      curdelaymap++;
    }
  }

}

// int kbd_input(char key) {
//   int res = 1;
//   switch(key) {
//   case 'w':
//     if(current_mode<MODES) createDelaymap(current_mode+1);
//     break;
//   case 'q':
//     if(current_mode>1) createDelaymap(current_mode-1);
//     break;
//   case 's':
//     set_blocksize(blocksize+1);
//     break;
//   case 'a':
//     if(blocksize>2) set_blocksize(blocksize-1);
//     break;
//   default:
//     res = 0;
//     break;
//   }
//   return res;
// }

void DelayGrab::createDelaymap(int mode) {
  double d;

  curdelaymap=(uint32_t *)delaymap;
  fastsrand(::time(NULL));

  for (y=delaymapheight; y>0; y--) {
    for (x=delaymapwidth; x>0; x--) {
      switch (mode) {
      case 1:
	/* Random delay with square distribution */
	d = (double)fastrand()/(double)RAND_MAX;
	*curdelaymap = (int)(d*d*16.0);
	break;
      case 2:
	/* Vertical stripes of increasing delay outward from center */
	if (x<(delaymapwidth/2)) {
	  v=(delaymapwidth/2)-x;
	} else if (x>(delaymapwidth/2)) {
	  v=x-(delaymapwidth/2);
	} else {
	  v=0;
	}
	*curdelaymap=v/2;
	break;
      case 3:
	/* Horizontal stripes of increasing delay outward from center */
	if(y<(delaymapheight/2)) {
	  v = (delaymapheight/2)-y;
	} else if(y>(delaymapheight/2)) {
	  v = y-(delaymapheight/2);
	} else {
	  v=0;
	}
	*curdelaymap=v/2;
	break;
      case 4:
	/* Rings of increasing delay outward from center */
	v = (int)isqrt((unsigned int)((x-(delaymapwidth/2))*
				      (x-(delaymapwidth/2))+
				      (y-(delaymapheight/2))*
				      (y-(delaymapheight/2))));
	*curdelaymap=v/2;
	break;
      } // switch
      /* Clip values */
      if ((int)(*curdelaymap)<0) {
	*curdelaymap=0;
      } else if (*curdelaymap>(QUEUEDEPTH-1)) {
	*curdelaymap=(QUEUEDEPTH-1);
      }
      curdelaymap++;
    }
  }
  current_mode = mode;
}

void DelayGrab::set_blocksize(int bs) {

  blocksize = bs;
  block_per_pitch = blocksize*(geo.pitch);
  block_per_bytespp = blocksize*(geo.bpp>>3);
  block_per_res = blocksize<<(geo.bpp>>4);

  delaymapwidth = (geo.w)/blocksize;
  delaymapheight = (geo.h)/blocksize;
  delaymapsize = delaymapheight*delaymapwidth;

  if(delaymap) { free(delaymap); delaymap = NULL; }
  delaymap = malloc(delaymapsize*4);

  createDelaymap(current_mode);
}

/* i learned this on books // by jaromil */
int DelayGrab::isqrt(unsigned int x) {
  unsigned int m, y, b; m = 0x40000000;
  y = 0; while(m != 0) { b = y | m; y = y>>1;
  if(x>=b) { x=x-b; y=y|m; }
  m=m>>2; } return y;
}



frei0r::construct<DelayGrab> plugin("Delaygrab",
				  "delayed frame blitting mapped on a time bitmap",
				  "Bill Spinhover, Andreas Schiffler, Jaromil, Ryan Zell",
				  3,1);
