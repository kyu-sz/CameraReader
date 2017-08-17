// CameraReaderTestbench.cpp : 定义控制台应用程序的入口点。
//

#include "stdafx.h"

#include <CameraReader/CameraReader/camera_reader.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace Theia::Camera;

int main(int argc, char* argv[])
{
	CWebCamReader camera;
	camera.Login("10.1.1.120", 8000, "admin", "hk123456");

	cv::Mat img;

	while (true)
	{
		img = camera.GetImage();
		
		cv::resize(img, img, cv::Size(img.cols >> 1, img.rows >> 1));

		cv::imshow("Web Camera", img);
		if (cv::waitKey(1) == 'c')
			break;
	}

	return 0;
}

