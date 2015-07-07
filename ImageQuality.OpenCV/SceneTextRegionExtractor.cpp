#include "Stdafx.h"
#include "SceneTextRegionExtractor.h"

using namespace std;
using namespace cv;

namespace ImageQuality
{
	IList<array<byte>^>^ SceneTextRegionExtractor::SimpleWatermark(array<byte>^ buffer)
	{
		Mat img = ReadImage(buffer);

		List<array<byte>^>^ images = gcnew List<array<byte>^>(4);

		Mat red = DetectAndRotate(img, Scalar(0, 0, 200), Scalar(50, 50, 255));
		images->Add(ToByteArray(red, ".tiff"));

		Mat yellow = DetectAndRotate(img, Scalar(0, 225, 225), Scalar(50, 255, 255));
		images->Add(ToByteArray(yellow, ".tiff"));

		Mat black = DetectAndRotate(img, Scalar(0, 0, 0), Scalar(25, 25, 25));
		images->Add(ToByteArray(black, ".tiff"));

		Mat white = DetectAndRotate(img, Scalar(200, 200, 200), Scalar(255, 255, 255));
		images->Add(ToByteArray(white, ".tiff"));

		return images;
	}

	Mat SceneTextRegionExtractor::DetectAndRotate(Mat img, Scalar lower, Scalar upper)
	{
		Mat output;
		inRange(img, lower, upper, output);

		Mat scaled;
		pyrDown(output, scaled);

		Mat morphKernel = getStructuringElement(MORPH_RECT, Size(5, 1));
		morphologyEx(scaled, scaled, MORPH_CLOSE, morphKernel);

		vector<vector<Point>> contours;
		vector<Vec4i> hierarchy;
		findContours(scaled, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));

		if (hierarchy.empty())
			return output;

		float biggest = 0, angle;
		for (int idx = 0; idx >= 0; idx = hierarchy[idx][0])
		{
			RotatedRect box = minAreaRect(contours[idx]);

			float area = box.size.area();
			if (area > biggest && box.size.width > 20 && box.size.height > 12)
			{
				biggest = area;
				angle = box.angle;
			}
		}

		if (angle < -45)
			angle += 90;

		int len = max(output.cols, output.rows);
		Point2f pt(len / 2., len / 2.);
		Mat r = getRotationMatrix2D(pt, angle, 1);
		warpAffine(output, output, r, Size(len, len));

		return output;
	}

	IList<Region^>^ SceneTextRegionExtractor::GetRegions(array<byte>^ buffer, Stream^ ocrImgStream, Stream^ regionStream)
	{
		List<Region^>^ list = gcnew List<Region^>(5);
		Mat image = ReadImage(buffer);

		if (ocrImgStream != nullptr)
		{
			Mat tiff;
			cvtColor(image, tiff, CV_BGR2GRAY);
			threshold(tiff, tiff, 0, 255, THRESH_BINARY | THRESH_OTSU);

			//threshold(tiff, tiff, 190, 255, THRESH_BINARY_INV); //todo

			WriteToStream(".tiff", tiff, ocrImgStream);
		}

		Mat gray;
		cvtColor(image, gray, CV_BGR2GRAY);

		Mat scaled = gray;
		pyrDown(scaled, scaled);
		pyrUp(scaled, scaled);

		Mat grad;
		Mat morphKernel = getStructuringElement(MORPH_ELLIPSE, Size(3, 3));
		morphologyEx(scaled, grad, MORPH_GRADIENT, morphKernel);

		Mat bw;
		threshold(grad, bw, 32, 255, THRESH_BINARY | THRESH_OTSU);

		Mat connected;
		morphKernel = getStructuringElement(MORPH_RECT, Size(5, 1));
		morphologyEx(bw, connected, MORPH_CLOSE, morphKernel);

		Mat mask = Mat::zeros(bw.size(), CV_8UC1);
		vector<vector<Point>> contours;
		vector<Vec4i> hierarchy;
		findContours(connected, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));

		if (!hierarchy.empty())
		{
			vector<Rect> regionRects;
			for (int idx = 0; idx >= 0; idx = hierarchy[idx][0])
			{
				Rect rect = boundingRect(contours[idx]);
				if (rect.height > 400)
					continue;

				Mat maskROI(mask, rect);
				maskROI = Scalar(0, 0, 0);

				drawContours(mask, contours, idx, Scalar(255, 255, 255), CV_FILLED);
				double r = (double)countNonZero(maskROI) / (rect.width*rect.height);

				if (r > .45 && rect.width > 20 && rect.height > 12)
				{
					regionRects.push_back(rect);
				}
			}

			if (!regionRects.empty())
			{
				Mat rectMask(Size(gray.cols, gray.rows), gray.type(), Scalar(0, 0, 0));
				for each (Rect rect in regionRects)
				{
					Rect expanded(rect.x - 5, rect.y, rect.width + 10, rect.height);
					rectangle(rectMask, expanded, Scalar(255, 255, 255), 2);
				}

				findContours(rectMask, contours, hierarchy, CV_RETR_CCOMP, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));

				vector<Rect> mergedRects;
				for (int idx = 0; idx >= 0; idx = hierarchy[idx][0])
				{
					Rect rect = boundingRect(contours[idx]);
					mergedRects.push_back(rect);
				}

				if (!mergedRects.empty())
				{
					for each (Rect rect in mergedRects)
					{
						rectangle(image, rect, Scalar(0, 255, 0), 2);

						Region^ region = gcnew Region(rect.x, rect.y, rect.width, rect.height);
						list->Add(region);
					}
				}
			}
		}

		if (regionStream != nullptr)
		{
			WriteToStream(".jpg", image, regionStream);
		}

		return list;
	}

	Mat SceneTextRegionExtractor::ReadImage(array<byte>^ buffer)
	{
		pin_ptr<byte> px = &buffer[0];
		Mat datax(1, buffer->Length, CV_8U, (void*)px, CV_AUTO_STEP);
		return imdecode(datax, CV_LOAD_IMAGE_COLOR);
	}

	array<byte>^ SceneTextRegionExtractor::ToByteArray(Mat image, const std::string& extension)
	{
		vector<byte> buffer;
		imencode(extension, image, buffer);

		array<byte>^ bytes = gcnew array<byte>(buffer.size());

		UnmanagedMemoryStream^ sourceStream = gcnew UnmanagedMemoryStream(buffer.data(), buffer.size());
		sourceStream->Read(bytes, 0, buffer.size());

		return bytes;
	}

	void SceneTextRegionExtractor::WriteToStream(const std::string& extension, Mat image, Stream^ stream)
	{
		vector<byte> buffer;
		imencode(extension, image, buffer);

		UnmanagedMemoryStream^ sourceStream = gcnew UnmanagedMemoryStream(buffer.data(), buffer.size());
		sourceStream->CopyTo(stream);
	}
}