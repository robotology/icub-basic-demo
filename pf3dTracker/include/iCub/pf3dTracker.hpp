/**
* Copyright: (C) 2009 RobotCub Consortium
* Authors: Matteo Taiana
* CopyPolicy: Released under the terms of the GNU GPL v2.0.
*/

//good combinations:
// _nParticles | stDev | inside_outside | nPixels
//	800       100	       1.5	    50
//with 800 particles and 30fps the 100% of the processor of CORTEX1 is used.
//could try to halve the number of pixels used for the circles.

#ifndef _PF3DTRACKER_
#define _PF3DTRACKER_

#include <iostream>
#include <sstream>
#include <string>

#include <yarp/os/BufferedPort.h>
#include <yarp/os/LogStream.h>
#include <yarp/os/Network.h>
#include <yarp/os/RFModule.h>
#include <yarp/os/Stamp.h>
#include <yarp/os/Time.h>
#include <yarp/sig/Image.h>
#include <yarp/sig/ImageDraw.h>
#include <yarp/sig/ImageFile.h>
#include <yarp/sig/Vector.h>

#ifdef _CH_
#pragma package <opencv>
#endif
#ifndef _EiC
#include <opencv2/opencv.hpp>
#include <opencv2/highgui/highgui.hpp>
#endif

#include <iCub/pf3dTrackerSupport.hpp>

//for tracking in the iCub: 1000 particles and an stDev of 80 work well with slow movements of the ball. the localization is quite stable. the shape model has a 20% difference wrt the real radius.
//#define _nParticles 5000
//#define stDev 100 80
//150 for the demo.
//#define stDev 150 

//#define NEWPOLICY 0
//1: only transform the color of the pixels that you need
//0: transform the whole image.

#define nPixels 50

#define YBins 4
#define UBins 8 
#define VBins 8

//should be 1.5 or 1.0 !!! ???
//#define inside_outside_difference_weight 1.5
//if I set it to 1.5, when the ball goes towards the camera, the tracker lags behind.
//this happens because the particles with "ball color" on both sides of the contour
//are still quite likely. the opposite is not true: when the ball goes away from the
// camera, the tracker follows it quite readily.

class PF3DTracker : public yarp::os::RFModule
{

private:

int _numParticlesReceived;

//parameters set during initialization.
yarp::os::ConstString _inputVideoPortName;
yarp::os::BufferedPort<yarp::sig::ImageOf<yarp::sig::PixelRgb> > _inputVideoPort;
yarp::os::ConstString _outputVideoPortName;
yarp::os::BufferedPort<yarp::sig::ImageOf<yarp::sig::PixelRgb> > _outputVideoPort;
yarp::os::ConstString _outputDataPortName;
yarp::os::BufferedPort<yarp::os::Bottle> _outputDataPort;
yarp::os::ConstString _inputParticlePortName;
yarp::os::BufferedPort<yarp::os::Bottle> _inputParticlePort;
yarp::os::ConstString _outputParticlePortName;
yarp::os::BufferedPort<yarp::os::Bottle> _outputParticlePort;
yarp::os::ConstString _outputAttentionPortName;
yarp::os::BufferedPort<yarp::sig::Vector> _outputAttentionPort;
yarp::os::ConstString _outputUVDataPortName;
yarp::os::BufferedPort<yarp::os::Bottle> _outputUVDataPort;
bool supplyUVdata;

std::string _projectionModel;
float _perspectiveFx;
float _perspectiveFy;
float _perspectiveCx;
float _perspectiveCy;
int _calibrationImageWidth;
int _calibrationImageHeight;
float _likelihoodThreshold;

int _seeingObject; //0 means false, 1 means true.
int _circleVisualizationMode;
std::string _initializationMethod;
std::string _trackedObjectType;
bool _saveImagesWithOpencv;
std::string _saveImagesWithOpencvDir;
double _initialX;
double _initialY;
double _initialZ;
double _attentionOutput;
double _attentionOutputMax;
double _attentionOutputDecrease;
CvRNG rngState; //something needed by the random number generator
bool _doneInitializing;

Lut*   _lut;
CvMat* _A;
int _nParticles;
float _accelStDev;
float _inside_outside_difference_weight;
int _colorTransfPolicy;

//float _modelHistogram[YBins][UBins][VBins]; //data
CvMatND* _modelHistogramMat; //OpenCV Matrix
CvMatND* _innerHistogramMat;//[YBins][UBins][VBins];
CvMatND* _outerHistogramMat;//[YBins][UBins][VBins];

CvMat* _model3dPointsMat; //shape model
CvMat* _visualization3dPointsMat; //visualization model for the sphere (when _circleVisualizationMode==1). should have less points, but it was easier to make it like this.

//not sure which of these are really used.
CvMat* _rzMat;      //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _ryMat;      //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _points2Mat; //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _p2Mat1;     //pointer to the first row of _points2Mat
CvMat* _p2Mat3;     //pointer to the third row of _points2Mat
CvMat* _tempMat;    //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _tempMat1;   //pointer to the first row of _tempMat
CvMat* _tempMat2;   //pointer to the first row of _tempMat
CvMat* _tempMat3;   //pointer to the first row of _tempMat
CvMat* _drawingMat; //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _projectionMat; //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _xyzMat1; //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _xyzMat2; //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _xyzMat3; //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _particles1; //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _particles2; //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _particles3; //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _particles4; //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _particles5; //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _particles6; //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _particles7; //this is used during some operations... it is defined here to avoid repeated instantiations
CvMat* _particles1to6;
CvMat* _newParticles1to6;
CvMat* _uv;

//resampling-related stuff
CvMat* _nChildren;
CvMat* _label;
CvMat* _u;
CvMat* _ramp;

//new resampling-related stuff
CvMat* _cumWeight;

//variables
CvMat* _particles;
CvMat* _newParticles;
CvMat* _noise;
CvMat* _noise1; //lines from 0 to 2
CvMat* _noise2; //lines from 3 to 5

yarp::os::Stamp _yarpTimestamp;
yarp::sig::ImageOf<yarp::sig::PixelRgb> *_yarpImage;
IplImage *_rawImage;
IplImage* _transformedImage;//_yuvBinsImage[image_width][image_height][3];
double _initialTime;
double _finalTime;

int _frameCounter;
int _framesNotTracking; //number of frames for which the likelihood has been under the threshold. after 20 such frames the tracker is restarted.
int downsampler;
float _lastU;
float _lastV;
bool _firstFrame; //when processing the first frame do not write the fps values, as it's wrong.

///////////////////////
//MEMBERS THAT "WORK":/
///////////////////////
bool testOpenCv();
bool readModelHistogram(CvMatND* histogram,const char fileName[]);
bool readInitialmodel3dPoints(CvMat* points,std::string fileName);
bool readMotionModelMatrix(CvMat* points, std::string fileName);
bool computeTemplateHistogram(std::string imageFileName,std::string dataFileName); //I checked the output, it seems to work, but it seems as if in the old version the normalization didn't take effect.
bool computeHistogram(CvMat* uv, IplImage* transformedImage,  CvMatND* innerHistogramMat, float &usedInnerPoints, CvMatND* outerHistogramMat, float &usedOuterPoints);
bool computeHistogramFromRgbImage(CvMat* uv, IplImage *image,  CvMatND* innerHistogramMat, float &usedInnerPoints, CvMatND* outerHistogramMat, float &usedOuterPoints);
bool calculateLikelihood(CvMatND* templateHistogramMat, CvMatND* innerHistogramMat, CvMatND* outerHistogramMat, float inside_outside, float &likelihood);
bool place3dPointsPerspective(CvMat* points, float x, float y, float z);
int perspective_projection(CvMat* xyz, float fx, float fy, float cx, float cy, CvMat* uv);
void drawContourPerspectiveYARP(CvMat* model3dPointsMat,float x, float y, float z, yarp::sig::ImageOf<yarp::sig::PixelRgb> *image,float _perspectiveFx,float  _perspectiveFy ,float _perspectiveCx,float  _perspectiveCy ,int R, int G, int B, float &meanU, float &meanV);
void drawSampledLinesPerspectiveYARP(CvMat* model3dPointsMat,float x, float y, float z, yarp::sig::ImageOf<yarp::sig::PixelRgb> *image,float _perspectiveFx,float  _perspectiveFy ,float _perspectiveCx,float  _perspectiveCy ,int R, int G, int B, float &meanU, float &meanV);
bool evaluateHypothesisPerspective(CvMat* model3dPointsMat, float x, float y, float z, CvMatND* modelHistogramMat, IplImage* transformedImage, float fx, float fy, float u0, float v0, float, float &likelihood);

//////////////////////////////////////////////
//MEMBERS THAT SHOULD BE CHANGED AND CHECKED:/
//////////////////////////////////////////////
bool evaluateHypothesisPerspectiveFromRgbImage(CvMat* model3dPoints,float x, float y, float z, CvMatND* modelHistogramMat, IplImage *image,  float fx, float fy, float u0, float v0, float inside_outside, float &likelihood);

bool systematicR(CvMat* inState, CvMat* weights, CvMat* outState);
bool systematic_resampling(CvMat* oldParticlesState, CvMat* oldParticlesWeights, CvMat* newParticlesState, CvMat* cumWeight);

public:

PF3DTracker(); //constructor
~PF3DTracker(); //destructor

virtual bool configure(yarp::os::ResourceFinder &rf); //member to set the object up.
virtual bool close();                  //member to close the object.
virtual bool interruptModule();        //member to close the object.
virtual bool updateModule();           //member that is repeatedly called by YARP, to give this object a chance to do something.
virtual double getPeriod();

};

#endif /* _PF3DTRACKER_ */
