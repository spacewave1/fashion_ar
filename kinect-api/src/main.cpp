#include "glut.h"
#include "kinect_websocket.h"
#include "kinect_processor.h"
#include "json_file_io.h"
#include <thread>
#include <fstream>

void runWebserver() {
    kinect_websocket kinectWebsocket;
    std::string *json = getJson();
    kinectWebsocket.setJson(json);
    kinectWebsocket.run();
}

void runFileWrite() {
    json_file_io jsonFileIo;
    std::string *json = getJson();
    jsonFileIo.setJson(json);
    jsonFileIo.setWritePath("../../test.json");
    jsonFileIo.run();
}


int main(int argc, char *argv[]) {
    //if (!initHead(argc, argv)) return 1;
    if (!init(argc, argv)) return 1;

    bool writeToCSVFile = false;
    if(writeToCSVFile){
        std::ofstream myfile;
        myfile.open("../kinect_t_pose.csv");
        myfile
                << "time, hip-centerX,Y,Z,W,spineX,Y,Z,W,shoulder-centerX,Y,Z,W,headX,Y,Z,W,"
                << "shoulder-leftX,Y,Z,W,elbow-leftX,Y,Z,W,wrist-leftX,Y,Z,W,hand-leftX,Y,Z,W,"
                << "shoulder-rightX,Y,Z,W,elbow-rightX,Y,Z,W,wrist-rightX,Y,Z,W,hand-rightX,Y,Z,W,"
                << "hip-leftX,Y,Z,W,knee-leftX,Y,Z,W,ankle-leftX,Y,Z,W,foot-leftX,Y,Z,W,hip-rightX,Y,Z,W,"
                << "knee-rightX,Y,Z,W,ankle-rightX,Y,Z,W,foot-rightX,Y,Z,W\n";
        myfile.close();
    }



    //if (!initKinectFaceTracking()) return 1;
    if (!initKinectSkeletonTracking()) return 1;

    thread websocketThread(runWebserver);
    //thread fileIOThread(runFileWrite);

    initGL();

    // Main loop
    execute();
    return 0;
}