/*--------------------------------------------------------------
Based on: dranger.com/ffmpeg/tutorialxx.c
					    by Martin Bohme

A simpley example of opening a video file and save some frames 
to ppm files.
---------------------------------------------------------------*/

#include "libavutil/avutil.h"
#include "libswresample/swresample.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

#include <stdio.h>


void SaveFrame(AVFrame *pFrame, int width, int height, int iFrame){
	FILE *pFile;
	char szFilename[32];
	int y;

	//Open file
	printf("----- try to save frame to frame%d.ppm \n",iFrame);
	sprintf(szFilename,"frame%d.ppm",iFrame);
	pFile=fopen(szFilename,"wb");
	if(pFile==NULL){
		printf("Fail to open file for write!\n");
		return;
	}

	//write header
	fprintf(pFile,"P6\n%d %d\n255\n",width,height);
	//write pixel data
	for(y=0; y<height; y++)
		fwrite(pFrame->data[0]+y*pFrame->linesize[0],1,width*3,pFile);

	//close file
	fclose(pFile);
}


int main(int argc, char *argv[]) {
	//Initializing these to NULL prevents segfaults!
	AVFormatContext	*pFormatCtx=NULL;
	int			i, videoStream;
	AVCodecContext		*pCodecCtxOrig=NULL;
	AVCodecContext		*pCodecCtx=NULL;
	AVCodec			*pCodec=NULL;
	AVFrame			*pFrame=NULL;
	AVFrame			*pFrameRGB=NULL;
	AVPacket		packet;
	int			frameFinished;
	int			numBytes;
	uint8_t			*buffer=NULL;
	struct SwsContext	*sws_ctx=NULL;

	if(argc < 2) {
		printf("Please provide a movie file\n");
		return -1;
	}

	//----- Register all formats and codecs
	av_register_all();

	//-----Open video file
	if(avformat_open_input(&pFormatCtx, argv[1], NULL, NULL)!=0) {
		printf("Fail to open video file!\n");
		return -1; 
	}

	//----Retrieve stream information
	if(avformat_find_stream_info(pFormatCtx, NULL)<0) {
		printf(" Fail to find stream information!\n");
		return -1;
	}

	//----Dump information about file onto standard error
	av_dump_format(pFormatCtx, 0, argv[1], 0);

	//-----Find the first video stream
	printf("----- try to find the first video stream... \n");
	videoStream=-1;
	for(i=0; i<pFormatCtx->nb_streams; i++)
		if(pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream=i;
			break;
		}
	if(videoStream == -1) {
		printf("Didn't find a video stream!\n");
		return -1;
	}

	//-----Get a pointer to the codec context for the video stream
	pCodecCtxOrig=pFormatCtx->streams[videoStream]->codec;
	//-----Find the decoder for the video stream
	printf("----- try to find the decoder for the video stream... \n");
	pCodec=avcodec_find_decoder(pCodecCtxOrig->codec_id);
	if(pCodec == NULL) {
		fprintf(stderr, "Unsupported codec!\n");
		return -1;
	}

	//----copy context
	pCodecCtx=avcodec_alloc_context3(pCodec);
	if(avcodec_copy_context(pCodecCtx, pCodecCtxOrig) != 0) {
		fprintf(stderr, "Couldn't copy code context!\n");
		return -1;
	}

	//----open codec
	if(avcodec_open2(pCodecCtx, pCodec, NULL) <0 ) {
		fprintf(stderr, "Cound not open codec!\n");
		return -1;
	}

	//----Allocate video frame
	pFrame=av_frame_alloc();

	//----allocate an AVFrame structure
	printf("----- try to allocate an AVFrame structure...\n");
	pFrameRGB=av_frame_alloc();
	if(pFrameRGB==NULL) {
		fprintf(stderr, "Fail to allocate AVFrame structure!\n");
		return -1;
	}

	//----Determine required buffer size and allocate buffer
	numBytes=avpicture_get_size(PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);
	buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));

	//----Assign appropriate parts of buffer to image planes in pFrameRGB
	//Note that pFrameRGB is an AVFrame, but AVFrame is a superset of AVPicture
	avpicture_fill((AVPicture *)pFrameRGB, buffer, PIX_FMT_RGB24, pCodecCtx->width, pCodecCtx->height);

	//----Initialize SWS context for software scaling
	printf("----- initialize SWS context for software scaling... \n");
	sws_ctx = sws_getContext( pCodecCtx->width,
				  pCodecCtx->height,
				  pCodecCtx->pix_fmt,
				  pCodecCtx->width,
				  pCodecCtx->height,
				  PIX_FMT_RGB24,
				  SWS_BILINEAR,
				  NULL,
				  NULL,
				  NULL
				);

	//----Read frames and save first five frames to disk
	printf("----- read frames and save first five frames to disk... \n");
	i=0;
	while( av_read_frame(pFormatCtx, &packet) >= 0) {
		//is this a packet from the video stream ?
		if(packet.stream_index==videoStream) {
			//decode video frame
			printf("...decoding video frame\n");
			avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);
			//did we get a video frame?
			if(frameFinished) {
				//convert the image from its native format to RGB
				printf("...converting image to RGB\n");
				sws_scale( sws_ctx,
					   (uint8_t const * const *)pFrame->data,
					   pFrame->linesize, 0, pCodecCtx->height,
					   pFrameRGB->data, pFrameRGB->linesize
					);
				//----- save the frame to disk
				if(++i<5)
					SaveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height,i);
				else
					break;
			}
		}

		//---- free the packet that was allocated by av_read_frame
		av_free_packet(&packet);

	}//end of while()

	//----Freee the RGB image
	av_free(buffer);
	av_frame_free(&pFrameRGB);

	//-----Free the YUV frame
	av_frame_free(&pFrame);

	//----Close the codecs
	avcodec_close(pCodecCtx);
	avcodec_close(pCodecCtxOrig);

	//----Close the video file
	avformat_close_input(&pFormatCtx);

	return 0;

}
