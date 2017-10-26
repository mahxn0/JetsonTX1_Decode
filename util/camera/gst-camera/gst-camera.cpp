/*
 * Copyright (c) 2017, NVIDIA CORPORATION. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include "gstCamera.h"

#include "glDisplay.h"
#include "glTexture.h"

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include "loadImage.h"
#include "cudaNormalize.h"


bool signal_recieved = false;

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
	gstCamera* camera = gstCamera::Create();
	
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
	FILE *fp=fopen("1.yuv","w+");
	while(!signal_recieved)
	{
		void *imgCPU=NULL;
		void *imgCUDA=NULL;
		// get the latest frame
		if(!camera->Capture(&imgCPU,&imgCUDA,1000))
		{
			printf("\ngst-camera: failed to capture frame\n");
		}
		else
			printf("gst-camera:  recieved new frame  CPU=0x%p  GPU=0x%p\n",imgCPU,imgCUDA);
			fwrite(imgCPU,m_Width*m_Height*3/2,1,fp);
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
