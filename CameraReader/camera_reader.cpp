#include <ctime>
#include <cmath>
#include <algorithm>
#include <map>
#include <cstdlib>
#include <mutex>
#include <unordered_map>

#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <CameraReader/CameraReader/camera_reader.hpp>

#ifdef _NO_HKSDK
#define CODEC "MPEG-4"
#else
#include <CameraReader/CameraReader/hikvision/PlayM4.h>
#include <CameraReader/CameraReader/hikvision/HCNetSDK.h>
#endif

#ifndef NET_DVR_NOERROR
#define NET_DVR_NOERROR 0
#endif

#ifdef _WIN32
//! Pause and wait for a key.
#define PAUSE system("PAUSE")
#include <direct.h>
//! Get the running directory.
#define GETCWD(buf,size) _getcwd(buf,size)
//! Sleep for x milliseconds.
#define SLEEP_MS(x) Sleep(x)
//! p_Tm is a preallocated "tm" structure. There is no guarantee that the resulting p_Tm remains at the same memory address.
#define LOCALTIME(p_Tm, p_Time) (localtime_s(p_Tm, p_Time), *p_Tm)
#define PATH_MAX _MAX_PATH
#define GetAbsolutePath(filename, buf_len, buf)	GetFullPathNameA(filename, buf_len, buf, NULL)
#else
//! Pause and wait for a key.
#define PAUSE getc(stdin)
#include <unistd.h>
//! Get the running directory.
#define GETCWD(buf,size) getcwd(buf,size)
//! Sleep for x milliseconds.
#define SLEEP_MS(x) usleep(x * 1000)
//! p_Tm is a preallocated "tm" structure. There is no guarantee that the resulting p_Tm remains at the same memory address.
#define LOCALTIME(p_Tm, p_Time) (delete p_Tm, p_Tm = localtime(p_Time))
#define GetAbsolutePath(filename, buf_len, buf)	realpath(filename, buf)
#endif

using namespace std;
using namespace cv;

namespace Theia
{
	namespace Camera
	{
		struct CamCap
		{
			int usage_cnt = 0;
			VideoCapture cap;
			mutex lock;
		};
		//! Key is device ID.
		unordered_map<int, CamCap> usb_cams_;
		void ReleaseCap(CamCap& cam_cap)
		{
			if (!--cam_cap.usage_cnt)
				cam_cap.cap.release();
		}
		void InitUSBCap(int usb_camera_device, int max_img_width, int max_img_height)
		{
			CamCap& cam = usb_cams_[usb_camera_device];

			if (!cam.usage_cnt)
			{
				cam.cap.open(usb_camera_device);
				Mat test_frame;

				if (cam.cap.isOpened())
				{
					cam.cap.set(CV_CAP_PROP_FRAME_WIDTH, max_img_width);
					cam.cap.set(CV_CAP_PROP_FRAME_HEIGHT, max_img_height);

					int failed_times = -1;
					do
					{
						cam.cap >> test_frame;
						++failed_times;
						if (failed_times > 1000)
							throw CCameraNoInputException("The USB camera's input is empty!");
					} while (test_frame.empty());

					cout << "USB camera " << usb_camera_device << " initialized! (" << test_frame.cols << 'x' << test_frame.rows << ')' << endl;
				}
				else
					throw CCameraNotFoundException("Cannot find USB camera!");
			}

			++cam.usage_cnt;
		}

#ifndef _NO_HKSDK
		map<DWORD, CWebCamReader*> g_client_list;
		int CWebCamReader::g_client_cnt = 0;
#endif

#ifndef _NO_HKSDK
		void CALLBACK g_RealDataCallBack_V30(LONG lRealHandle, DWORD dwDataType, BYTE *pBuffer, DWORD dwBufSize, DWORD dwUser)
		{
			//HWND hWnd = GetConsoleWindow();
			const HWND hWnd = NULL;

			CWebCamReader* pClient = g_client_list[dwUser];

			switch (dwDataType)
			{
			case NET_DVR_SYSHEAD: //系统头
				if (!PlayM4_GetPort(&pClient->port_))  //获取播放库未使用的通道号
					break;
				//PlayM4_SetDecodeFrameType(pClient->port_, 1);
				PlayM4_SkipErrorData(pClient->port_, true);
				PlayM4_SetDisplayBuf(pClient->port_, 2);

				//m_iPort = pClient->port_; //第一次回调的是系统头，将获取的播放库port号赋值给全局port，下次回调数据时即使用此port号播放
				if (dwBufSize > 0)
				{
					if (!PlayM4_SetStreamOpenMode(pClient->port_, STREAME_REALTIME))  //设置实时流播放模式
					{
						cout << "Error " << PlayM4_GetLastError(pClient->port_) << " occured when setting stream open mode!" << endl;
						break;
					}

					if (!PlayM4_OpenStream(pClient->port_, pBuffer, dwBufSize, SOURCE_BUF_MAX)) //打开流接口
					{
						cout << "Error " << PlayM4_GetLastError(pClient->port_) << " occured when opening stream!" << endl;
						break;
					}

					if (!PlayM4_Play(pClient->port_, hWnd)) //播放开始
					{
						cout << "Error " << PlayM4_GetLastError(pClient->port_) << " occured when starting to play!" << endl;
						break;
					}
				}
			case NET_DVR_STREAMDATA:   //码流数据
				if (dwBufSize > 0 && pClient->port_ != -1)
				{
					pClient->img_prepared_ = false;

					while (!PlayM4_InputData(pClient->port_, pBuffer, dwBufSize))
					{
						if (PlayM4_GetLastError(pClient->port_) != PLAYM4_BUF_OVER)
						{
							cout << "Error " << PlayM4_GetLastError(pClient->port_) << " occured when inputting data!" << endl;
							break;
						}
						cout << "Buffer overflow when input data! Retrying after 5ms..." << endl;
						SLEEP_MS(1);
					}

					//cout << dwBufSize << endl;

					if (dwBufSize == 20)
					{
						if (!PlayM4_GetBMP(pClient->port_, pClient->decode_buf_, pClient->decode_buf_size_, &dwBufSize))
						{
							cout << "Error " << PlayM4_GetLastError(pClient->port_) << " occured when getting bmp!" << endl;
							break;
						}

						PlayM4_GetPictureSize(pClient->port_, &pClient->default_img_width_, &pClient->default_img_height_);
						pClient->img_prepared_ = true;

						SLEEP_MS(10);
					}
				}
			}
		}
#endif

		cv::Mat CCamReader::GetImage(int width, int height, int channels, bool crop, bool flip, int flip_mode)
		{
			img_buf_ = GetImage();

			if (img_buf_.empty())
				img_buf_ = cv::Mat(0, 0, CV_8UC3);
			else
			{
				Convert(img_buf_, channels);

				if (width || height && (width != default_img_width_ || height != default_img_height_))
				{
					if (crop)
					{
						if (width * default_img_height_ > default_img_width_ * height)	//width / height > default_img_width_ / default_img_height_
						{
							const int cut = (default_img_height_ - height * default_img_width_ / width) >> 1;
							cv::resize(img_buf_.rowRange(cut, default_img_height_ - cut), img_buf_, cv::Size(width, height));
						}
						else
						{
							const int cut = (default_img_width_ - width * default_img_height_ / height) >> 1;
							cv::resize(img_buf_.colRange(cut, default_img_width_ - cut), img_buf_, cv::Size(width, height));
						}
					}
					else
						cv::resize(img_buf_, img_buf_, cv::Size(width, height));
				}

				if (flip)
					cv::flip(img_buf_, img_buf_, flip_mode);
			}

			return img_buf_;
		}

		const cv::Mat& CWebCamReader::GetImage()
		{
#ifdef _NO_HKSDK
			int attempt_cnt = 0;
			do
			{
				cap_ >> img_buf_;
				++attempt_cnt;
			} while (img_buf_.empty() && attempt_cnt < 100);
			return img_buf_;
#else
			while (!img_prepared_)
				SLEEP_MS(5);
			img_buf_ = cv::Mat(default_img_height_, default_img_width_, CV_8UC4, decode_buf_ + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER));
			img_prepared_ = false;
			return img_buf_;
#endif
		}

		const cv::Mat& CCamCapReader::GetImage()
		{
			auto& cam = usb_cams_[usb_camera_device_];
			while (!cam.lock.try_lock())
				SLEEP_MS(1);
			int attempt_cnt = 0;
			do
			{
				cam.cap >> img_buf_;
				++attempt_cnt;
			} while (img_buf_.empty() && attempt_cnt < 100);
			cam.lock.unlock();
			return img_buf_;
		}

#ifndef _NO_HKSDK
		void CALLBACK g_ExceptionCallBack(DWORD dwType, LONG user_id_, LONG lHandle, void *pUser)
		{
			char tempbuf[256] = { 0 };
			switch (dwType)
			{
			case EXCEPTION_RECONNECT:
				printf("----------reconnect--------%lld\n", time(NULL));
				break;
			default:
				break;
			}
		}
#endif

#define SKIN_R	227
#define SKIN_G	181
#define SKIN_B	172
#define YCrCb_STEP 3

#pragma warning(suppress: 6262)
		void Balance(cv::Mat& img, bool for_global, bool for_face)
		{
			if (img.type() == CV_8U)
				cv::equalizeHist(img, img);
			else
			{
				const int step = (img.type() == CV_8UC3) ? 3 : 4;

				const int pixel_num = img.rows * img.cols;

				if (for_global)
				{
					int brightest = -1, darkest = 2048;
					int b_r = 255, b_g = 255, b_b = 255;
					int d_r = 0, d_g = 0, d_b = 0;

					int r_bias;
					int g_bias;
					int b_bias;

					{
						int color_cnt[32][32][32] = {};
						long long r_cnt = 0, g_cnt = 0, b_cnt = 0;
						for (int y = 0; y < img.rows; ++y)
						{
							const uchar* line = img.ptr(y);
							for (int x = 0; x < img.cols; ++x)
							{
								//Count pixels of different color
								++color_cnt[line[0] >> 3][line[1] >> 3][line[2] >> 3];
								//Sum up the value of rgb of each pixels
								r_cnt += line[0];
								g_cnt += line[1];
								b_cnt += line[2];
								//Move to the next pixel
								line += step;
							}
						}

						r_cnt /= pixel_num;
						g_cnt /= pixel_num;
						b_cnt /= pixel_num;
						const long long min_cnt = min(min(r_cnt, g_cnt), b_cnt);

						r_bias = int(r_cnt - min_cnt);
						g_bias = int(g_cnt - min_cnt);
						b_bias = int(b_cnt - min_cnt);

						for (int r = 0; r < 32; ++r)
						{
							for (int g = 0; g < 32; ++g)
							{
								for (int b = 0; b < 32; ++b)
								{
									if (color_cnt[r][g][b] >(pixel_num >> 12))
									{
										const int darkness_evaluator = r + g + b + (max(max(r, g), b) << 1) - min(min(r, g), b);
										if (darkness_evaluator < darkest)
										{
											darkest = darkness_evaluator;
											d_r = r;
											d_g = g;
											d_b = b;
										}

										const int brightness_evaluator = r + g + b + (min(min(r, g), b) << 1) - max(max(r, g), b);
										if (brightness_evaluator > brightest)
										{
											brightest = brightness_evaluator;
											b_r = r;
											b_g = g;
											b_b = b;
										}
									}
								}
							}
						}
					}

					if (b_r == d_r || b_g == d_g || b_b == d_b)
					{
						fprintf(stderr, "Unable to balance!\n");
						return;
					}

					b_r = (b_r << 3) | (b_r >> 2);
					b_g = (b_g << 3) | (b_g >> 2);
					b_b = (b_b << 3) | (b_b >> 2);

					d_r = (d_r << 3) | (d_r >> 2);
					d_g = (d_g << 3) | (d_g >> 2);
					d_b = (d_b << 3) | (d_b >> 2);

					float r_ratio = 255.f / (b_r - d_r);
					float g_ratio = 255.f / (b_g - d_g);
					float b_ratio = 255.f / (b_b - d_b);

					r_bias = (r_bias + (d_r << 3) - d_r) >> 3;
					g_bias = (g_bias + (d_g << 3) - d_g) >> 3;
					b_bias = (b_bias + (d_b << 3) - d_b) >> 3;

					for (int y = 0; y < img.rows; ++y)
					{
						uchar* line = img.ptr(y);
						for (int x = 0; x < img.cols; ++x)
						{
							line[0] = min((int)(max(line[0] - r_bias, 0) * r_ratio), 255);
							line[1] = min((int)(max(line[1] - g_bias, 0) * g_ratio), 255);
							line[2] = min((int)(max(line[2] - b_bias, 0) * b_ratio), 255);
							line += step;
						}
					}
				}

				if (for_face)
				{
					int original_type = img.type();

					if (original_type == CV_8UC3)	//rgb
						cv::cvtColor(img, img, CV_RGB2YCrCb);
					else	//rgba
					{
						cv::cvtColor(img, img, CV_RGBA2RGB);
						cv::cvtColor(img, img, CV_RGB2YCrCb);
					}

					const int SKIN_Cb = int(0.439*SKIN_R - 0.368*SKIN_G - 0.071*SKIN_B + 128);
					const int SKIN_Cr = int(-0.148*SKIN_R - 0.291*SKIN_G + 0.439*SKIN_B + 128);

					int cr_cnt = 0;
					int cb_cnt = 0;
					for (int y = img.rows >> 2; y < (img.rows * 3) >> 2; ++y)
					{
						const uchar* line = img.ptr(y);
						for (int x = img.cols >> 2; x < (img.cols * 3) >> 2; ++x)
						{
							cr_cnt += line[1];
							cb_cnt += line[2];
							line += step;
						}
					}
					const float cr_ratio = SKIN_Cr * (pixel_num >> 2) / (float)cr_cnt;
					const float cb_ratio = SKIN_Cb * (pixel_num >> 2) / (float)cb_cnt;

					for (int y = 0; y < img.rows; ++y)
					{
						uchar* line = img.ptr(y);
						for (int x = 0; x < img.cols; ++x)
						{
							line[1] = min(int(line[1] * cr_ratio), 255);
							line[2] = min(int(line[2] * cb_ratio), 255);
							line += step;
						}
					}

					if (original_type == CV_8UC3)	//rgb
						cv::cvtColor(img, img, CV_YCrCb2RGB);
					//cv::cvtColor(img, img, CV_YCrCb2BGR);
					else	//rgba
					{
						cv::cvtColor(img, img, CV_YCrCb2RGB);
						cv::cvtColor(img, img, CV_RGB2RGBA);
					}
				}
			}
		}

		long CWebCamReader::Login(_In_ const char* dev_ip, unsigned short port, _In_ const char* username, _In_ const char* passwd)
		{
			if (online_)
				Logout();
#ifndef _NO_HKSDK
			//---------------------------------------
			// 注册设备
			NET_DVR_DEVICEINFO_V30 struDeviceInfo;
			user_id_ = NET_DVR_Login_V30(
				const_cast<char*>(dev_ip),
				port,
				const_cast<char*>(username), 
				const_cast<char*>(passwd),
				&struDeviceInfo);
			if (user_id_ < 0)
			{
				fprintf(stderr, "Login error %d: %s\n", NET_DVR_GetLastError(), NET_DVR_GetErrorMsg());
				return (last_error_ = NET_DVR_GetLastError());
			}
			g_client_list[user_id_] = this;


			//---------------------------------------
			//启动预览并设置回调数据流
			NET_DVR_CLIENTINFO ClientInfo = { 0 };
			ClientInfo.hPlayWnd = NULL;         //需要SDK解码时句柄设为有效值，仅取流不解码时可设为空
			ClientInfo.lChannel = 1;       //预览通道号
			//ClientInfo.lLinkMode = 1 << 31;       //最高位(31)为0表示主码流，为1表示子码流0～30位表示连接方式：0－TCP方式；1－UDP方式；2－多播方式；3－RTP方式;
			ClientInfo.lLinkMode = 0;       //最高位(31)为0表示主码流，为1表示子码流0～30位表示连接方式：0－TCP方式；1－UDP方式；2－多播方式；3－RTP方式;
			ClientInfo.sMultiCastIP = NULL;   //多播地址，需要多播预览时配置

			real_play_handle_ = NET_DVR_RealPlay_V30(user_id_, &ClientInfo, NULL, NULL, TRUE);

			if (real_play_handle_ < 0)
			{
				printf("NET_DVR_RealPlay_V30 error\n");
				NET_DVR_Logout(user_id_);
				return (last_error_ = NET_DVR_GetLastError());
			}

			if (!NET_DVR_SetRealDataCallBack(real_play_handle_, g_RealDataCallBack_V30, 0))
			{
				printf("NET_DVR_SetRealDataCallBack error\n");
				return (last_error_ = NET_DVR_GetLastError());
			}
#else
			stringstream rtsp_url_ss;
			rtsp_url_ss << "rtsp://" << username << ":" << passwd << "@" << dev_ip << ":" << port << "/" << CODEC << "/ch1/main/av_stream";
			cap_.open(rtsp_url_ss.str());
			if (!cap_.isOpened())
				return -1;
#endif
			online_ = true;

			return (last_error_ = NET_DVR_NOERROR);
		}

		inline void Convert(cv::Mat& img, int num_channels)
		{
			if (num_channels == 1)
			{
				if (img.type() == CV_8UC3)
					cv::cvtColor(img, img, CV_RGB2GRAY);
				else if (img.type() == CV_8UC4)
					cv::cvtColor(img, img, CV_RGBA2GRAY);
			}
			else if (num_channels == 3)
			{
				if (img.type() == CV_8U)
					cv::cvtColor(img, img, CV_GRAY2RGB);
				else if (img.type() == CV_8UC4)
					cv::cvtColor(img, img, CV_RGBA2RGB);
			}
			else if (num_channels == 4)
			{
				if (img.type() == CV_8U)
					cv::cvtColor(img, img, CV_GRAY2RGBA);
				else if (img.type() == CV_8UC3)
					cv::cvtColor(img, img, CV_RGB2RGBA);
			}
		}

		const char* CWebCamReader::GetLastError()
		{
#ifndef _NO_HKSDK
			return NET_DVR_GetErrorMsg(&last_error_);
#else
			return last_error_ == 0 ? "No error." : last_error_ == -1 ? "Unable to open the destination camera with RTSP." : "Unknown error.";
#endif
		}

		void CWebCamReader::Logout()
		{
#ifndef _NO_HKSDK
			//---------------------------------------
			PlayM4_Stop(port_);
			PlayM4_CloseStream(port_);
			PlayM4_FreePort(port_);
			//关闭预览
			NET_DVR_StopRealPlay(real_play_handle_);
			//注销用户
			NET_DVR_Logout_V30(user_id_);
#else
			cap_.release();
#endif
			online_ = false;
		}

		CWebCamReader::CWebCamReader(int max_img_width, int max_img_height) : online_(false)
		{
#ifndef _NO_HKSDK
			if (g_client_cnt == 0)
			{
				//---------------------------------------
				// 初始化
				NET_DVR_Init();
			}

			//---------------------------------------
			//设置异常消息回调函数
			NET_DVR_SetExceptionCallBack_V30(0, NULL, g_ExceptionCallBack, NULL);

			default_img_width_ = max_img_width;
			default_img_height_ = max_img_height;
			decode_buf_size_ = default_img_width_ * default_img_height_ * sizeof(BYTE) * 4 + sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

			decode_buf_ = (PBYTE)_aligned_malloc(decode_buf_size_, 32);

			++g_client_cnt;
#endif
		}

		CCamCapReader::~CCamCapReader()
		{
			ReleaseCap(usb_cams_[usb_camera_device_]);
		}

		CCamCapReader::CCamCapReader(int usb_camera_device, int max_img_width, int max_img_height) :
			usb_camera_device_(usb_camera_device)
		{
			InitUSBCap(usb_camera_device, max_img_width, max_img_height);

			default_img_width_ = (int)usb_cams_[usb_camera_device_].cap.get(CV_CAP_PROP_FRAME_WIDTH);
			default_img_height_ = (int)usb_cams_[usb_camera_device_].cap.get(CV_CAP_PROP_FRAME_HEIGHT);
		}

		CWebCamReader::~CWebCamReader()
		{
#ifndef _NO_HKSDK
			--g_client_cnt;

			if (!g_client_cnt)
				NET_DVR_Cleanup();

			_aligned_free(decode_buf_);
#endif
		}

		bool CCamCapReader::EnumerateCameras(vector<int> &cam_idx)
		{
			cam_idx.clear();
			struct CapDriver{
				int enumValue; string enumName; string comment;
			};
			// list of all CAP drivers (see highgui_c.h)  
			vector<CapDriver> drivers;
			drivers.push_back({ CV_CAP_MIL, "CV_CAP_MIL", "MIL proprietary drivers" });
			drivers.push_back({ CV_CAP_VFW, "CV_CAP_VFW", "platform native" });
			drivers.push_back({ CV_CAP_FIREWARE, "CV_CAP_FIREWARE", "IEEE 1394 drivers" });
			drivers.push_back({ CV_CAP_STEREO, "CV_CAP_STEREO", "TYZX proprietary drivers" });
			drivers.push_back({ CV_CAP_QT, "CV_CAP_QT", "QuickTime" });
			drivers.push_back({ CV_CAP_UNICAP, "CV_CAP_UNICAP", "Unicap drivers" });
			drivers.push_back({ CV_CAP_DSHOW, "CV_CAP_DSHOW", "DirectShow (via videoInput)" });
			drivers.push_back({ CV_CAP_MSMF, "CV_CAP_MSMF", "Microsoft Media Foundation (via videoInput)" });
			drivers.push_back({ CV_CAP_PVAPI, "CV_CAP_PVAPI", "PvAPI, Prosilica GigE SDK" });
			drivers.push_back({ CV_CAP_OPENNI, "CV_CAP_OPENNI", "OpenNI (for Kinect)" });
			drivers.push_back({ CV_CAP_OPENNI_ASUS, "CV_CAP_OPENNI_ASUS", "OpenNI (for Asus Xtion)" });
			drivers.push_back({ CV_CAP_ANDROID, "CV_CAP_ANDROID", "Android" });
			drivers.push_back({ CV_CAP_ANDROID_BACK, "CV_CAP_ANDROID_BACK", "Android back camera" }),
			drivers.push_back({ CV_CAP_ANDROID_FRONT, "CV_CAP_ANDROID_FRONT", "Android front camera" }),
			drivers.push_back({ CV_CAP_XIAPI, "CV_CAP_XIAPI", "XIMEA Camera API" });
			drivers.push_back({ CV_CAP_AVFOUNDATION, "CV_CAP_AVFOUNDATION", "AVFoundation framework for iOS" });
			drivers.push_back({ CV_CAP_GIGANETIX, "CV_CAP_GIGANETIX", "Smartek Giganetix GigEVisionSDK" });
			drivers.push_back({ CV_CAP_INTELPERC, "CV_CAP_INTELPERC", "Theia Perceptual Computing SDK" });

			std::string winName, driverName, driverComment;
			int driverEnum;
			Mat frame;
			bool found;
			std::cout << "Searching for cameras IDs..." << endl << endl;
			for (int drv = 0; drv < drivers.size(); drv++)
			{
				driverName = drivers[drv].enumName;
				driverEnum = drivers[drv].enumValue;
				driverComment = drivers[drv].comment;
				std::cout << "Testing driver " << driverName << "...";
				found = false;

				int maxID = 100; //100 IDs between drivers  
				if (driverEnum == CV_CAP_VFW)
					maxID = 10; //VWF opens same camera after 10 ?!?  
				else if (driverEnum == CV_CAP_ANDROID)
					maxID = 98; //98 and 99 are front and back cam  
				else if ((driverEnum == CV_CAP_ANDROID_FRONT) || (driverEnum == CV_CAP_ANDROID_BACK))
					maxID = 1;

				for (int idx = 0; idx <maxID; idx++)
				{
					VideoCapture cap(driverEnum + idx);  // open the camera  
					if (cap.isOpened())                  // check if we succeeded  
					{
						found = true;
						cap >> frame;
						if (frame.empty())
							std::cout << endl << driverName << "+" << idx << "\t opens: OK \t grabs: FAIL";
						else
						{
							std::cout << endl << driverName << "+" << idx << "\t opens: OK \t grabs: OK";
							cam_idx.push_back(driverEnum + idx);  // vector of all available cameras 
						}
						// display the frame  
						// imshow(driverName + "+" + to_string(idx), frame); waitKey(1);  
					}
					cap.release();
				}
				if (!found) cout << "Nothing !" << endl;
				cout << endl;
			}
			cout << cam_idx.size() << " camera IDs has been found ";
			cout << "Press a key..." << endl; cin.get();

			return (cam_idx.size()>0); // returns success  
		}
	}
}