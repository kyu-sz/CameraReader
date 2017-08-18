/*!*****************************************************************************
 * Copyright 2015-2017 Theia Corporation All Rights Reserved.
 *
 * The source code,  information  and material  ("Material") contained  herein is
 * owned by Theia Corporation or its  suppliers or licensors,  and  title to such
 * Material remains with Theia  Corporation or its  suppliers or  licensors.  The
 * Material  contains  proprietary  information  of  Theia or  its suppliers  and
 * licensors.  The Material is protected by  worldwide copyright  laws and treaty
 * provisions.  No part  of  the  Material   may  be  used,  copied,  reproduced,
 * modified, published,  uploaded, posted, transmitted,  distributed or disclosed
 * in any way without Theia's prior express written permission.  No license under
 * any patent,  copyright or other  intellectual property rights  in the Material
 * is granted to  or  conferred  upon  you,  either   expressly,  by implication,
 * inducement,  estoppel  or  otherwise.  Any  license   under such  intellectual
 * property rights must be express and approved by Theia in writing.
 *
 * Unless otherwise agreed by Theia in writing,  you may not remove or alter this
 * notice or  any  other  notice   embedded  in  Materials  by  Theia  or Theia's
 * suppliers or licensors in any way.
 *******************************************************************************/

/*!	@file camera_reader.h
 *	@brief C API header for CCamReader.
 *
 *	This is a C API header for CCamReader.
 *	It contains definition of a camera helper base class and various classes implementing it.
 */

#pragma once

#include <iostream>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

/*!	@def CAMERAREADER_API
 *	@brief A macro for VC++ auto dll import-export configuration.
 *
 *	This macro works only for VC++.
 *	The DLL project should predefine CAMERAREADER_EXPORTS in project settings,
 *	and extern projects using this header should not.
 *	This macro checks the predefinition and determines whether the classes or functions labeled
 *	by this macro should be imported from an extern DLL or exported to the target DLL.
 */
#ifdef _WIN32
#ifdef CAMERAREADER_EXPORTS
#define CAMERAREADER_API __declspec(dllexport)
#else
#define CAMERAREADER_API __declspec(dllimport)
#endif
#else
#define CAMERAREADER_API
#endif

#ifdef _WIN32
#include <Windows.h>
#else
/*!	@def _In_
 *	@brief Macro definition for capability of "Input to called function" SAL annotation for non-VC++ compilers.
 *
 *	SAL annotations are a feature of VC++ compilers, but adding these annotations might cause compilation errors in other compilers.
 *	Defining this kind of empty macro definition can eliminate these errors, but cannot maintain the SAL function.
 *	The original meaning of this label is "Data is passed to the called function, and is treated as read-only."
 */
#define _In_
/*!	@def _Out_
 *	@brief Macro definition for capability of "Output to caller" SAL annotation for non-VC++ compilers.
 *
 *	SAL annotations are a feature of VC++ compilers, but adding these annotations might cause compilation errors in other compilers.
 *	Defining this kind of empty macro definition can eliminate these errors, but cannot maintain the SAL function.
 *	The original meaning of this label is "The caller only provides space for the called function to write to. The called function writes data into that space."
 */
#define _Out_
/*!	@def	_Inout_
 *	@brief	Macro definition for capability of "Input to called function, and output to caller" SAL annotation for non-VC++ compilers.
 *
 *	SAL annotations are a feature of VC++ compilers, but adding these annotations might cause compilation errors in other compilers.
 *	Defining this kind of empty macro definition can eliminate these errors, but cannot maintain the SAL function.
 *	The original meaning of this label is "Usable data is passed into the function and potentially is modified."
 */
#define _Inout_
/*!	@def	_Outptr_
 *	@brief	Macro definition for capability of "Output of pointer to caller" SAL annotation for non-VC++ compilers.
 *
 *	SAL annotations are a feature of VC++ compilers, but adding these annotations might cause compilation errors in other compilers.
 *	Defining this kind of empty macro definition can eliminate these errors, but cannot maintain the SAL function.
 *	The original meaning of this label is "Like Output to caller. The value that's returned by the called function is a pointer."
 */
#define _Outptr_
#endif

//#define _NO_HKSDK

// HikVision SDK is not available on ARM.
#if defined(__arm__) || defined(__aarch64__)
//! Define _NO_HKSDK in situations where HikVision SDK is not available.
//! The module will then use OpenCV VideoCapture to read from web cameras.
#define _NO_HKSDK
#endif

/*! @namespace Theia
 *	Programs by Theia.
 */
namespace Theia
{
	namespace Camera
	{
		/*!	@class CCamReader
		 *	@brief Base class for camera helpers.
		 *
		 *	This is a base class of helper classes for connecting and retrieving videos from cameras.
		 *	It returns videos by frame in OpenCV Mat format.
		 */
		class CAMERAREADER_API CCamReader
		{
		public:
			/*! Get the width of the default frame.
			 *	@return		The width of the default frame.
			 */
			inline int GetDefaultImgWidth() const { return default_img_width_; }
			/*! Get the height of the default frame.
			 *	@return		The height of the default frame.
			 */
			inline int GetDefaultImgHeight() const { return default_img_height_; }

			/*! Get the next image with specified parameters.
			 *	@param[in] width	Expected width of the next image.
			 *	@param[in] height	Expected height of the next image.
			 *	@param[in] channels	Expected channels of the next image.
			 *	@param[in] crop		If set to true, crop the image when its width-height rate does not match the specified one.
			 *						Else, explicitly resize it.
			 *	@param[in] flip		Whether to flip the image.
			 *	@param[in] flipmode	A flag to specify how to flip the array.
			 *						0 means flipping around the x-axis.
			 *						Positive value (for example, 1) means flipping around y-axis.
			 *						Negative value (for example, -1) means flipping around both axes.
			 *	@return				The image newly retrieved.
			 */
			cv::Mat GetImage(
				int width,
				int height,
				int channels,
				bool crop = true,
				bool flip = false,
				int flip_mode = 0);

			/*! Get the next image with default parameters.
			 *	@return			The image newly retrieved.
			 */
			virtual const cv::Mat& GetImage() = 0;

			/*! Get the last image retrieved.
			 *	Called only after calling GetImage.
			 *	@return			The last image retrieved.
			 */
			cv::Mat GetLastImg() const { return img_buf_; }

		protected:
			//! The width of the default frame.
			long default_img_width_;
			//! The height of the default frame.
			long default_img_height_;

			//! The result image buffer.
			cv::Mat img_buf_;

			//! Whether the next image is prepared.
			bool img_prepared_;
		};

		/*!	@class CWebCamReader
		 *	@brief Helper for web cameras.
		 *
		 *	This is a helper classes for connecting and retrieving videos from web cameras.
		 */
		class CAMERAREADER_API CWebCamReader : public CCamReader
		{
		public:
			/*! Login to the web camera.
			 *	@return	The last error occurred (0 for no error).
			 */
			virtual long Login(
				_In_ const char* dev_ip,
				unsigned short port,
				_In_ const char* username,
				_In_ const char* passwd);
			/*! Logout from the web camera.
			 *	Remember to call this before deconstruction if logged in.
			 */
			virtual void Logout();

			/*! Constructor of CWebCamReader.
			 *	Initializes basic environment with maximum image size parameters.
			 *	@param[in] max_img_width		The min size of image to be retrieved from the camera.
			 *	@param[in] max_img_height	The max size of image to be retrieved from the camera.
			 */
			CWebCamReader(int max_img_width = 1980, int max_img_height = 1080);
			/*! Deconstructor of CWebCamReader.
			 *	Release basic environment.
			 *	Remember to call Logout() before deconstruction if logged in.
			 */
			virtual ~CWebCamReader();
			
			/*! Get the next image with default parameters.
			 *	@return	The image newly retrieved.
			 */
			const cv::Mat& GetImage();

			/*! Get the error message of the web camera.
			 *	@return			A const pointer to a static string containing the error message.
			 */
			const char* GetLastError();
		private:
			//! Whether this object is connecting an online camera.
			bool online_;

			cv::VideoCapture cap_;

			//! The code of the last error.
			long last_error_;
#ifndef _NO_HKSDK
			//! The size of decode buffer.
			size_t decode_buf_size_;
			//! The decode buffer.
			PBYTE decode_buf_;

			//! Connected port.
			long port_;
			//! User ID.
			long user_id_;
			//! Handle for real play decoding.
			long real_play_handle_;

			//! Counts the number of clients.
			static int g_client_cnt;

			//! Call back function for decoding.
			friend void CALLBACK g_RealDataCallBack_V30(
				long lRealHandle,
				unsigned long dwDataType,
				unsigned char *pBuffer,
				unsigned long dwBufSize,
				unsigned long dwUser);
#endif
		};

		/*!	@class	CCameraNotFoundException
		 *	@brief	Exception for cases that specified camera device is not found.
		 *	This exception is raised when the specified camera device is not found.
		 */
		class CCameraNotFoundException : public std::runtime_error
		{
		public:
			/*!	@brief Construct from message string.
			 *	@param	_Message	Reference to a std::string containing the error message.
			 */
			explicit CCameraNotFoundException(_In_ const std::string& _Message) : std::runtime_error(_Message.c_str()) {}

			/*!	@brief Construct from message string.
			 *	@param	_Message	Const pointer to a C string containing the error message.
			 */
			explicit CCameraNotFoundException(_In_ const char* _Message) : std::runtime_error(_Message) {}
		};

		/*!	@class	CCameraNoInputException
		 *	@brief	Exception for cases that specified camera device is not inputting.
		 *	This exception is raised when the specified camera device is not inputting.
		 */
		class CCameraNoInputException : public std::runtime_error
		{
		public:
			/*!	@brief Construct from message string.
			 *	@param	_Message	Reference to a std::string containing the error message.
			 */
			explicit CCameraNoInputException(_In_ const std::string& _Message) : std::runtime_error(_Message.c_str()) {}

			/*!	@brief Construct from message string.
			 *	@param	_Message	Const pointer to a C string containing the error message.
			 */
			explicit CCameraNoInputException(_In_ const char* _Message) : std::runtime_error(_Message) {}
		};

		/*!	@class CCamCapReader
			 *	@brief Helper for USB cameras.
			 *
			 *	This is a helper classes for connecting and retrieving videos from USB cameras.
			 */
		class CAMERAREADER_API CCamCapReader : public CCamReader
		{
		public:
			/*! Get the next image with default parameters.
			 *	@return	The image newly retrieved.
			 */
			const cv::Mat& GetImage();

			/*! Constructor of CCamCapReader.
			 *	Opens a capture of the camera specified by the given camera code,
			 *	and try to set the resolution of the camera according to the specified max image size.
			 *	@param[in]	usb_camera_device			The code of camera to capture. Availble ones can be obtained from EnumerateCameras(std::vector<int> &).
			 *	@param[in]	max_img_width				Maximum width of images to be captured.
			 *	@param[in]	max_img_height				Maximum height of images to be captured.
			 *	@throws		CCameraNotFoundException	If the specified camera device is not found.
			 */
			CCamCapReader(int usb_camera_device = 0, int max_img_width = 1980, int max_img_height = 1080);
			/*! Deconstructor of CCamCapReader.
				Close the camera capture.
				*/
			virtual ~CCamCapReader();

			/*! List the codes of all available camera devices in the param std::vector.
			 *	@param[in]	cam_idx		std::vector buffer for the camera codes.
			 *	@return				Whether at least one available camera is found.
			 */
			static bool EnumerateCameras(_In_ std::vector<int> &cam_idx);
		private:
			int usb_camera_device_;	//! Device ID of the USB camera.
		};

		/*! Convert the type of the image according to the param channels.
		 *	@param	img				The image to be converted.
		 *	@param	num_channels	The target channel number. 1: Gray-scale; 3: RGB; 4: RGBA.
		 */
		void CAMERAREADER_API Convert(_Inout_ cv::Mat& img, int num_channels);

		/*! Balance the hue and brightness of the image.
		 *	@param	img			The image to be balanced.
		 *	@param	for_global	If set as true, the image would be first balanced according to global color distribution.
		 *	@param	for_face	If set as true, the image would be assumed to be a face image, then balanced specially for faces.
		 */
		void CAMERAREADER_API Balance(_Inout_ cv::Mat& img, bool for_global = true, bool for_face = true);
		
		/*! Used for simulating a web camera reader with a USB camera reader when you don't have a web camera.
		 */
		class CAMERAREADER_API CFakeWebCamReader : public CWebCamReader
		{
		public:
			/*! Do nothing.
			 *	@return	The last error occurred (0 for no error).
			 */
			inline long Login(
				_In_ const char* dev_ip,
				unsigned short port,
				_In_ const char* username,
				_In_ const char* passwd)
			{
				return 0;
			}
			/*! Do nothing.
			 */
			inline void Logout() {}

			/*! Constructor of CFakeWebCamReader.
			 */
			inline CFakeWebCamReader(int usb_camera_device = 0, int max_img_width = 1980, int max_img_height = 1080)
				: CWebCamReader(max_img_width, max_img_height), agent_(usb_camera_device, max_img_width, max_img_height)
			{}
			/*! Deconstructor of CFakeWebCamReader.
			 */
			inline virtual ~CFakeWebCamReader() {}

			/*! Get the next image (actually from the USB camera) with default parameters.
			 *	@return	The image newly retrieved.
			 */
			inline const cv::Mat& GetImage() { return agent_.GetImage(); }
		private:
			CCamCapReader agent_;
		};
	}
}