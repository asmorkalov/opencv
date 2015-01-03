#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace cv;
const char* message = "Hello Android!";

// You shound not call it. It's added to fix linkage problem with CUDA 7.0 aarch64

extern "C"
{

    void nppiDCTQuantFwd8x8LS_JPEG_8u16s_C1R_NEW_SM30()
    {}

    void nppiDCTQuantInv8x8LS_JPEG_16s8u_C1R_NEW_SM30()
    {}

}

int main(int argc, char* argv[])
{
  (void)argc; (void)argv;
  // print message to console
  printf("%s\n", message);

  // put message to simple image
  Size textsize = getTextSize(message, CV_FONT_HERSHEY_COMPLEX, 3, 5, 0);
  Mat img(textsize.height + 20, textsize.width + 20, CV_32FC1, Scalar(230,230,230));
  putText(img, message, Point(10, img.rows - 10), CV_FONT_HERSHEY_COMPLEX, 3, Scalar(0, 0, 0), 5);

  // save\show resulting image
#if ANDROID
  imwrite("/mnt/sdcard/HelloAndroid.png", img);
#else
  imshow("test", img);
  waitKey();
#endif
  return 0;
}
