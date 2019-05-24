#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/tracking.hpp>

#include <iostream>
#include <fstream>
#include <dirent.h>
#include <thread>
#include <map>
#include <vector>
#include <string>
#include <regex>
#include <algorithm>

#include "resources/JsonBuilder.h"

using namespace std;
using namespace cv;

// Directory to save JSON config video_files to.
#define JSON_DIR "static/video-info/"
#define VIDEO_DIR "static/videos/"

// Uncomment this to see the tracking at work.
//#define TRACKING

vector<string> GetVideosFromDir(string, string);
void ProcessVideo(string);
vector<string> SplitString(string&, const char*);
std::map<std::string, std::string> GetGeoURIValues(std::string&);

int main()
{
    useOptimized();
    setUseOptimized(true);

    // Keep the application running to continuously check for more files
    while(true)
    {
        auto json_files = GetVideosFromDir(JSON_DIR, ".json");
        auto video_files = GetVideosFromDir(VIDEO_DIR, ".mp4");

        for(int i = 0; i < json_files.size(); i++)
        {
            string jf = json_files[i].substr(json_files[i].find("DE_")+3, json_files[i].length());
            jf = jf.substr(0, jf.find_last_of("."));

            for(auto it = video_files.begin(); it != video_files.end(); ++it)
                if((*it).find(jf) != string::npos)
                {
                    cout << "\"" << (*it) << "\" has already been processed!"<<endl;
                    video_files.erase(it);
                    break;
                }
        }

        vector<thread> threads;
        threads.resize(video_files.size());
        for(int i = 0; i < video_files.size(); i++)
            threads[i] = thread(ProcessVideo, video_files[i]);

        for(int i = 0; i < threads.size(); i++)
            threads[i].join();
    }
    
    return 0;
}

vector<string> GetVideosFromDir(string dir, string filter)
{
    vector<string> video_files;
    DIR *dp;
	struct dirent *d;
	if ((dp = opendir(dir.c_str())) != NULL)
	{
		while ((d = readdir(dp)) != NULL)
			if ((strcmp(string(d->d_name).c_str(), ".") != false &&
                strcmp(string(d->d_name).c_str(), "..") != false) &&
                string(d->d_name).find(filter) != string::npos)
				video_files.push_back(dir + d->d_name);
		closedir(dp);
	}
    return video_files;
}

void ProcessVideo(string file)
{
    string vidFileName = file;
    // Open video.
    VideoCapture cap(vidFileName);
    if (!cap.isOpened()) {
        cerr << "*** Could not open video file: \"" << vidFileName << "\"" << endl;
        return;
    }

    // Clean up the video file name.
    {
        // Now that we've opened the file, scrub it clean of directory names to get the tag name.
        vidFileName = vidFileName.substr(vidFileName.find_last_of("/")+1, vidFileName.length());
        
        // Strip the uneeded V_ from the video.
        if(vidFileName.find("V_") != string::npos)
            vidFileName = vidFileName.substr(vidFileName.find_last_of("V_")+1, vidFileName.length());
    }

    // Get the total number of frames in the video.
    auto totalFrames = cap.get(cv::CAP_PROP_FRAME_COUNT);
    cout << "=== Processing \"" << vidFileName << "\" ===" << endl;
    cout << "  > Total Frames: " << totalFrames << endl;

    QRCodeDetector qrDetector;

    // Create the JSON object which will hold all of the detected events.
    JSON DE("DetectedEvents");

    #ifdef TRACKING
    // ** Object detection variable setup
    Mat mask;
    Ptr<BackgroundSubtractor> bkgd_sub_ptr = createBackgroundSubtractorKNN();
    SimpleBlobDetector::Params params;
    Ptr<SimpleBlobDetector> detector = SimpleBlobDetector::create(params);
    // ** End Object detection setup
    #endif

    int currentFrame = 0;
    while (true) 
    {
        // Read in the first frame of the video.
        Mat frame;
        cap.read(frame);

        // If the frame is empty, we simply skip this one and move on.
        // TODO: GoPro videos seem to have lots of these due to large amounts of meta-data. This might be the only workaround for now
        if (frame.empty()) continue;

        currentFrame++;
        if (currentFrame >= totalFrames) break;

        // Detect QR code and read data into JSON object.
        {
            Mat boundBox;
            String url = qrDetector.detectAndDecode(frame, boundBox);
            if (url.length() > 0) 
            {
                // This exists to ensure that there is only one QR code,
                // however in future it might be useful to have more.
                auto tempVec = DE.GetSubobjectNames();
                if(find(tempVec.begin(), tempVec.end(), "Event_QRCode") == tempVec.end())
                {
                    map<string, string> info = GetGeoURIValues(url);
                    info.insert(make_pair("frame", to_string(currentFrame)));
                    JSON QREvent = JSON("Event_QRCode", info);
                    DE.AddObject(QREvent);
                }
            }
        }

        #ifdef TRACKING
        // Masking to find contours of moving objects.
        {
            bkgd_sub_ptr->apply(frame, mask);
            vector<KeyPoint> keypoints;
            detector->detect(frame, keypoints);

            Mat im_with_keypoints;
            drawKeypoints(frame, keypoints, im_with_keypoints, Scalar(0,0,255), DrawMatchesFlags::DRAW_RICH_KEYPOINTS );
            
            
            /// Find contours   
            vector<vector<Point> > contours;
            vector<Vec4i> hierarchy;
            RNG rng(12345);
            findContours( mask, contours, hierarchy, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE, Point(0, 0) );
            /// Draw contours
            Mat drawing = Mat::zeros( mask.size(), CV_8UC3 );
            for( int i = 0; i< contours.size(); i++ )
            {
                Scalar color = Scalar( rng.uniform(0, 255), rng.uniform(0,255), rng.uniform(0,255) );
                drawContours( drawing, contours, i, color, 2, 8, hierarchy, 0, Point() );
            }     
            

            imshow("Detection", im_with_keypoints);
            
        }
        // Related to showing the frame image.
        char c=(char)waitKey(25);
        if(c==27)
            break; 
        #endif
    }

    cout << "=== Finished processing \"" << vidFileName << "\" ===" << endl;
    
    cap.release();

    destroyAllWindows();

    // Construct the JSON object array of all events detected.
    DE.BuildJSONObjectArray();

    // Create the JSON file for this video.
    ofstream configFile;
    configFile.open (JSON_DIR "DE_" + vidFileName.substr(0, vidFileName.find_last_of(".")) + ".json");
    configFile << DE.GetJSON();
    configFile.close();
}

vector<string> SplitString(string& str, const char* delimiter)
{
    vector<string> result;
    size_t i = 0;
    string temp = str;

    regex r("\\s+");
    while ((i = temp.find(delimiter)) != string::npos)
    {
        if (i > str.length())
            i = str.length() - 1;
            result.push_back(regex_replace(temp.substr(0, i), r, ""));
            temp.erase(0, i + 1);
    }
    result.push_back(regex_replace(temp.substr(0, i), r, ""));

    return result;
}

std::map<std::string, std::string> GetGeoURIValues(std::string& uri)
{
    std::map<std::string, std::string> json;
    
    auto strings = SplitString(uri, ";");
    for(auto str : strings)
    {
        if(str.find("geo:") != std::string::npos)
        {
            str = str.substr(str.find("geo:")+4, str.length());
            auto v = SplitString(str, ",");
            
            json.insert(std::make_pair("lat", v[0]));
            json.insert(std::make_pair("long", v[1]));
        }
        auto v = SplitString(str, "=");
        for(int i = 0; i < v.size()-1; i+=2)
            json.insert(std::make_pair(v[i], v[i+1]));
    }
    
    return json;
}
