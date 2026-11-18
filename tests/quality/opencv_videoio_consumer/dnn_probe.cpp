#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>

#include <iostream>
#include <sstream>
#include <string>

int main() {
    const std::string information = cv::getBuildInformation();
    const auto ipp = information.find("Intel IPP:");
    if (ipp == std::string::npos) return 1;
    std::istringstream field(information.substr(ipp + std::string("Intel IPP:").size()));
    std::string enabled;
    if (!(field >> enabled) || enabled != "NO") return 1;
    const cv::Mat image(2, 2, CV_8UC3, cv::Scalar(4, 5, 6));
    const auto blob = cv::dnn::blobFromImage(image);
    if (blob.empty() || blob.dims != 4 || blob.type() != CV_32F ||
        blob.size[0] != 1 || blob.size[1] != 3) return 1;
    std::cout << "OpenCV DNN CPU blob consumer: PASS\n";
    return 0;
}
