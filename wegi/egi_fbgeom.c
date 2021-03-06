/*-----------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Referring to: http://blog.chinaunix.net/uid-22666248-id-285417.html
本文的copyright归yuweixian4230@163.com 所有，使用GPL发布，可以自由拷贝，转载。
但转载请保持文档的完整性，注明原作者及原链接，严禁用于任何商业用途。
作者：yuweixian4230@163.com
博客：yuweixian4230.blog.chinaunix.net

Note:
1. Not thread safe.

TODO:

Modified and appended by Midas-Zhou
-----------------------------------------------------------------------------*/
#include "egi_fbgeom.h"
#include "egi.h"
#include "egi_debug.h"
#include "egi_math.h"
#include <unistd.h>
#include <string.h> /*memset*/
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <math.h>
#include <stdlib.h>

//#ifndef _TYPE_FBDEV_
//#define _TYPE_FBDEV_
#if 0
    typedef struct fbdev{
        int fdfd; //open "dev/fb0"
        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;
        long int screensize;
        char *map_fb;
    }FBDEV;
#endif 
//#endif


/* global variale, Frame buffer device */
FBDEV   gv_fb_dev __attribute__(( visibility ("hidden") )) ;
EGI_BOX gv_fb_box;

/* default color set */
static uint16_t fb_color=(30<<11)|(10<<5)|10;  //r(5)g(6)b(5)

/*-------------------------------------
Return:
	0	OK
	<0	Fails
---------------------------------------*/
int init_dev(FBDEV *dev)
{
        FBDEV *fr_dev=dev;

        fr_dev->fdfd=open(EGI_FBDEV_NAME,O_RDWR);
	if(fr_dev<0) {
	  printf("Open /dev/fb0: %s\n",strerror(errno));
	  return -1;
	}

        printf("Framebuffer device was opended successfully.\n");
        ioctl(fr_dev->fdfd,FBIOGET_FSCREENINFO,&(fr_dev->finfo));
        ioctl(fr_dev->fdfd,FBIOGET_VSCREENINFO,&(fr_dev->vinfo));
        fr_dev->screensize=fr_dev->vinfo.xres*fr_dev->vinfo.yres*fr_dev->vinfo.bits_per_pixel/8;

	/* mmap FB */
        fr_dev->map_fb=(unsigned char *)mmap(NULL,fr_dev->screensize,PROT_READ|PROT_WRITE,MAP_SHARED,
											fr_dev->fdfd,0);
	if(fr_dev->map_fb==MAP_FAILED) {
		printf("Fail to mmap FB!\n");
		close(fr_dev->fdfd);
		return -2;
	}

	/* init fb_filo */
	fr_dev->filo_on=0;
	fr_dev->fb_filo=egi_malloc_filo(1<<13, sizeof(FBPIX), FILO_AUTO_DOUBLE);//|FILO_AUTO_HALVE
	if(fr_dev->fb_filo==NULL) {
		printf("Fail to malloc FB FILO!\n");
		munmap(dev->map_fb,dev->screensize);
		close(fr_dev->fdfd);
		return -3;
	}

	/* assign fb box */
	gv_fb_box.startxy.x=0;
	gv_fb_box.startxy.y=0;
	gv_fb_box.endxy.x=fr_dev->vinfo.xres-1;
	gv_fb_box.endxy.y=fr_dev->vinfo.yres-1;

//      printf("init_dev successfully. fr_dev->map_fb=%p\n",fr_dev->map_fb);
	printf(" \n------- FB Parameters -------\n");
	printf(" bits_per_pixel: %d bits \n",fr_dev->vinfo.bits_per_pixel);
	printf(" line_length: %d bytes\n",fr_dev->finfo.line_length);
	printf(" xres: %d pixels, yres: %d pixels \n", fr_dev->vinfo.xres, fr_dev->vinfo.yres);
	printf(" xoffset: %d,  yoffset: %d \n", fr_dev->vinfo.xoffset, fr_dev->vinfo.yoffset);
	printf(" screensize: %ld bytes\n", fr_dev->screensize);
	printf(" ----------------------------\n\n");

	return 0;
}


/*-------------------------
Release FB and free map
--------------------------*/
void release_dev(FBDEV *dev)
{
	if(!dev || !dev->map_fb)
		return;

	egi_free_filo(dev->fb_filo);
	munmap(dev->map_fb,dev->screensize);
	close(dev->fdfd);
}


/*-------------------------------------------------------------
Put fb->filo_on to 1, as turn on FB FILO.

Note:
1. To activate FB FILO, depends also on FB_writing handle codes.

Midas Zhou
--------------------------------------------------------------*/
inline void fb_filo_on(FBDEV *dev)
{
	if(!dev || !dev->fb_filo)
		return;

	dev->filo_on=1;
}

inline void fb_filo_off(FBDEV *dev)
{
	if(!dev || !dev->fb_filo)
		return;

	dev->filo_on=0;
}



/*----------------------------------------------
Pop out all FBPIXs in the fb filo
Midas Zhou
----------------------------------------------*/
void fb_filo_flush(FBDEV *dev)
{
	FBPIX fpix;

	if(!dev || !dev->fb_filo)
		return;

	while( egi_filo_pop(dev->fb_filo, &fpix)==0 )
	{
		/* write back to FB */
		//printf("EGI FILO pop out: pos=%ld, color=%d\n",fpix.position,fpix.color);
		*((uint16_t *)(dev->map_fb+fpix.position)) = fpix.color;
	}
}


/*  set color for every dot */
inline void fbset_color(uint16_t color)
{
	fb_color=color;
}


/*------------------------------------
 check if (px,py) in box(x1,y1,x2,y2)
 return true or false

 Midas Zhou
-------------------------------------*/
bool point_inbox(int px,int py, int x1, int y1,int x2, int y2)
{
       int xl,xh,yl,yh;

	if(x1>=x2){
		xh=x1;xl=x2;
	}
	else {
		xl=x1;xh=x2;
	}

	if(y1>=y2){
		yh=y1;yl=y2;
	}
	else {
		yh=y2;yl=y1;
	}

	if( (px>=xl && px<=xh) && (py>=yl && py<=yh))
		return true;
	else
		return false;
}

/*---------------------------------------------------------------
Check whether the box is totally WITHIN the container.
If there is an overlap of any sides, it is deemed as NOT within!
Midas
----------------------------------------------------------------*/
bool  box_inbox(EGI_BOX* inbox, EGI_BOX* container)
{
	int xcu,xcd; /*u-max, d-min*/
	int xiu,xid;
	int ycu,ycd;
	int yiu,yid;


	/* 1. get Max. and Min X coord of the container */
     	if( container->startxy.x > container->endxy.x ) {
		xcu=container->startxy.x;
		xcd=container->endxy.x;
	}
	else {
		xcu=container->endxy.x;
		xcd=container->startxy.x;
	}

	/* 2. get Max. and Min X coord of the inbox */
     	if( inbox->startxy.x > inbox->endxy.x ) {
		xiu=inbox->startxy.x;
		xid=inbox->endxy.x;
	}
	else {
		xiu=inbox->endxy.x;
		xid=inbox->startxy.x;
	}

	/* 3. compare X coord */
	if( !(xcd<xid) || !(xcu>xiu) )
		return false;

	/* 4. get Max. and Min Y coord of the container */
     	if( container->startxy.y > container->endxy.y ) {
		ycu=container->startxy.y;
		ycd=container->endxy.y;
	}
	else {
		ycu=container->endxy.y;
		ycd=container->startxy.y;
	}
	/* 5. get Max. and Min Y coord of the inbox */
     	if( inbox->startxy.y > inbox->endxy.y ) {
		yiu=inbox->startxy.y;
		yid=inbox->endxy.y;
	}
	else {
		yiu=inbox->endxy.y;
		yid=inbox->startxy.y;
	}
	/* 6. compare Y coord */
	if( !(ycd<yid) || !(ycu>yiu) )
		return false;


	return true;
}


/*--------------------------------------------------------------------
Check whether the box is totally out of the container.
If there is an overlap of any sides, it is deemed as NOT totally out!
Midas
--------------------------------------------------------------------*/
bool  box_outbox(EGI_BOX* inbox, EGI_BOX* container)
{
	int xcu,xcd; /*u-max, d-min*/
	int xiu,xid;
	int ycu,ycd;
	int yiu,yid;


	/* 1. get Max. and Min X coord of the container */
     	if( container->startxy.x > container->endxy.x ) {
		xcu=container->startxy.x;
		xcd=container->endxy.x;
	}
	else {
		xcu=container->endxy.x;
		xcd=container->startxy.x;
	}

	/* 2. get Max. and Min X coord of the inbox */
     	if( inbox->startxy.x > inbox->endxy.x ) {
		xiu=inbox->startxy.x;
		xid=inbox->endxy.x;
	}
	else {
		xiu=inbox->endxy.x;
		xid=inbox->startxy.x;
	}

	/* 3. compare X coord */
	if( !( xcd>xiu || xcu<xid ) )
		return false;


	/* 4. get Max. and Min Y coord of the container */
     	if( container->startxy.y > container->endxy.y ) {
		ycu=container->startxy.y;
		ycd=container->endxy.y;
	}
	else {
		ycu=container->endxy.y;
		ycd=container->startxy.y;
	}
	/* 5. get Max. and Min Y coord of the inbox */
     	if( inbox->startxy.y > inbox->endxy.y ) {
		yiu=inbox->startxy.y;
		yid=inbox->endxy.y;
	}
	else {
		yiu=inbox->endxy.y;
		yid=inbox->startxy.y;
	}
	/* 6. compare Y coord */
	if( !( ycd>yiu || ycu<yid ) )
		return false;


	return true;
}




/*---------------------------------------------
    clear screen with given color
      BUG:
	!!!!call egi_colorxxxx_randmon() error
 Midas
---------------------------------------------*/
void clear_screen(FBDEV *dev, uint16_t color)
{
	int i,j;
	FBDEV *fr_dev=dev;
	int xres=dev->vinfo.xres;
	int yres=dev->vinfo.yres;
	int bytes_per_pixel=fr_dev->vinfo.bits_per_pixel/8;
	long int location=0;

	for(location=0; location < (fr_dev->screensize/bytes_per_pixel); location++)
	        *((uint16_t*)(fr_dev->map_fb+location*bytes_per_pixel))=color;

/*   ---------------following is more accurate!?!?  ///
	for(i=0;i<yres;i++)
	{
		for(j=0;j<xres;j++)
		{
		        location=(j+fr_dev->vinfo.xoffset)*(fr_dev->vinfo.bits_per_pixel/8)+
                	     (i+fr_dev->vinfo.yoffset)*fr_dev->finfo.line_length;
       			 *((unsigned short int *)(fr_dev->map_fb+location))=fb_color;
		}
	}
*/
}

/*-----------------------------------------------------
    Return:
	0	OK
	-1	get out of FB mem.(ifndef FB_DOTOUT_ROLLBACK)
--------------------------------------------------------*/
int draw_dot(FBDEV *dev,int x,int y) //(x.y) 是坐标
{
        FBDEV *fr_dev=dev;
	int fx=x;
	int fy=y;
        long int location=0;
	int xres=fr_dev->vinfo.xres;
	int yres=fr_dev->vinfo.yres;
	FBPIX fpix;

#ifdef FB_DOTOUT_ROLLBACK
	/* map to LCD(X,Y) */
	if(fx>xres-1)
		fx=fx%xres;
	else if(fx<0)
	{
		fx=xres-(-fx)%xres;
		fx=fx%xres; /* here fx=1-240 */
	}
	if( fy > yres-1) {
		fy=fy%yres;
	}
	else if(fy<0) {
		fy=yres-(-fy)%yres;
		fy=fy%yres; /* here fy=1-320 */
	}


#else /* NO ROLLBACK */

	/* ignore out_ranged points */
	if( fx>(xres-1) || fx<0) {
		return -1;
	}
	if( fy>(yres-1) || fy<0 ) {
		return -1;
	}
#endif

        location=(fx+fr_dev->vinfo.xoffset)*(fr_dev->vinfo.bits_per_pixel/8)+
                     (fy+fr_dev->vinfo.yoffset)*fr_dev->finfo.line_length;

	/* push to FB FILO */
	if(fr_dev->filo_on)
        {
                fpix.position=location; /* pixel to bytes, !!! FAINT !!! */
                fpix.color=*(uint16_t *)(fr_dev->map_fb+location);
                egi_filo_push(fr_dev->fb_filo, &fpix);
        }

	/* NOT necessary ???  check if no space left for a 16bit_pixel in FB mem */
	if( location<0 || location > (fr_dev->screensize-sizeof(uint16_t)) )
	{
		printf("WARNING: point location out of fb mem.!\n");
		return -1;
	}

        *((unsigned short int *)(fr_dev->map_fb+location))=fb_color;

	return 0;
}


#if 0   /*------ OBSOLETE, seems the same effect as above !! --------*/
    void draw_dot(FBDEV *dev,int x,int y) //(x.y) 是坐标
    {
        FBDEV *fr_dev=dev;
        int *xx=&x;
        int *yy=&y;
        long int location=0;

        location=(*xx+fr_dev->vinfo.xoffset)*(fr_dev->vinfo.bits_per_pixel/8)+
                     (*yy+fr_dev->vinfo.yoffset)*fr_dev->finfo.line_length;

        *((unsigned short int *)(fr_dev->map_fb+location))=fb_color;
    }
#endif

#if 0 ///////////// OBSELETE ///////////////////
/*---------------------------------------------------
	Draw a simple line
---------------------------------------------------*/
void draw_line(FBDEV *dev,int x1,int y1,int x2,int y2)
{
        FBDEV *fr_dev=dev;
        int *xx1=&x1;
        int *yy1=&y1;
        int *xx2=&x2;
        int *yy2=&y2;

        int i=0;
        int j=0;
	int k;
        int tekxx=*xx2-*xx1;
        int tekyy=*yy2-*yy1;

        if(*xx2>*xx1)
        {
            for(i=*xx1;i<=*xx2;i++)
            {
                j=(i-*xx1)*tekyy/tekxx+*yy1; /* Y */
//		for(k=*yy1
                draw_dot(fr_dev,i,j);

            }
        }
	else if(*xx2 == *xx1)
	{
	   if(*yy2>=*yy1)
	   {
		for(i=*yy1;i<=*yy2;i++)
		   draw_dot(fr_dev,*xx1,i);
	    }
	    else //yy2<yy1
	   {
		for(i=*yy2;i<=*yy1;i++)
			draw_dot(fr_dev,*xx1,i);
	   }
	}
        else
        {
            for(i=*xx2;i<=*xx1;i++)
            {
                j=(i-*xx2)*tekyy/tekxx+*yy2;
                draw_dot(fr_dev,i,j);
            }
        }
}
#endif //////////////// END /////////////////////


/*---------------------------------------------------
	Draw a simple line
---------------------------------------------------*/
void draw_line(FBDEV *dev,int x1,int y1,int x2,int y2)
{
        int i=0;
        int j=0;
	int k;
        int tekxx=x2-x1;
        int tekyy=y2-y1;
	int tmp;

        if(x2>x1) {
	    tmp=y1;
            for(i=x1;i<=x2;i++) {
                j=(i-x1)*tekyy/tekxx+y1;
		if(y2>=y1) {
			for(k=tmp;k<=j;k++)		/* fill uncontinous points */
		                draw_dot(dev,i,k);
		}
		else { /* y2<y1 */
			for(k=tmp;k>=j;k--)
				draw_dot(dev,i,k);
		}
		tmp=j;
            }
        }
	else if(x2 == x1) {
	   if(y2>=y1) {
		for(i=y1;i<=y2;i++)
		   draw_dot(dev,x1,i);
	    }
	    else {
		for(i=y2;i<=y1;i++)
			draw_dot(dev,x1,i);
	   }
	}
        else /* x1>x2 */
        {
	    tmp=y2;
            for(i=x2;i<=x1;i++) {
                j=(i-x2)*tekyy/tekxx+y2;
		if(y1>=y2) {
			for(k=tmp;k<=j;k++)		/* fill uncontinous points */
		        	draw_dot(dev,i,k);
		}
		else {  /* y2>y1 */
			for(k=tmp;k>=j;k--)		/* fill uncontinous points */
		        	draw_dot(dev,i,k);
		}
		tmp=j;
            }
        }
}



/*--------------------------------------------------------------------
Draw A Line with width, draw no circle at two points
x1,x1: starting point
x2,y2: ending point
w: width of the line ( W=2*N+1 )

NOTE: if you input w=0, it's same as w=1.

Midas Zhou
----------------------------------------------------------------------*/
void draw_wline_nc(FBDEV *dev,int x1,int y1,int x2,int y2, unsigned int w)
{
	int i;
	int xr1,yr1,xr2,yr2;

	/* half width, also as circle rad */
	int r=w>>1; /* so w=0 and w=1 is the same */

	/* x,y, difference */
	int ydif=y2-y1;
	int xdif=x2-x1;

        int32_t fp16_len = mat_fp16_sqrtu32(ydif*ydif+xdif*xdif);

   if(fp16_len !=0 )
   {
	/* draw multiple lines  */
	for(i=0;i<=r;i++)
	{
		/* draw UP_HALF multiple lines  */
		xr1=x1-(i*ydif<<16)/fp16_len;
		yr1=y1+(i*xdif<<16)/fp16_len;
		xr2=x2-(i*ydif<<16)/fp16_len;
		yr2=y2+(i*xdif<<16)/fp16_len;

		draw_line(dev,xr1,yr1,xr2,yr2);

		/* draw LOW_HALF multiple lines  */
		xr1=x1+(i*ydif<<16)/fp16_len;
		yr1=y1-(i*xdif<<16)/fp16_len;
		xr2=x2+(i*ydif<<16)/fp16_len;
		yr2=y2-(i*xdif<<16)/fp16_len;

		draw_line(dev,xr1,yr1,xr2,yr2);
	}
   } /* end of len !=0, if len=0, the two points are the same position */

}



/*--------------------------------------------------------------------
Draw A Line with width, with circle at two points.
x1,x1: starting point
x2,y2: ending point
w: width of the line ( W=2*N+1 )

NOTE: if you input w=0, it's same as w=1.

Midas Zhou
----------------------------------------------------------------------*/
void draw_wline(FBDEV *dev,int x1,int y1,int x2,int y2, unsigned int w)
{
	/* half width, also as circle rad */
	int r=w>>1; /* so w=0 and w=1 is the same */

	int i;
	int xr1,yr1,xr2,yr2;

	/* x,y, difference */
	int ydif=y2-y1;
	int xdif=x2-x1;

        int32_t fp16_len = mat_fp16_sqrtu32(ydif*ydif+xdif*xdif);


   if(fp16_len !=0 )
   {
	/* draw multiple lines  */
	for(i=0;i<=r;i++)
	{
		/* draw UP_HALF multiple lines  */
		xr1=x1-(i*ydif<<16)/fp16_len;
		yr1=y1+(i*xdif<<16)/fp16_len;
		xr2=x2-(i*ydif<<16)/fp16_len;
		yr2=y2+(i*xdif<<16)/fp16_len;

		draw_line(dev,xr1,yr1,xr2,yr2);

		/* draw LOW_HALF multiple lines  */
		xr1=x1+(i*ydif<<16)/fp16_len;
		yr1=y1-(i*xdif<<16)/fp16_len;
		xr2=x2+(i*ydif<<16)/fp16_len;
		yr2=y2-(i*xdif<<16)/fp16_len;

		draw_line(dev,xr1,yr1,xr2,yr2);
	}
   } /* end of len !=0, if len=0, the two points are the same position */


	/* draw start/end circles */
	draw_filled_circle(dev, x1, y1, r);
	draw_filled_circle(dev, x2, y2, r);
}



/*---------------------------------------------------------------------
Draw A Poly Line, with circle at each point.

points:	     input points for the polyline
num:	     total number of input points.
w: width of the line ( W=2*N+1 )

Midas Zhou
---------------------------------------------------------------------*/
void draw_pline(FBDEV *dev, EGI_POINT *points, int pnum, unsigned int w)
{
	int i=0;

	/* check input data */
	if( points==NULL || pnum<=0 ) {
		printf("%s: Input params error.\n", __func__);
		return ;
	}

	for(i=0; i<pnum-1; i++) {
		draw_wline(dev,points[i].x,points[i].y,points[i+1].x,points[i+1].y,w);
	}

}


/*---------------------------------------------------------------------
Draw A Poly Line, with no circle at each point.

points:	     input points for the polyline
num:	     total number of input points.
w: width of the line ( W=2*N+1 )

Midas Zhou
---------------------------------------------------------------------*/
void draw_pline_nc(FBDEV *dev, EGI_POINT *points, int pnum, unsigned int w)
{
	int i=0;

	/* check input data */
	if( points==NULL || pnum<=0 ) {
		printf("%s: Input params error.\n", __func__);
		return ;
	}

	for(i=0; i<pnum-1; i++) {
		draw_wline_nc(dev,points[i].x,points[i].y,points[i+1].x,points[i+1].y,w);
	}

}



/*------------------------------------------------
	           draw an oval
-------------------------------------------------*/
void draw_oval(FBDEV *dev,int x,int y) //(x.y) 是坐标
{
        FBDEV *fr_dev=dev;
        int *xx=&x;
        int *yy=&y;

	draw_line(fr_dev,*xx,*yy-3,*xx,*yy+3);
	draw_line(fr_dev,*xx-1,*yy-2,*xx-1,*yy+2);
	draw_line(fr_dev,*xx-2,*yy-1,*xx-2,*yy+1);
	draw_line(fr_dev,*xx+1,*yy-2,*xx+1,*yy+2);
	draw_line(fr_dev,*xx+2,*yy-2,*xx+2,*yy+2);
}


/*--------------------------------------------------
Draw an rectangle
Midas Zhou
--------------------------------------------------*/
void draw_rect(FBDEV *dev,int x1,int y1,int x2,int y2)
{
//        FBDEV *fr_dev=dev;

	draw_line(dev,x1,y1,x1,y2);
	draw_line(dev,x1,y2,x2,y2);
	draw_line(dev,x2,y2,x2,y1);
	draw_line(dev,x2,y1,x1,y1);
}

/*--------------------------------------------------
Draw an rectangle with pline
x1,y1,x2,y2:	two points define a rect.
w:		with of ploy line.

Midas Zhou
--------------------------------------------------*/
void draw_wrect(FBDEV *dev,int x1,int y1,int x2,int y2, int w)
{
	/* sort min and max x,y */
	int xl=(x1<x2?x1:x2);
	int xr=(x1>x2?x1:x2);
	int yu=(y1<y2?y1:y2);
	int yd=(y1>y2?y1:y2);

	draw_wline_nc(dev,xl-w/2,yu,xr+w/2,yu,w);
	draw_wline_nc(dev,xl-w/2,yd,xr+w/2,yd,w);
	draw_wline_nc(dev,xl,yu-w/2,xl,yd+w/2,w);
	draw_wline_nc(dev,xr,yu-w/2,xr,yd+w/2,w);

}


/*-------------------------------------------------------------
    Draw a filled rectangle defined by two end points of its
    diagonal line. Both points are also part of the rectangle.

    Return:
		0	OK
		//ignore -1	point out of FB mem
    Midas
------------------------------------------------------------*/
int draw_filled_rect(FBDEV *dev,int x1,int y1,int x2,int y2)
{
	int xr,xl,yu,yd;
	int i,j;

        /* sort point coordinates */
        if(x1>x2) {
                xr=x1;
		xl=x2;
        }
        else {
                xr=x2;
		xl=x1;
	}

        if(y1>y2) {
                yu=y1;
		yd=y2;
        }
        else {
                yu=y2;
		yd=y1;
	}

	for(i=yd;i<=yu;i++)
	{
		for(j=xl;j<=xr;j++)
		{
                	draw_dot(dev,j,i); /* ignore range check */
		}
	}

	return 0;
}

/*--------------------------------------------------------------------------
Same as draw_filled_rect(), but with color.
Midas Zhou
---------------------------------------------------------------------------*/
int draw_filled_rect2(FBDEV *dev, uint16_t color, int x1,int y1,int x2,int y2)
{
	int xr,xl,yu,yd;
	int i,j;

        /* sort point coordinates */
        if(x1>x2) {
                xr=x1;
		xl=x2;
        }
        else {
                xr=x2;
		xl=x1;
	}

        if(y1>y2) {
                yu=y1;
		yd=y2;
        }
        else {
                yu=y2;
		yd=y1;
	}

	for(i=yd;i<=yu;i++)
	{
		for(j=xl;j<=xr;j++)
		{
			fb_color=color;
                	draw_dot(dev,j,i); /* ignore range check */
		}
	}

	return 0;
}



/*----------------------------------------------
draw a circle,
	(x,y)	circle center
	r	radius
Midas Zhou
-----------------------------------------------*/
void draw_circle(FBDEV *dev, int x, int y, int r)
{
	int i;
	int s;

	for(i=0;i<r;i++)
	{
//		s=sqrt(r*r*1.0-i*i*1.0);
		s=round(sqrt(r*r*1.0-i*i*1.0));
//		if(i==0)s-=1; /* erase four tips */
		draw_dot(dev,x-s,y+i);
		draw_dot(dev,x+s,y+i);
		draw_dot(dev,x-s,y-i);
		draw_dot(dev,x+s,y-i);
		/* flip X-Y */
		draw_dot(dev,x-i,y+s);
		draw_dot(dev,x+i,y+s);
		draw_dot(dev,x-i,y-s);
		draw_dot(dev,x+i,y-s);
	}
}


/*-----------------------------------------------------------------
draw a circle formed by poly lines.
	(x,y)	circle center
	r	radius
	w	pline width

Note:	1. The final circle looks ugly if 'r' or 'w' is small.!!!!
	2. np=30, for rmax=20 with end circle
	   np=90, for rmax=70 with end circle

Midas Zhou
------------------------------------------------------------------*/
void draw_pcircle(FBDEV *dev, int x0, int y0, int r, unsigned int w)
{
	int i;
	int np=90; /* number of segments in a 1/4 circle */
	double dx[90+1],dy[90+1];
	EGI_POINT points[90+1]; /* points on 1/4 circle */

	for(i=0; i<np+1; i++) {
		dx[i]=1.0*r*cos(90.0/np*i/180.0*MATH_PI); //cos(1.0*np*i*MATH_PI/2.0);
		dy[i]=1.0*r*sin(90.0/np*i/180.0*MATH_PI);  // sin(1.0*np*i*MATH_PI/2.0);
	}

	/* draw 4th quadrant */
	for(i=0; i<np+1; i++) {
		points[i].x=x0+dx[i];
		points[i].y=y0+dy[i];
	}
	points[0].x -=1; /* erase tip */
	points[np].y -=1;
	draw_pline(dev,points,np+1,w);

	/* draw 3rd quadrant */
	for(i=0; i<np+1; i++) {
		points[i].x = (x0<<1) - points[i].x;
	}
	draw_pline(dev,points,np+1,w);

	/* draw 2nd quadrant */
	for(i=0; i<np+1; i++) {
		points[i].y = (y0<<1) - points[i].y;
	}
	draw_pline(dev,points,np+1,w);

	/* draw 1st quadrant */
	for(i=0; i<np+1; i++) {
		points[i].x = (x0<<1) - points[i].x;
	}
	draw_pline(dev,points,np+1,w);

}


/*-----------------------------------------------------------------
draw a a filled triangle.
points: 3 points

Midas Zhou
------------------------------------------------------------------*/
void draw_filled_triangle(FBDEV *dev, EGI_POINT *points)
{
	if(points==NULL)
		return;

	int i;
	int nl=0,nr=0; /* left and right point index */
	int nm; /* mid point index */

	float klr,klm,kmr;
	int yu,yd,ymu;

	for(i=1;i<3;i++) {
		if(points[i].x < points[nl].x) nl=i;
		if(points[i].x > points[nr].x) nr=i;
	}

	/* three points are collinear */
	if(nl==nr) {
		draw_pline_nc(dev, points, 3, 1);
		return;
	}

	/* get x_mid point index */
	nm=3-nl-nr;

	//printf("points[nl=%d] x=%d, y=%d \n", nl,points[nl].x,points[nl].y);
	//printf("points[nm=%d] x=%d, y=%d \n", nm,points[nm].x,points[nm].y);
	//printf("points[nr=%d] x=%d, y=%d \n", nr,points[nr].x,points[nr].y);

	if(points[nr].x != points[nl].x) {
		klr=1.0*(points[nr].y-points[nl].y)/(points[nr].x-points[nl].x);
	}
	else
		klr=1000000.0; /* a big value */

	if(points[nm].x != points[nl].x) {
		klm=1.0*(points[nm].y-points[nl].y)/(points[nm].x-points[nl].x);
	}
	else
		klm=1000000.0;

	if(points[nr].x != points[nm].x) {
		kmr=1.0*(points[nr].y-points[nm].y)/(points[nr].x-points[nm].x);
	}
	else
		kmr=1000000.0;

	//printf("klr=%f, klm=%f, kmr=%f \n",klr,klm,kmr);

	/* draw lines for two tri */
	for( i=0; i<points[nm].x-points[nl].x+1; i++)
	{
		yu=points[nl].y+klr*i;
		yd=points[nl].y+klm*i;
		//printf("part1: x=%d	yu=%d	yd=%d \n", points[nl].x+i, yu, yd);
		draw_line(dev, points[nl].x+i, yu, points[nl].x+i, yd);
	}
	ymu=yu;
	for( i=0; i<points[nr].x-points[nm].x+1; i++)
	{
		yu=ymu+klr*i;
		yd=points[nm].y+kmr*i;
		//printf("part2: x=%d	yu=%d	yd=%d \n", points[nm].x+i, yu, yd);
		draw_line(dev, points[nm].x+i, yu, points[nm].x+i, yd);
	}

}



/*-----------------------------------------------------------------------
draw a filled annulus.
	(x0,y0)	circle center
	r	radius in mid of w.
	w	width of annulus.

Note:
	1. When w=1, the result annulus is a circle with some
	   unconnected dots.!!!
--------------------------------------------------------------------------*/
void draw_filled_annulus(FBDEV *dev, int x0, int y0, int r, unsigned int w)
{
	int i,j,k;
	int m,n;
	int ro=r+(w>>1); /* outer radium */
	int ri=r-(w>>1); /* inner radium */

        for(j=0; j<ro; j++) /* j<=ro,  j=ro erased here!!!  */
        {
		/* distance from Xcenter to the point on outer circle */
	        m=round(sqrt( ro*ro-j*j ));
		//m=mat_fp16_sqrtu32(ro*ro-j*j)>>16; /* Seems no effect */
		m-=1; /* diameter is odd */

		/* distance from Xcenter to the point on inner circle */
                if(j<ri) {
                        k=round(sqrt( ri*ri-j*j));
			//k=mat_fp16_sqrtu32(ri*ri-j*j)>>16;
			k-=1; /* diameter is odd */
		}
                else
                        k=0;

		/*erase tips*/
//		if(j==0) {
//			m-=1;
//			k-=1;
//		}

		/* draw 4th quadrant */
		draw_line(dev,x0+k,y0+j,x0+m,y0+j);
		/* draw 3rd quadrant */
		draw_line(dev,x0-k,y0+j,x0-m,y0+j);
		/* draw 2nd quadrant */
		draw_line(dev,x0-k,y0-j,x0-m,y0-j);
		/* draw 1st quadrant */
		draw_line(dev,x0+k,y0-j,x0+m,y0-j);
	}
}


/*------------------------------------------------
  draw a filled circle
  Midas Zhou
-------------------------------------------------*/
void draw_filled_circle(FBDEV *dev, int x, int y, int r)
{
	int i;
	int s;

	for(i=0;i<r;i++)
	{
		s=round(sqrt(r*r-i*i));
		s-=1; /* diameter is always odd */

		//if(i==0)s-=1; /* erase four tips */
		draw_line(dev,x-s,y+i,x+s,y+i);
		draw_line(dev,x-s,y-i,x+s,y-i);
	}

}



/*----------------------------------------------------------------------------
   copy a block of FB memory  to buffer
   x1,y1,x2,y2:	  LCD area corresponding to FB mem. block
   buf:		  data dest.

   Return
		2	area out of FB mem
		1	partial area out of FB mem boundary
		0	OK
		-1	fails
   Midas Zhou
----------------------------------------------------------------------------*/
   int fb_cpyto_buf(FBDEV *fb_dev, int x1, int y1, int x2, int y2, uint16_t *buf)
   {
	int i,j;
	int xl,xr; /* left right */
	int yu,yd; /* up down */
	int ret=0;
	long int location=0;
	int xres=fb_dev->vinfo.xres;
	int yres=fb_dev->vinfo.yres;
	int tmpx,tmpy;

	/* check buf */
	if(buf==NULL)
	{
		printf("fb_cpyto_buf(): buf is NULL!\n");
		return -1;
	}

	/* sort point coordinates */
	if(x1>x2){
		xr=x1;
		xl=x2;
	}
	else{
		xr=x2;
		xl=x1;
	}
	if(y1>y2){
		yu=y1;
		yd=y2;
	}
	else{
		yu=y2;
		yd=y1;
	}



#ifdef FB_DOTOUT_ROLLBACK /* -------------   ROLLBACK  ------------------*/

	/* ---------  copy mem --------- */
	for(i=yd;i<=yu;i++)
	{
        	for(j=xl;j<=xr;j++)
		{
			/* map i,j to LCD(Y,X) */
			if(i<0) /* map Y */
			{
				tmpy=yres-(-i)%yres; /* here tmpy=1-320 */
				tmpy=tmpy%yres;
			}
			else if(i > yres-1)
				tmpy=i%yres;
			else
				tmpy=i;

			if(j<0) /* map X */
			{
				tmpx=xres-(-j)%xres; /* here tmpx=1-240 */ 
				tmpx=tmpx%xres;
			}
			else if(j > xres-1)
				tmpx=j%xres;
			else
				tmpx=j;

			location=(tmpx+fb_dev->vinfo.xoffset)*(fb_dev->vinfo.bits_per_pixel/8)+
        	        	     (tmpy+fb_dev->vinfo.yoffset)*fb_dev->finfo.line_length;

			/* copy to buf */
        		*buf = *((uint16_t *)(fb_dev->map_fb+location));
			 buf++;
		}
	}

#else  /* -----------------  NO ROLLBACK  ------------------------*/

	/* normalize x,y */
	if(yd<0)yd=0;
	if(yu>yres-1)yu=yres-1;

	if(xl<0)xl=0;
	if(xr>xres-1)xr=xres-1;

	/* ---------  copy mem --------- */
	for(i=yd;i<=yu;i++)
	{
        	for(j=xl;j<=xr;j++)
		{
#if 0 /* This shall never happen now! */
			if( i<0 || j<0 || i>yres-1 || j>xres-1 )
			{
				EGI_PDEBUG(DBG_FBGEOM,"WARNING: fb_cpyfrom_buf(): coordinates out of range!\n");
				ret=1;
			}
#endif
			/* map i,j to LCD(Y,X) */
			if(i>yres-1) /* map Y */
				tmpy=yres-1;
			else if(i<0)
				tmpy=0;
			else
				tmpy=i;

			if(j>xres-1) /* map X */
				tmpx=xres-1;
			else if(j<0)
				tmpx=0;
			else
				tmpx=j;

			location=(tmpx+fb_dev->vinfo.xoffset)*(fb_dev->vinfo.bits_per_pixel/8)+
        	        	     (tmpy+fb_dev->vinfo.yoffset)*fb_dev->finfo.line_length;

			/* copy to buf */
        		*buf = *((uint16_t *)(fb_dev->map_fb+location));
			 buf++;
		}
	}

#endif

	return ret;
   }



 /*----------------------------------------------------------------------------
   copy a block of buffer to FB memory.
   x1,y1,x2,y2:	  LCD area corresponding to FB mem. block
   buf:		  data source

   Return
		1	partial area out of FB mem boundary
		0	OK
		-1	fails
   Midas
   ----------------------------------------------------------------------------*/
   int fb_cpyfrom_buf(FBDEV *fb_dev, int x1, int y1, int x2, int y2, const uint16_t *buf)
   {
	int i,j;
	int xl,xr; /* left right */
	int yu,yd; /* up down */
	int ret=0;
	long int location=0;
	int xres=fb_dev->vinfo.xres;
	int yres=fb_dev->vinfo.yres;
	int tmpx,tmpy;

	/* check buf */
	if(buf==NULL)
	{
		printf("fb_cpyfrom_buf(): buf is NULL!\n");
		return -1;
	}

	/* sort point coordinates */
	if(x1>x2){
		xr=x1;
		xl=x2;
	}
	else{
		xr=x2;
		xl=x1;
	}

	if(y1>y2){
		yu=y1;
		yd=y2;
	}
	else{
		yu=y2;
		yd=y1;
	}




#ifdef FB_DOTOUT_ROLLBACK /* -------------   ROLLBACK  ------------------*/

	/* ------------ copy mem ------------*/
	for(i=yd;i<=yu;i++)
	{
        	for(j=xl;j<=xr;j++)
		{			/* map i,j to LCD(Y,X) */
			if(i<0) /* map Y */
			{
				tmpy=yres-(-i)%yres; /* here tmpy=1-320 */
				tmpy=tmpy%yres;
			}
			else if(i>yres-1)
				tmpy=i%yres;
			else
				tmpy=i;

			if(j<0) /* map X */
			{
				tmpx=xres-(-j)%xres; /* here tmpx=1-240 */
				tmpx=tmpx%xres;
			}
			else if(j>xres-1)
				tmpx=j%xres;
			else
				tmpx=j;

			location=(tmpx+fb_dev->vinfo.xoffset)*(fb_dev->vinfo.bits_per_pixel/8)+
        	        	     (tmpy+fb_dev->vinfo.yoffset)*fb_dev->finfo.line_length;

			/* --- copy to fb ---*/
        		*((uint16_t *)(fb_dev->map_fb+location))=*buf;
			buf++;
		}
	}

#else  /* -----------------  NO ROLLBACK  ------------------------*/

	/* normalize x,y */
	if(yd<0)yd=0;
	if(yu>yres-1)yu=yres-1;

	if(xl<0)xl=0;
	if(xr>xres-1)xr=xres-1;

	/* ------------ copy mem ------------*/
	for(i=yd;i<=yu;i++)
	{
        	for(j=xl;j<=xr;j++)
		{
#if 0 /* This shall never happen now */
			if( i<0 || j<0 || i>yres-1 || j>xres-1 )
			{
				EGI_PDEBUG(DBG_FBGEOM,"WARNING: fb_cpyfrom_buf(): coordinates out of range!\n");
				ret=1;
			}
#endif
			/* map i,j to LCD(Y,X) */
			if(i>yres-1) /* map Y */
				tmpy=yres-1;
			else if(i<0)
				tmpy=0;
			else
				tmpy=i;

			if(j>xres-1) /* map X */
				tmpx=xres-1;
			else if(j<0)
				tmpx=0;
			else
				tmpx=j;

			location=(tmpx+fb_dev->vinfo.xoffset)*(fb_dev->vinfo.bits_per_pixel/8)+
        	        	     (tmpy+fb_dev->vinfo.yoffset)*fb_dev->finfo.line_length;

			/* --- copy to fb ---*/
        		*((uint16_t *)(fb_dev->map_fb+location))=*buf;
			buf++;
		}
	}

#endif

	return ret;

   }




/*--------------------------------------- Method 1 -------------------------------------------
1. draw an image through a map of rotation.
2.

n:		side pixel number of a square image. to be inform of 2*m+1;
x0y0:		left top coordinate of the square.
image:		16bit color buf of a square image
SQMat_XRYR:	Rotation map matrix of the square.
 		point(i,j) map to egi_point_coord SQMat_XRYR[n*i+j]
Midas
------------------------------------------------------------------------------------------*/
/*----------------------- Drawing for method 1  -------------------------*/
#if 0
void fb_drawimg_SQMap(int n, struct egi_point_coord x0y0, uint16_t *image,
						const struct egi_point_coord *SQMat_XRYR)
{
	int i,j,k,m;
	int s;
//	int mapx,mapy;
	uint16_t color;

	/* check if n can be resolved in form of 2*m+1 */
	if( (n-1)%2 != 0)
	{
		printf("fb_drawimg_SQMap(): the number of pixels on the square side must be n=2*m+1.\n");
	 	return;
	}

	/* map each image point index k to get rotated position index m,
	   then draw the color to index m position
	   only sort out points within the circle */
	for(i=-n/2-1;i<n/2;i++) /* row index,from top to bottom, -1 to compensate precision loss */
	{
		s=sqrt(n*n/4-i*i);
		for(j=n/2+1-s;j<=n/2+s;j++) /* +1 to compensate precision loss */
		{
			k=(i+n/2)*n+j;
			/*  for current point index k, get mapped point index m, origin left top */
			m=SQMat_XRYR[k].y*n+SQMat_XRYR[k].x; /* get point index after rotation */
			fbset_color(image[k]);
			draw_dot(&gv_fb_dev, x0y0.x+m%n, x0y0.y+m/n);
		}
	}
}
#endif


/*----------------------- Drawing for Method: revert rotation  --------------------------------
n:              pixel number for square side.
x0y0:		origin point coordinate of the square image,  in LCD coord system.
centxy:         the center point coordinate of the concerning square area of LCD(fb).
image:		original square shape image R5G6B5 data.
SQMat_XRYR:     after rotation
                square matrix after rotation mapping, coordinate origin is same as LCD(fb) origin.
--------------------------------------------------------------------------------------------------*/
void fb_drawimg_SQMap(int n, struct egi_point_coord x0y0, const uint16_t *image,
						const struct egi_point_coord *SQMat_XRYR)
{
	int i,j,k;

	/* check if n can be resolved in form of 2*m+1 */
	if( (n-1)%2 != 0)
	{
		printf("fb_drawimg_SQMap(): the number of pixels on the square side must be n=2*m+1.\n");
	 	return;
	}

	for(i=0;i<n;i++)
		for(j=0;j<n;j++)
		{
			/* since index i point map to  SQMat_XRYR[i](x,y), so get its mapped index k */
			k=SQMat_XRYR[i*n+j].y*n+SQMat_XRYR[i*n+j].x;
			//printf("k=%d\n",k);
			fbset_color(image[k]);
			draw_dot(&gv_fb_dev, x0y0.x+j, x0y0.y+i); /* ???? n-j */
		}
}



/*------------------------ Draw annulus mapping ------------------------------------------------
n:              pixel number for ouside square side. also is the outer diameter for the annulus.
ni:             pixel number for inner square side, also is the inner diameter for the annulus.
x0y0:		origin point coordinate of the square image,  in LCD coord system.
centxy:         the center point coordinate of the concerning square area of LCD(fb).
image:		original square shape image R5G6B5 data.
ANMat_XRYR:     after rotation
                square matrix after rotation mapping, coordinate origin is same as LCD(fb) origin.

Midas Zhou
----------------------------------------------------------------------------------------------*/
void fb_drawimg_ANMap(int n, int ni, struct egi_point_coord x0y0, const uint16_t *image,
		            			       const struct egi_point_coord *ANMat_XRYR)
{
	int i,j,m,k,t,pn;

	/* check if n can be resolved in form of 2*m+1 */
	if( (n-1)%2 != 0)
	{
		printf("fb_drawimg_ANMap(): WARNING: the number of pixels on the square side must be n=2*m+1.\n");
	 	//return;
	}

	/* Drawing only points of the annulus */
        for(j=0; j<=n/2;j++) /* row index,Y: 0->n/2 */
        {
                m=sqrt( (n/2)*(n/2)-j*j ); /* distance from Y to the point on outer circle */

                if(j<ni/2)
                        k=sqrt( (ni/2)*(ni/2)-j*j); /* distance from Y to the point on inner circle */
                else
                        k=0;

                for(i=-m;i<=-k;i++) /* colum index, X: -m->-n, */
                {
			/* since index i point map to  ANMat_XRYR[i](x,y), so get its mapped index t */

                        /* upper and left part of the annuls j: 0->n/2*/
			pn=(n/2-j)*n+n/2+i;
			t=ANMat_XRYR[pn].y*n+ANMat_XRYR[pn].x;/* +n/2 for origin from (n/2,n/2) to (0,0) */
			fbset_color(image[t]); /* get mapped pixel */
			draw_dot(&gv_fb_dev, x0y0.x+n/2+i, x0y0.y+n/2-j);

                        /* lower and left part of the annulus -j: 0->-n/2*/
			pn=(n/2+j)*n+n/2+i;
			t=ANMat_XRYR[pn].y*n+ANMat_XRYR[pn].x;/* +n/2 for origin from (n/2,n/2) to (0,0) */
			fbset_color(image[t]); /* get mapped pixel */
			draw_dot(&gv_fb_dev, x0y0.x+n/2+i, x0y0.y+n/2+j);

                        /* upper and right part of the annuls -i: m->n */
			pn=(n/2-j)*n+n/2-i;
			t=ANMat_XRYR[pn].y*n+ANMat_XRYR[pn].x;/* +n/2 for origin from (n/2,n/2) to (0,0) */
			fbset_color(image[t]); /* get mapped pixel */
			draw_dot(&gv_fb_dev, x0y0.x+n/2-i, x0y0.y+n/2-j);

                        /* lower and right part of the annulus */
			pn=(n/2+j)*n+n/2-i;
			t=ANMat_XRYR[pn].y*n+ANMat_XRYR[pn].x;/* +n/2 for origin from (n/2,n/2) to (0,0) */
			fbset_color(image[t]); /* get mapped pixel */
			draw_dot(&gv_fb_dev, x0y0.x+n/2-i, x0y0.y+n/2+j);
		}
	}
}



/*-----------------------------------------------------------------------------
scale a block of pixel buffer.
 owid,ohgt:	old/original width and height of the pixel area
 nwid,nhgt:   new width and height
 obuf:        old data buffer for 16bit color pixels
 nbuf;	scaled data buffer for 16bit color pixels

Return
	1	partial area out of FB mem boundary
	0	OK
	-1	fails
Midas Zhou
------------------------------------------------------------------------------*/
int fb_scale_pixbuf(unsigned int owid, unsigned int ohgt, unsigned int nwid, unsigned int nhgt,
			const uint16_t *obuf, uint16_t *nbuf)
{
	int i,j;
	int imap,jmap;

	/* 1. check data */
	if(owid==0 || ohgt==0 || nwid==0 || nhgt==0)
	{
		printf("fb_scale_pixbuf(): pixel area is 0! fail to scale.\n");
		return -1;
	}
	if(obuf==NULL || nbuf==NULL)
	{
		printf("fb_scale_pixbuf(): old or new pixel buffer is NULL! fail to scale.\n");
		return -2;
	}

	/* 2. map pixel from old area to new area */
	for(i=1;i<nhgt+1;i++) /* pixel row index */
	{
		for(j=1;j<nwid+1;j++) /* pixel column index */
		{
			/* map i,j to old buf index imap,jmap */
			//imap= round( (float)i/(float)nhgt*(float)ohgt );
			//jmap= round( (float)j/(float)nwid*(float)owid );
			imap= i*ohgt/nhgt;
			jmap= j*owid/nwid;
			//PDEBUG("fb_scale_pixbuf(): imap=%d, jmap=%d\n",imap,jmap);
			/* get mapped pixel color */
			nbuf[ (i-1)*nwid+(j-1) ] = obuf[ (imap-1)*owid+(jmap-1) ];
		}
	}

	return 0;
}

/*-------------------------------------------------------------------
to get a new EGI_POINT by interpolation of 2 points

interpolation direction: from pa to pb
pn:	interpolate point.
pa,pb:	2 points defines the interpolation line.
off:	distance form pa to pn.
	>0  directing from pa to pb.
	<0  directiong from pb to pa.

return:
	2	pn extend from pa
	1	pn extend from pb
	0	ok
	<0	fails, or get end of

Midas Zhou
--------------------------------------------------------------------*/
int egi_getpoit_interpol2p(EGI_POINT *pn, int off, const EGI_POINT *pa, const EGI_POINT *pb)
{
	int ret=0;
	float cosang,sinang;

	/* distance from pa to pb */
	float s=sqrt( (pb->x-pa->x)*(pb->x-pa->x)+(pb->y-pa->y)*(pb->y-pa->y) );
	/* cosine and sine of the slop angle */
	if(s==0)
		return -1;
	else
	{
		cosang=(pb->x-pa->x)/s;
		sinang=(pb->y-pa->y)/s;
	}
	/* check if out of range */
	if(off>s)ret=1;
	if(off<0)ret=2;

	/* interpolate point */
	pn->x=pa->x+off*cosang;
	pn->y=pa->y+off*sinang;

	return ret;
}

/*--------------------------------------------------------
calculate number of steps between 2 points
step:	length of each step
pa,pb	two points

return:
	>0 	number of steps
	=0 	if pa==pb or s<step

Midas Zhou
---------------------------------------------------------*/
int egi_numstep_btw2p(int step, const EGI_POINT *pa, const EGI_POINT *pb)
{
        /* distance from pa to pb */
        float s=sqrt( (pb->x-pa->x)*(pb->x-pa->x)+(pb->y-pa->y)*(pb->y-pa->y) );

	if(step <= 0)
	{
		printf("egi_numstep_btw2p(): step must be greater than zero!\n");
		return -1;
	}

	if(s==0)
	{
		printf("egi_numstep_btw2p(): WARNING!!! point A and B is the same! \n");
		return 0;
	}

	return s/step;
}


/*--------------------------------------------------------
pick a random point within a box

pr:	pointer to a point wihin the box.
box:	the box.

return:
	0	OK
	<0	fail
Midas
---------------------------------------------------------*/
int egi_randp_inbox(EGI_POINT *pr, const EGI_BOX *box)
{
	if(pr==NULL || box==NULL)
	{
		printf("egi_randp_inbox(): pr or box is NULL! \n");
		return -1;
	}

	EGI_POINT pa=box->startxy;
	EGI_POINT pb=box->endxy;

	pr->x=pa.x+(egi_random_max(pb.x-pa.x)-1);  /* if x=-10, -8<= egi_random_max(x) <=1 */
	pr->y=pa.y+(egi_random_max(pb.y-pa.y)-1); /* if x=10,  1<= egi_random_max(x) <=10 */

	return 0;
}


/*--------------------------------------------------------
pick a random point on a box's sides.

pr:	pointer to a point wihin the box.
box:	the box.

return:
	0	OK
	<0	fail
Midas Zhou
---------------------------------------------------------*/
int egi_randp_boxsides(EGI_POINT *pr, const EGI_BOX *box)
{
	if(pr==NULL || box==NULL)
	{
		printf("egi_randp_boxsides(): pr or box is NULL! \n");
		return -1;
	}

	EGI_POINT pa=box->startxy;
	EGI_POINT pb=box->endxy;

	if( egi_random_max(2) -1 ) /* on Width sides */
	{
		pr->x=pa.x+(egi_random_max(pb.x-pa.x)-1);  /* if x=-10, -8<= egi_random_max(x) <=1 */
		if( egi_random_max(2) -1 ) /* up or down side */
			pr->y=pa.y;
		else
			pr->y=pb.y;
	}
	else /* on Heigh sides */
	{
		pr->y=pa.y+(egi_random_max(pb.y-pa.y)-1);  /* if x=-10, -8<= egi_random_max(x) <=1 */
		if( egi_random_max(2) -1 ) /* left or right side */
			pr->x=pa.x;
		else
			pr->x=pb.x;
	}

	return 0;
}



/*----------------------------------------------
With param color
Midas Zhou
-----------------------------------------------*/
void draw_circle2(FBDEV *dev, int x, int y, int r, EGI_16BIT_COLOR color)
{
	fbset_color(color);
	draw_circle(dev, x,y, r);
}
void draw_filled_annulus2(FBDEV *dev, int x0, int y0, int r, unsigned int w, EGI_16BIT_COLOR color)
{
	fbset_color(color);
	draw_filled_annulus(dev, x0, y0, r,  w);
}
void draw_filled_circle2(FBDEV *dev, int x, int y, int r, EGI_16BIT_COLOR color)
{
	fbset_color(color);
	draw_filled_circle(dev, x, y,r);
}
