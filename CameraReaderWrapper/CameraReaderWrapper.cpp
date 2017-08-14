#include <msclr\marshal.h>

#include <cstring>

#include <CameraReader/CameraReader/camera_reader.hpp>
#include <CameraReader/CameraReaderWrapper/CameraReaderWrapper.hpp>

using namespace cv;
using System::IntPtr;

namespace msclr
{
	namespace interop
	{
		template <>
		inline std::string marshal_as(System::String^ const & _from_obj)
		{
			if (_from_obj == nullptr)
			{
				throw gcnew System::ArgumentNullException(_EXCEPTION_NULLPTR);
			}
			std::string _to_obj;
			size_t _size = details::GetAnsiStringSize(_from_obj);

			if (_size > 1)
			{
				// -1 because resize will automatically +1 for the NULL
				_to_obj.resize(_size - 1);
				char *_dest_buf = &(_to_obj[0]);

				details::WriteAnsiString(_dest_buf, _size, _from_obj);
			}

			return _to_obj;
		}
	} //namespace interop
} //namespace msclr

namespace Theia 
{
	namespace Camera
	{
		int SimpleImage::Width::get()
		{
			return m_pImg->cols;
		}

		int SimpleImage::Height::get()
		{
			return m_pImg->rows;
		}

		int SimpleImage::Channels::get() 
		{
			return m_pImg->channels();
		}

		IntPtr SimpleImage::RawPixelData::get() 
		{
			return (IntPtr)m_pImg->data;
		}

		SimpleImage^ SimpleImage::Clone()
		{
			return gcnew SimpleImage(*m_pImg);
		}

		SimpleImage^ SimpleImage::Crop(int x, int y, int width, int height)
		{
			return gcnew SimpleImage(m_pImg->colRange(x, x + width).rowRange(y, y + height));
		}

		// The C++ finalizer destructor ensures that unmanaged resources get
		// released if the user releases the object without explicitly 
		// disposing of it.
		//
		SimpleImage::!SimpleImage()
		{
			// Call the appropriate methods to clean up unmanaged 
			// resources here. If disposing is false when Dispose(bool,
			// disposing) is called, only the following code is executed.
			if (m_pImg)	delete m_pImg;
			m_pImg = NULL;
		}

		SimpleImage::SimpleImage(const Mat& img) 
		{
			m_pImg = new Mat(img.clone());
		}

		SimpleImage::~SimpleImage() 
		{
			this->!SimpleImage();
		}

		array<byte> ^ SimpleImage::GetPixelData()
		{
			array<byte> ^ managed = gcnew array<byte>(Size);
			System::Runtime::InteropServices::Marshal::Copy(
				(System::IntPtr)m_pImg->data,
				managed,
				0,
				Size);
			return managed;
		}

		int SimpleImage::Size::get()
		{
			return Width * Height * Channels;
		}

		array<int> ^ USBCameraReader::EnumerateCameras()
		{
			vector<int> camera_ids;
			CUSBCamReader::EnumerateCameras(camera_ids);

			array<int>^ managed_camera_ids = gcnew array<int>(camera_ids.size());
			for (int i = 0; i < camera_ids.size(); ++i)
				managed_camera_ids[i] = camera_ids[i];

			return managed_camera_ids;
		}

		int CameraReader::GetDefaultImgWidth() { return m_pCameraReader->GetDefaultImgWidth(); }
		int CameraReader::GetDefaultImgHeight() { return m_pCameraReader->GetDefaultImgHeight(); }

		WebCameraReader::WebCameraReader()
		{
			m_pCameraReader = new CWebCamReader;
		}

		USBCameraReader::USBCameraReader(int usb_camera_device)
		{
			try
			{
				m_pCameraReader = new CUSBCamReader(usb_camera_device);
			}
			catch (const std::exception& e)
			{
				System::Exception ^ clr_e = gcnew System::Exception(gcnew System::String(e.what()));
				throw clr_e;
			}
		}

		System::String^ WebCameraReader::GetLastError()
		{
			return gcnew System::String(((CWebCamReader *)m_pCameraReader)->GetLastError());
		}

		LONG WebCameraReader::Login(System::String^ ip, unsigned short port, System::String^ username, System::String^ passwd)
		{
			int ret = ((CWebCamReader *)m_pCameraReader)->Login(
				&msclr::interop::marshal_as<std::string>(ip)[0],
				port,
				&msclr::interop::marshal_as<std::string>(username)[0],
				&msclr::interop::marshal_as<std::string>(passwd)[0]);

			return ret;
		}

		void WebCameraReader::Logout()
		{
			((CWebCamReader *)m_pCameraReader)->Logout();
		}

		CameraReader::!CameraReader()
		{
			if (m_pCameraReader)	delete m_pCameraReader;
			m_pCameraReader = NULL;
		}

		CameraReader::~CameraReader() 
		{
			this->!CameraReader();
		}

		SimpleImage^ CameraReader::GetImage()
		{
			cv::Mat img;
			try
			{
				img = m_pCameraReader->GetImage(0, 0, 3);
			}
			catch (const std::exception& e)
			{
				System::Exception ^ clr_e = gcnew System::Exception(gcnew System::String(e.what()));
				throw clr_e;
			}
			return gcnew SimpleImage(img);
		}
	}
}