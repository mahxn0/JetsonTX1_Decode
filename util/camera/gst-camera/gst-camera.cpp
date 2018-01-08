
#include "gstCamera.h"

#include "glDisplay.h"
#include "glTexture.h"
#include <time.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "loadImage.h"
#include "cudaNormalize.h"
#include "opencv2/opencv.hpp"
#include "opencv2/highgui/highgui.hpp"
using namespace cv;
bool signal_recieved = false;
Mat nv12tomat(unsigned char*yuvdata,unsigned int width,unsigned int height)
{
	Mat nv12_img=Mat(height*3/2,width,CV_8UC1,yuvdata);
	Mat rgb_img;
	cvtColor(nv12_img,rgb_img,CV_YUV2BGR_NV12);
	return rgb_img;
}
void sig_handler(int signo)
{
	if( signo == SIGINT )
	{
		printf("received SIGINT\n");
		signal_recieved = true;
	}
}


int main( int argc, char** argv )
{
	printf("gst-camera\n  args (%i):  ", argc);

	for( int i=0; i < argc; i++ )
		printf("%i [%s]  ", i, argv[i]);
		  
	printf("\n");
	
		
	if( signal(SIGINT, sig_handler) == SIG_ERR )
		printf("\ncan't catch SIGINT\n");

	/*
	 * create the camera device
	 */
	gstCamera* camera = gstCamera::Create(1920,1080);
	
	if( !camera )
	{
		printf("\ngst-camera:  failed to initialize video device\n");
		return 0;
	}
	u_int32_t m_Width=camera->GetWidth();
	u_int32_t m_Height=camera->GetHeight();
	u_int32_t m_Bitdepth=camera->GetPixelDepth();
	printf("\ngst-camera:  successfully initialized video device\n");
	printf("   width:  %u\n", m_Width);
	printf("   height:  %u\n", m_Height);
	printf("   depth:  %u (bpp)\n", m_Bitdepth);
	if( !camera->Open() )
	{
		printf("\ngst-camera:  failed to open camera for streaming\n");
		return 0;
	}
	printf("\ngst-camera:  camera open for streaming\n");
	//FILE *fp=fopen("1.yuv","w+");
	void *imgCUDA=NULL;
	void *imgCPU=NULL;
	while(!signal_recieved)
	{
		struct timeval tvs,tve;
		// get the latest frame
		if(!camera->Capture(&imgCPU,&imgCUDA,1000))
		{
			printf("\ngst-camera: failed to capture frame\n");
		}
		else
			printf("gst-camera:  recieved new frame  CPU=0x%p  GPU=0x%p\n",imgCPU,imgCUDA);
			//fwrite(imgCPU,m_Width*m_Height*3/2,1,fp);
		// if(!camera->ConvertRGBA(imgCUDA,&imgRGBA,1))
		// {
		// 	printf("gst-camera:  failed to convert from NV12 to RGBA\n");
		// }		  
		//Mat img(1280,720,CV_8U,imgCPU);
		
		//nv12tomat((unsigned char *)imgCPU,m_Width,m_Height
		namedWindow("outimg",CV_WINDOW_AUTOSIZE);
		double now=gettimeofday(&tvs,NULL);
		Mat img=nv12tomat((unsigned char *)imgCPU,m_Width,m_Height);
		gettimeofday(&tve,NULL);
        double span = tve.tv_sec-tvs.tv_sec + (tve.tv_usec-tvs.tv_usec)/1000000.0;
		printf("gettimeofday time: %.12f\n",span);
		imshow("outimg",img);
		waitKey(1);
		//printf("img height=%d\n",img.cols);
		//printf("img width=%d\n",img.rows);
		//imwrite("1.bmp",img);
	}
	//shutdown the camera device
	if( camera != NULL )
	{
		delete camera;
		camera = NULL;
	}

	// if( display != NULL )
	// {
	// 	delete display;
	// 	display = NULL;
	// }
	
	printf("gst-camera:  video device has been un-initialized.\n");
	printf("gst-camera:  this concludes the test of the video device.\n");
	return 0;
}
