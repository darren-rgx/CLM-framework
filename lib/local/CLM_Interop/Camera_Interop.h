// Camera_Interop.h

#pragma once

#pragma unmanaged

// Include all the unmanaged things we need.

#include <opencv2/core/core.hpp>
#include "opencv2/objdetect.hpp"
#include "opencv2/calib3d.hpp"
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

// For camera listings
#include "comet_auto_mf.h"
#include "camera_helper.h"

#pragma managed

#include <msclr\marshal.h>
#include <msclr\marshal_cppstd.h>

using namespace System;
using namespace OpenCVWrappers;
using namespace System::Collections::Generic;

using namespace msclr::interop;

namespace Camera_Interop {

	public ref class CaptureFailedException : System::Exception 
	{
        public:
        
		CaptureFailedException(System::String^ message): Exception(message){}	
	};
	
	public ref class Capture
	{
	private:

		// OpenCV based video capture for reading from files
		VideoCapture* vc;

		RawImage^ latestFrame;
		RawImage^ grayFrame;

		double fps;

		bool is_webcam;
		bool is_image_seq;

		int  frame_num;
		vector<string>* image_files;

		int vid_length;

	public:

		int width, height;

		Capture(int device, int width, int height)
		{
			assert(device >= 0);

			latestFrame = gcnew RawImage();

			vc = new VideoCapture(device);
			vc->set(CV_CAP_PROP_FRAME_WIDTH, width);
			vc->set(CV_CAP_PROP_FRAME_HEIGHT, height);

			is_webcam = true;
			is_image_seq = false;

			this->width = width;
			this->height = height;

			vid_length = 0;
			frame_num = 0;

			int set_width = vc->get(CV_CAP_PROP_FRAME_WIDTH);
			int set_height = vc->get(CV_CAP_PROP_FRAME_HEIGHT);

			if(!vc->isOpened())
			{
				throw gcnew CaptureFailedException("Failed to open the webcam");
			}
			if(set_width != width || set_height != height)
			{
				throw gcnew CaptureFailedException("Failed to open the webcam with desired resolution");
			}
		}

		Capture(System::String^ videoFile)
		{
			latestFrame = gcnew RawImage();

			vc = new VideoCapture(marshal_as<std::string>(videoFile));
			fps = vc->get(CV_CAP_PROP_FPS);
			is_webcam = false;
			is_image_seq = false;
			this->width = vc->get(CV_CAP_PROP_FRAME_WIDTH);
			this->height = vc->get(CV_CAP_PROP_FRAME_HEIGHT);

			vid_length = vc->get(CV_CAP_PROP_FRAME_COUNT);
			frame_num = 0;

			if(!vc->isOpened())
			{
				throw gcnew CaptureFailedException("Failed to open the video file");
			}
		}

		// An alternative to using video files is using image sequences
		Capture(List<System::String^>^ image_files)
		{
			
			latestFrame = gcnew RawImage();

			is_webcam = false;
			is_image_seq = true;
			this->image_files = new vector<string>();

			for(int i = 0; i < image_files->Count; ++i)
			{
				this->image_files->push_back(marshal_as<std::string>(image_files[i]));
			}
			vid_length = image_files->Count;
		}

		static List<Tuple<System::String^, List<Tuple<int,int>^>^, RawImage^>^>^ GetCameras()
		{

			auto managed_camera_list = gcnew List<Tuple<System::String^, List<Tuple<int,int>^>^, RawImage^>^>();

			// Using DirectShow for capturing from webcams (for MJPG as has issues with other formats)
		    comet::auto_mf auto_mf;

			std::vector<camera> cameras = camera_helper::get_all_cameras();
			
			for (size_t i = 0; i < cameras.size(); ++i)
			{
				cameras[i].activate();
				
				std::string name = cameras[i].name(); 

				// List camera media types
				auto media_types = cameras[i].media_types();

				auto resolutions = gcnew List<Tuple<int,int>^>();

				set<pair<pair<int, int>, media_type>> res_set_mjpg;
				set<pair<pair<int, int>, media_type>> res_set_rgb;

				Mat sample_img;
				RawImage^ sample_img_managed = gcnew RawImage();

				for (size_t m = 0; m < media_types.size(); ++m)
				{
					auto media_type_curr = media_types[m];		
					if(media_type_curr.format() == MediaFormat::MJPG)
					{
						res_set_mjpg.insert(pair<pair<int, int>, media_type>(pair<int,int>(media_type_curr.resolution().width, media_type_curr.resolution().height), media_type_curr));
					}
					else if(media_type_curr.format() == MediaFormat::RGB24)
					{
						res_set_rgb.insert(pair<pair<int, int>, media_type>(pair<int,int>(media_type_curr.resolution().width, media_type_curr.resolution().height), media_type_curr));
					}
				}
				
				bool found = false;

				for (auto beg = res_set_mjpg.begin(); beg != res_set_mjpg.end(); ++beg)
				{
					auto resolution = gcnew Tuple<int, int>(beg->first.first, beg->first.second);
					resolutions->Add(resolution);

					if((resolution->Item1 >= 640) && (resolution->Item2 >= 480) && !found)
					{
						found = true;
						cameras[i].set_media_type(beg->second);
						
						// read several images (to avoid overexposure)						
						for (int k = 0; k < 5; ++k)
							cameras[i].read_frame();

						// Flip horizontally
						cv::flip(cameras[i].read_frame(), sample_img, 1);
					}
				}

				// If we didn't find any MJPG resolutions revert to RGB24
				if(resolutions->Count == 0)
				{
					for (auto beg = res_set_rgb.begin(); beg != res_set_rgb.end(); ++beg)
					{
						auto resolution = gcnew Tuple<int, int>(beg->first.first, beg->first.second);
						resolutions->Add(resolution);

						if((resolution->Item1 >= 640) && (resolution->Item2 >= 480) && !found)
						{
							found = true;
							VideoCapture cap1(i);
							cap1.set(CV_CAP_PROP_FRAME_WIDTH, resolution->Item1);
							cap1.set(CV_CAP_PROP_FRAME_HEIGHT, resolution->Item2);

							for (int k = 0; k < 5; ++k)
								cap1.read(sample_img);

							// Flip horizontally
						cv::flip(sample_img, sample_img, 1);

						}
					}
				}
				sample_img.copyTo(sample_img_managed->Mat);					


				managed_camera_list->Add(gcnew Tuple<System::String^, List<Tuple<int,int>^>^, RawImage^>(gcnew System::String(name.c_str()), resolutions, sample_img_managed));
			}
			return managed_camera_list;
		}

		RawImage^ GetNextFrame(bool mirror)
		{
			frame_num++;

			if(vc != nullptr)
			{
				
				bool success = vc->read(latestFrame->Mat);

				if (!success)
				{
					// Indicate lack of success by returning an empty image
					Mat empty_mat = cv::Mat();
					empty_mat.copyTo(latestFrame->Mat);
					return latestFrame;
				}
			}
			else if(is_image_seq)
			{
				if(image_files->empty())
				{
					// Indicate lack of success by returning an empty image
					Mat empty_mat = cv::Mat();
					empty_mat.copyTo(latestFrame->Mat);
					return latestFrame;
				}

				Mat img = imread(image_files->at(0), -1);
				img.copyTo(latestFrame->Mat);
				// Remove the first frame
				image_files->erase(image_files->begin(), image_files->begin() + 1);
			}
			
			if (grayFrame == nullptr) {
				if (latestFrame->Width > 0) {
					grayFrame = gcnew RawImage(latestFrame->Width, latestFrame->Height, CV_8UC1);
				}
			}

			if(mirror)
			{
				flip(latestFrame->Mat, latestFrame->Mat, 1);
			}


			if (grayFrame != nullptr) {
				cvtColor(latestFrame->Mat, grayFrame->Mat, CV_BGR2GRAY);
			}

			return latestFrame;
		}

		double GetProgress()
		{
			if(vc != nullptr && is_webcam)
			{
				return - 1.0;
			}
			else
			{
				return (double)frame_num / (double)vid_length;
			}
		}

		bool isOpened()
		{
			if(vc != nullptr)
				return vc->isOpened();
			else
			{
				if(is_image_seq && image_files->size() > 0)
					return true;
				else
					return false;
			}
		}

		RawImage^ GetCurrentFrameGray() {
			return grayFrame;
		}

		double GetFPS() {
			return fps;
		}
		
		// Finalizer. Definitely called before Garbage Collection,
		// but not automatically called on explicit Dispose().
		// May be called multiple times.
		!Capture()
		{
			delete vc; // Automatically closes capture object before freeing memory.			
			delete image_files;
		}

		// Destructor. Called on explicit Dispose() only.
		~Capture()
		{
			this->!Capture();
		}
	};

}
