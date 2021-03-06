/*-------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

page creation jobs:
1. egi_create_XXXpage() function.
   1.1 creating eboxes and page.
   1.2 assign thread-runner to the page.
   1.3 assign routine to the page.
   1.4 assign button functions to corresponding eboxes in page.
2. thread-runner functions.
3. egi_XXX_routine() function if not use default egi_page_routine().
4. button reaction functins

Midas Zhou
---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "egi.h"
#include "egi_color.h"
#include "egi_txt.h"
#include "egi_objtxt.h"
#include "egi_btn.h"
#include "egi_page.h"
#include "egi_symbol.h"
#include "egi_log.h"
#include "egi_timer.h"

/* icon code for button symbols */
#define ICON_CODE_PREV 		0
#define ICON_CODE_PAUSE 	1
#define ICON_CODE_STOP		10
#define ICON_CODE_PLAY 		3
#define ICON_CODE_NEXT 		2
#define ICON_CODE_EXIT 		5
#define ICON_CODE_SHUFFLE	6	/* pick next file randomly */
#define ICON_CODE_REPEATONE	7	/* repeat current file */
#define ICON_CODE_LOOPALL	8	/* loop all files in the list */
#define ICON_CODE_GOHOME	9

static uint16_t btn_symcolor;

static int egi_ffplay_prev(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int egi_ffplay_playpause(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int egi_ffplay_next(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int egi_ffplay_playmode(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);
static int egi_ffplay_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data);


/*---------- [  PAGE ::  Mplayer Operation ] ---------
1. create eboxes for 4 buttons and 1 title bar

Return
	pointer to a page	OK
	NULL			fail
------------------------------------------------------*/
EGI_PAGE *egi_create_ffplaypage(void)
{
	int i;
	int btnum=5;
	EGI_EBOX *ffplay_btns[5];
	EGI_DATA_BTN *data_btns[5];

	/* --------- 1. create buttons --------- */
        for(i=0;i<btnum;i++) /* row of buttons*/
        {
		/* 1. create new data_btns */
		data_btns[i]=egi_btndata_new(i, /* int id */
						square, /* enum egi_btn_type shape */
						&sympg_sbuttons, /* struct symbol_page *icon. If NULL, use geometry. */
						0, /* int icon_code, assign later.. */
						&sympg_testfont /* for ebox->tag font */
						);
		/* if fail, try again ... */
		if(data_btns[i]==NULL)
		{
			EGI_PLOG(LOGLV_ERROR,"egi_create_ffplaypage(): fail to call egi_btndata_new() for data_btns[%d]. retry...\n", i);
			i--;
			continue;
		}

		/* Do not show tag on the button */
		data_btns[i]->showtag=false;

		/* 2. create new btn eboxes */
		ffplay_btns[i]=egi_btnbox_new(NULL, /* put tag later */
						data_btns[i], /* EGI_DATA_BTN *egi_data */
				        	1, /* bool movable */
					        48*i, 320-(60-5), /* int x0, int y0 */
						48,60, /* int width, int height */
				       		0, /* int frame,<0 no frame */
		       				egi_color_random(medium) /*int prmcolor, for geom button only. */
					   );
		/* if fail, try again ... */
		if(ffplay_btns[i]==NULL)
		{
			printf("egi_create_ffplaypage(): fail to call egi_btnbox_new() for ffplay_btns[%d]. retry...\n", i);
			free(data_btns[i]);
			data_btns[i]=NULL;
			i--;
			continue;
		}
	}

	/* get a random color for the icon */
	btn_symcolor=egi_color_random(medium);
	EGI_PLOG(LOGLV_INFO,"%s: set 24bits btn_symcolor as 0x%06X \n",	__FUNCTION__, COLOR_16TO24BITS(btn_symcolor) );

	/* add tags,set icon_code and reaction function here */
	egi_ebox_settag(ffplay_btns[0], "Prev");
	data_btns[0]->icon_code=(btn_symcolor<<16)+ICON_CODE_PREV; /* SUB_COLOR+CODE */
	ffplay_btns[0]->reaction=egi_ffplay_prev;

	egi_ebox_settag(ffplay_btns[1], "Play&Pause");
	data_btns[1]->icon_code=(btn_symcolor<<16)+ICON_CODE_PLAY; /* 13--pause, 15--play */
	ffplay_btns[1]->reaction=egi_ffplay_playpause;

	egi_ebox_settag(ffplay_btns[2], "Next");
	data_btns[2]->icon_code=(btn_symcolor<<16)+ICON_CODE_NEXT;
	ffplay_btns[2]->reaction=egi_ffplay_next;

	egi_ebox_settag(ffplay_btns[3], "Exit");
	data_btns[3]->icon_code=(btn_symcolor<<16)+ICON_CODE_EXIT;
	ffplay_btns[3]->reaction=egi_ffplay_exit;

	egi_ebox_settag(ffplay_btns[4], "Playmode");
	data_btns[4]->icon_code=(btn_symcolor<<16)+ICON_CODE_SHUFFLE;
	ffplay_btns[4]->reaction=egi_ffplay_playmode;



	/* --------- 2. create title bar --------- */
	EGI_EBOX *title_bar= create_ebox_titlebar(
	        0, 0, /* int x0, int y0 */
        	0, 2,  /* int offx, int offy */
		WEGI_COLOR_GRAY, //egi_colorgray_random(medium), //light),  /* int16_t bkcolor */
    		NULL	/* char *title */
	);
	egi_txtbox_settitle(title_bar, "	eFFplay V0.0 ");


	/* --------- 3. create ffplay page ------- */
	/* 3.1 create ffplay page */
	EGI_PAGE *page_ffplay=egi_page_new("page_ffplay");
	while(page_ffplay==NULL)
	{
		printf("egi_create_ffplaypage(): fail to call egi_page_new(), try again ...\n");
		page_ffplay=egi_page_new("page_ffplay");
		tm_delayms(10);
	}

	page_ffplay->ebox->prmcolor=WEGI_COLOR_BLACK;

        /* 3.2 put pthread runner */
        //page_ffplay->runner[0]= ;

        /* 3.3 set default routine job */
        page_ffplay->routine=egi_page_routine; /* use default routine function */

        /* 3.4 set wallpaper */
        page_ffplay->fpath=NULL; //"/tmp/mplay.jpg";


	/* 3.5 add ebox to home page */
	for(i=0;i<btnum;i++) /* add buttons */
		egi_page_addlist(page_ffplay, ffplay_btns[i]);
	egi_page_addlist(page_ffplay, title_bar); /* add title bar */


	return page_ffplay;
}


/*-----------------  RUNNER 1 --------------------------

-------------------------------------------------------*/
static void egi_pageffplay_runner(EGI_PAGE *page)
{

}

/*--------------------------------------------------------------------
ffplay PREV
return
----------------------------------------------------------------------*/
static int egi_ffplay_prev(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	/* only react to status 'pressing' */
	return btnret_OK;
}

/*--------------------------------------------------------------------
ffplay palypause
return
----------------------------------------------------------------------*/
static int egi_ffplay_playpause(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	/* only react to status 'pressing' */
	struct egi_data_btn *data_btn=(struct egi_data_btn *)(ebox->egi_data);

	/* toggle the icon between play and pause */
	if( (data_btn->icon_code<<16) == ICON_CODE_PLAY<<16 )
		data_btn->icon_code=(btn_symcolor<<16)+ICON_CODE_PAUSE;
	else
		data_btn->icon_code=(btn_symcolor<<16)+ICON_CODE_PLAY;

	/* set refresh flag for this ebox */
	egi_ebox_needrefresh(ebox);

	return btnret_OK;
}

/*--------------------------------------------------------------------
ffplay exit
return
----------------------------------------------------------------------*/
static int egi_ffplay_next(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	return btnret_OK;
}

/*--------------------------------------------------------------------
ffplay play mode rotate.
return
----------------------------------------------------------------------*/
static int egi_ffplay_playmode(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
	static int count=0;

        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

	/* only react to status 'pressing' */
	struct egi_data_btn *data_btn=(struct egi_data_btn *)(ebox->egi_data);

	count++;
	if(count>2)
		count=count>>3;

	/* rotate code: SHUFFLE -> REPEATONE -> LOOPALL -> */
	data_btn->icon_code=(btn_symcolor<<16)+(ICON_CODE_SHUFFLE+count%3);

	/* set refresh flag for this ebox */
	egi_ebox_needrefresh(ebox);

	return btnret_OK;
}

/*--------------------------------------------------------------------
ffplay exit
???? do NOT call long sleep function in button functions.
return
----------------------------------------------------------------------*/
static int egi_ffplay_exit(EGI_EBOX * ebox, EGI_TOUCH_DATA * touch_data)
{
        /* bypass unwanted touch status */
        if(touch_data->status != pressing)
                return btnret_IDLE;

        egi_msgbox_create("Message:\n   Click! Start to exit page!", 300, WEGI_COLOR_ORANGE);
        return btnret_REQUEST_EXIT_PAGE;
}
