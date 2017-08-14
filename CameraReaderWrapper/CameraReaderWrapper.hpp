#pragma once

using System::Byte;

namespace cv
{
	class Mat;
}

namespace Theia 
{
	namespace Camera
	{
		public ref class SimpleImage : public System::IDisposable
		{
		public:
			SimpleImage(const cv::Mat& img);
			!SimpleImage();
			~SimpleImage();

			SimpleImage^ Clone();

			SimpleImage^ Crop(int x, int y, int width, int height);

			inline bool IsEmpty() { return Width == 0 || Height == 0; }

			property int Width
			{
				int get();
			}

			property int Height
			{
				int get();
			}

			property int Channels
			{
				int get();
			}

			property int Size
			{
				int get();
			}

			property System::IntPtr RawPixelData
			{
				System::IntPtr get();
			}

			array<Byte> ^ GetPixelData();
		private:
			cv::Mat* m_pImg = NULL;
		};

		class CCamReader;

		public ref class CameraReader : public System::IDisposable
		{
		public:
			!CameraReader();
			~CameraReader();

			SimpleImage^ GetImage();

			int GetDefaultImgWidth();
			int GetDefaultImgHeight();
		protected:
			CCamReader* m_pCameraReader = NULL;
		};

		public ref class WebCameraReader : public CameraReader
		{
		public:
			long Login(System::String^ ip, unsigned short port, System::String^ username, System::String^ passwd);
			void Logout();

			WebCameraReader();

			System::String^ GetLastError();
		};

		public ref class USBCameraReader : public CameraReader
		{
		public:
			static array<int> ^ EnumerateCameras();

			USBCameraReader(int usb_camera_device);
		};
	}
}