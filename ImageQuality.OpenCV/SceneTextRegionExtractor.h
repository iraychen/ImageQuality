#pragma once

#include "Region.h"

using namespace System;
using namespace System::Collections::Generic;
using namespace System::IO;
using namespace std;
using namespace cv;

namespace ImageQuality {

	public ref class SceneTextRegionExtractor
	{
	public:
		IList<Region^>^ GetRegions(array<byte>^ buffer, Stream^ ocrImgStream, Stream^ regionStream);
		IList<array<byte>^>^ SimpleWatermark(array<byte>^ buffer);

	private:
		Mat DetectAndRotate(Mat img, Scalar lower, Scalar upper);
		void WriteToStream(const string& extension, Mat image, Stream^ outStream);
		Mat ReadImage(array<byte>^ buffer);
	};
}