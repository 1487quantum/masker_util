#include "masker_util/masker_util.hpp"

/*
 * A ROS package for image masking & cropping utility for fisheye / wide-angle / omnidirectional image.
 * masker_util.cpp
 *
 *  __ _  _   ___ ______ ____                    _                   
 * /_ | || | / _ \____  / __ \                  | |                  
 *  | | || || (_) |  / / |  | |_   _  __ _ _ __ | |_ _   _ _ __ ___  
 *  | |__   _> _ <  / /| |  | | | | |/ _` | '_ \| __| | | | '_ ` _ \ 
 *  | |  | || (_) |/ / | |__| | |_| | (_| | | | | |_| |_| | | | | | |
 *  |_|  |_| \___//_/   \___\_\\__,_|\__,_|_| |_|\__|\__,_|_| |_| |_|
 *
 * Copyright (C) 2020 1487Quantum
 * 
 * 
 * Licensed under the MIT License.
 * 
 */

masker_util::masker_util(const ros::NodeHandle& nh_)
{
    this->nh = nh_;
};

bool masker_util::init()
{
    // Pub-Sub
    image_transport::ImageTransport it(this->nh);
    imgSub = it.subscribe("img_in", 1, &masker_util::imgCallback, this); //Sub
    imgPub = it.advertise("img_out", 1);

    return true;
}

// === CALLBACK & PUBLISHER ===
void masker_util::imgCallback(const sensor_msgs::ImageConstPtr& imgp)
{
    try {
        cv_bridge::CvImagePtr imagePtrRaw{ cv_bridge::toCvCopy(imgp, sensor_msgs::image_encodings::BGR16) };
        cv::Mat frame{ imagePtrRaw->image };
        cv::Size f_size(frame.cols, frame.rows);

        bool setMask{ true };
        //Circle mask
        int cirRad{ 720 };
        cv::Scalar circleColor(255, 0, 0);
        cv::Point cOffset(-40, -20); //Circle offset from img center: x < 0 is left, y < 0 is up]
        cv::Point cPoint(f_size.width / 2 + cOffset.x, f_size.height / 2 + cOffset.y);

        //Draw Crop region
        constexpr int cropBorder{ 50 };
        cv::Rect cropFrame(frame.cols / 2 - (cirRad + cropBorder + (frame.cols / 2 - cPoint.x)), 0, 2 * (cirRad + cropBorder), frame.rows);

        if (setMask) {
            constexpr int markLen{ 40 };
            circle(frame, cPoint, cirRad, circleColor, 2, cv::LINE_AA); //Add circle
            drawMarker(frame, cPoint, circleColor, cv::MARKER_CROSS, markLen, 2, cv::LINE_AA); //Circle center
            drawMarker(frame, cv::Point(cPoint.x - cOffset.x, cPoint.y - cOffset.y), cv::Scalar(0, 255, 0), cv::MARKER_TILTED_CROSS, markLen / 2, 2, cv::LINE_AA); //Add image centre cross
            rectangle(frame, cropFrame, cv::Scalar(255, 0, 0)); //Crop region
        }

        if (!setMask) {
            //Mask - Remove details outside circle
            cv::Mat pRoi(cv::Mat::zeros(frame.size(), CV_8UC1));
            cv::Mat mask = cv::Mat::zeros(frame.size(), CV_8UC1);
            circle(mask, cPoint, cirRad, cv::Scalar(255), -1, cv::LINE_AA);
            frame.copyTo(pRoi, mask);
            frame = pRoi(cropFrame); //Crop image
            pRoi.release();
        }
        imgPub.publish(cv_bridge::CvImage(std_msgs::Header(), "bgr16", frame).toImageMsg());
        frame.release();
    }
    catch (cv_bridge::Exception& e) {
        ROS_ERROR("Could not convert from '%s' to 'bgr16'.", imgp->encoding.c_str());
    }
}

int main(int argc, char** argv)
{
    ros::init(argc, argv, "masker_util");
    ros::NodeHandle nh;

    masker_util cimg(nh);
    if (!cimg.init()) {
        ROS_WARN("Exiting...");
        ros::shutdown();
        return -1;
    }
    ros::spin();

    return 0;
}

