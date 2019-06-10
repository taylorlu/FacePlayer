//
//  ViewController.h
//  FacePlayer
//
//  Created by LuDong on 2019/1/18.
//  Copyright © 2019年 LuDong. All rights reserved.
//

#include <opencv2/opencv.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#import <AVFoundation/AVFoundation.h>
#import <MobileCoreServices/MobileCoreServices.h>
#import <Endian.h>
#import <CoreML/CoreML.h>
#import <Vision/Vision.h>
#include "mtcnn.h"
#include "net.h"
#import "prnet.h"
#include "face-data.h"
#include "mesh_core.h"
#include <pthread.h>
#import <UIKit/UIKit.h>
#import <IJKMediaFramework/IJKMediaFramework.h>
#import "IJKMoviePlayerViewController.h"

@interface ViewController : UIViewController {
    
    double *inputData;
    MTCNN mtcnn;
    prnet *irModel;
    FaceData face_data;
    float *texture_color;
    
    float *depth_buffer;
    uint8_t *face_mask;
    float *verticesTemp;
    float *posTemp;
    float *new_image;
    uint8_t *output_image;
    
    int video_width;
    int video_height;
    uint64_t nb_frames;
    
    __weak IBOutlet UIButton *button;
    __weak IBOutlet UIProgressView *progressView;
    
    IJKVideoViewController *videoViewController;
//    IJKFFMoviePlayerController *ijkPlayerController;
//    MPMoviePlayerController *mPMoviePlayerController;
//    AVPlayerViewController *_PlayerVC;
//    AVPlayerViewController *playerViewController;
    NSString *outputFile;
}

//@property (nonatomic, strong) AVPlayerViewController *playerViewController;

- (IBAction)clickAction:(id)sender;

@end

