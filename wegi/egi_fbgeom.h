/*------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.


Referring to: http://blog.chinaunix.net/uid-22666248-id-285417.html
 本文的copyright归yuweixian4230@163.com 所有，使用GPL发布，可以自由拷贝，转载。
但转载请保持文档的完整性，注明原作者及原链接，严禁用于任何商业用途。

作者：yuweixian4230@163.com
博客：yuweixian4230.blog.chinaunix.net


Modified and appended by: Midas Zhou
-----------------------------------------------------------------------------*/
#ifndef __EGI_FBGEOM_H__
#define __EGI_FBGEOM_H__

#include <stdio.h>
#include <linux/fb.h>
#include <stdint.h>
#include <stdbool.h>
#include "egi.h"
#include "egi_filo.h"

#define EGI_FBDEV_NAME "/dev/fb0"

/* for draw_dot(), roll back to start if reach boundary of FB mem */
#define no_FB_DOTOUT_ROLLBACK /* also check FB_SYMBOUT_ROLLBACK in symbol.h */

typedef struct fbdev{
        int fdfd; /* file descriptor, open "dev/fb0" */
        struct fb_var_screeninfo vinfo;
        struct fb_fix_screeninfo finfo;
        long int screensize;
        unsigned char *map_fb;
	/* pthread_mutex_t fbmap_lock; */
	EGI_FILO *fb_filo;
	int filo_on;	/* >0,activate FILO push */
}FBDEV;

/* distinguished from PIXEL in egi_bmpjpg.h */
typedef struct fbpixel {
	long int position;
	uint16_t color;
}FBPIX;


/* global variale, Frame buffer device */
extern FBDEV   gv_fb_dev;
extern EGI_BOX gv_fb_box;

/* functions */
int 		init_dev(FBDEV *dev);
void 		release_dev(FBDEV *dev);
inline void 	fbset_homepage_loop(bool loop);
void 		fb_filo_flush(FBDEV *dev);
inline void 	fb_filo_on(FBDEV *dev);
inline void 	fb_filo_off(FBDEV *dev);
bool 	point_inbox(int px,int py,int x1,int y1,int x2,int y2);
bool    box_inbox(EGI_BOX* box, EGI_BOX* container);
bool    box_outbox(EGI_BOX* box, EGI_BOX* container);
void 	fbset_color(uint16_t color);
void 	clear_screen(FBDEV *dev, uint16_t color);
//////////////// draw function ////////////
int 	draw_dot(FBDEV *dev,int x,int y);
void 	draw_line(FBDEV *dev,int x1,int y1,int x2,int y2);
void 	draw_wline_nc(FBDEV *dev,int x1,int y1,int x2,int y2, unsigned w);
void 	draw_wline(FBDEV *dev,int x1,int y1,int x2,int y2, unsigned w);
void 	draw_pline_nc(FBDEV *dev, EGI_POINT *points,int pnum, unsigned int w);
void 	draw_pline(FBDEV *dev, EGI_POINT *points,int pnum, unsigned int w);
void 	draw_oval(FBDEV *dev,int x,int y);
void 	draw_rect(FBDEV *dev,int x1,int y1,int x2,int y2);
void 	draw_wrect(FBDEV *dev,int x1,int y1,int x2,int y2, int w);
int 	draw_filled_rect(FBDEV *dev,int x1,int y1,int x2,int y2);
int     draw_filled_rect2(FBDEV *dev,uint16_t color, int x1,int y1,int x2,int y2);
void 	draw_circle(FBDEV *dev, int x, int y, int r);
void 	draw_pcircle(FBDEV *dev, int x0, int y0, int r, unsigned int w);
void 	draw_filled_triangle(FBDEV *dev, EGI_POINT *points);
void 	draw_filled_annulus(FBDEV *dev, int x0, int y0, int r, unsigned int w);
void 	draw_filled_circle(FBDEV *dev, int x, int y, int r);

//////////////// new draw function, with color /////////////
void draw_circle2(FBDEV *dev, int x, int y, int r, EGI_16BIT_COLOR color);
void draw_filled_annulus2(FBDEV *dev, int x0, int y0, int r, unsigned int w, EGI_16BIT_COLOR color);
void draw_filled_circle2(FBDEV *dev, int x, int y, int r, EGI_16BIT_COLOR color);


int 	fb_cpyto_buf(FBDEV *fb_dev, int x1, int y1, int x2, int y2, uint16_t *buf);
int 	fb_cpyfrom_buf(FBDEV *fb_dev, int x1, int y1, int x2, int y2, const uint16_t *buf);


/* square mapping */
void fb_drawimg_SQMap(int n, struct egi_point_coord x0y0, const uint16_t *image,
   	                                           const struct egi_point_coord *SQMat_XRYR);
/* annulus mapping */
void fb_drawimg_ANMap(int n, int ni, struct egi_point_coord x0y0, const uint16_t *image,
                                                       const struct egi_point_coord *ANMat_XRYR);

int fb_scale_pixbuf(unsigned int owid, unsigned int ohgt, unsigned int nwid, unsigned int nhgt,
                       const uint16_t *obuf, uint16_t *nbuf);

int egi_getpoit_interpol2p(EGI_POINT *pn, int off, const EGI_POINT *pa, const EGI_POINT *pb);
int egi_numstep_btw2p(int step, const EGI_POINT *pa, const EGI_POINT *pb);
int egi_randp_inbox(EGI_POINT *pr, const EGI_BOX *box);
int egi_randp_boxsides(EGI_POINT *pr, const EGI_BOX *box);


#endif
