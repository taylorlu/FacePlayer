//
//  ViewController.m
//  FacePlayer
//
//  Created by LuDong on 2019/1/18.
//  Copyright © 2019年 LuDong. All rights reserved.
//

#import "ViewController.h"

extern "C" {
#include "mediaPipeline.h"
}

@interface ViewController ()

@end

@implementation ViewController

-(int) process:(cv::Mat) imgMat : (float *)pos :(bool) isRGB{
    
    ncnn::Mat ncnn_img = ncnn::Mat::from_pixels(imgMat.data, isRGB ? ncnn::Mat::PIXEL_RGB : ncnn::Mat::PIXEL_BGR2RGB, imgMat.cols, imgMat.rows);
    std::vector<Bbox> finalBbox;
    
    mtcnn.detect(ncnn_img, finalBbox);
    
    int num_box = (int)finalBbox.size();
    vector<uint32_t> realPos;
    
    float maxScore=0;
    int maxProbIndex=0;
    for(int i=0; i<finalBbox.size(); i++) {
        if(finalBbox[i].score>maxScore) {
            maxScore = finalBbox[i].score;
            maxProbIndex = i;
        }
    }
    if(num_box>0) {
        int left = finalBbox[maxProbIndex].x1;
        int right = finalBbox[maxProbIndex].x2;
        int top = finalBbox[maxProbIndex].y1;
        int bottom = finalBbox[maxProbIndex].y2;
        
        float old_size = (right-left+bottom-top)/2.0;
        float centerX = right - (right-left)/2.0;
        float centerY = bottom - (bottom-top)/2 + old_size*0.14;
        int size = int(old_size*1.32);
        
        int x1 = centerX-size/2;
        int y1 = centerY-size/2;
        int x2 = centerX+size/2;
        int y2 = centerY+size/2;
        int width = x2 - x1;
        int height = y2 - y1;
        
        double scale = 256.0/width;
        double transX = -x1*scale;
        double transY = -y1*scale;
        
        if(x2>imgMat.cols) {
            cv::copyMakeBorder(imgMat, imgMat, 0, 0, 0, x2-imgMat.cols, cv::BORDER_CONSTANT, cv::Scalar(0));
        }
        if(x1<0) {
            cv::copyMakeBorder(imgMat, imgMat, 0, 0, -x1, 0, cv::BORDER_CONSTANT, cv::Scalar(0));
            x1 = 0;
        }
        if(y2>imgMat.rows) {
            cv::copyMakeBorder(imgMat, imgMat, 0, y2-imgMat.rows, 0, 0, cv::BORDER_CONSTANT, cv::Scalar(0));
        }
        if(y1<0) {
            cv::copyMakeBorder(imgMat, imgMat, -y1, 0, 0, 0, cv::BORDER_CONSTANT, cv::Scalar(0));
            y1 = 0;
        }
        cv::Mat cropped_image;
        cv::resize(imgMat(cv::Rect(x1, y1, width, height)), cropped_image, cv::Size(256,256));
        
        vector<cv::Mat> xc;
        split(cropped_image, xc);
        
        int count = 0;
        for(int i=0; i<256*256; i++) {
            inputData[count++] = *(xc[2].data+i)/256.0;
        }
        for(int i=0; i<256*256; i++) {
            inputData[count++] = *(xc[1].data+i)/256.0;
        }
        for(int i=0; i<256*256; i++) {
            inputData[count++] = *(xc[0].data+i)/256.0;
        }
        
        MLMultiArray *arr = [[MLMultiArray alloc] initWithDataPointer:inputData shape:[NSArray arrayWithObjects:[NSNumber numberWithInt:3], [NSNumber numberWithInt:256], [NSNumber numberWithInt:256], nil] dataType:MLMultiArrayDataTypeDouble strides:[NSArray arrayWithObjects:[NSNumber numberWithInt:256*256], [NSNumber numberWithInt:256], [NSNumber numberWithInt:1], nil] deallocator:nil error:nil];
        
        prnetOutput *output = [irModel predictionFromPlaceholder__0:arr error:nil];
        MLMultiArray *multiArr = [output resfcn256__Conv2d_transpose_16__Sigmoid__0];
        
        int plannerSize = [[multiArr strides][0] intValue];
        double *dataPointer = (double *)[multiArr dataPointer];
        for(int i=0; i<plannerSize*3; i++) {
            dataPointer[i] *= 1.1*256;
        }

        cv::Mat posMat1(1,256*256,CV_64F, dataPointer);
        cv::Mat posMat2(1,256*256,CV_64F, dataPointer + plannerSize);
        cv::Mat posMat3(1,256*256,CV_64F, dataPointer + plannerSize*2);

        double tformData[9] = {scale,0.0,transX, 0.0,scale,transY, 0.0,0.0,1.0};
        cv::Mat tform(3,3,CV_64F, tformData);
        cv::Mat z = posMat3/scale;
        posMat3.setTo(cv::Scalar(1));

        cv::Mat posMats;
        posMats.push_back(posMat1);
        posMats.push_back(posMat2);
        posMats.push_back(posMat3);

        cv::Mat vertices;
        vertices = tform.inv()*posMats;
        z.row(0).copyTo(vertices.row(2));
        vertices.convertTo(vertices, CV_32F);

        memcpy(pos, vertices.data, 256*256*3*sizeof(float));
        
        return num_box;
    }
    return 0;
}

void renderFinish(void *opaque) {
    ViewController *instance =  (__bridge ViewController *)opaque;
    return [instance renderFinishImpl];
}

-(void) renderFinishImpl {
    dispatch_async(dispatch_get_main_queue(), ^{
        [button setTitle:@"play" forState:UIControlStateNormal];
        [button setEnabled:YES];
    });
}

int RenderFace(void *opaque, uint8_t *targetData, int frameCount) {
    
    ViewController *instance =  (__bridge ViewController *)opaque;
    return [instance RenderFaceImpl: targetData: frameCount];
}

-(int) RenderFaceImpl: (uint8_t *)targetData: (int) frameCount{
    
    cv::Mat targetMat = cv::Mat(video_height, video_width, CV_8UC3, targetData);
    int faceCount = 0;
    @autoreleasepool{
        faceCount = [self process: targetMat: posTemp: YES];
    }
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [progressView setProgressViewStyle:UIProgressViewStyleDefault];
        [progressView setProgress:(float)frameCount/nb_frames animated:YES];
    });
    
    if(faceCount>0) {

        int count = 0;
        int nver = (int)face_data.face_indices.size();
        int ntri = (int)face_data.triangles.size()/3;

        // verticesTemp ==> (xyz, xyz, xyz, ...)
        for (int i=0; i<nver; i++) {
            int ind = face_data.face_indices[i];
            verticesTemp[count++] = float(*(posTemp+ ind));
            verticesTemp[count++] = float(*(posTemp+256*256   + ind));
            verticesTemp[count++] = float(*(posTemp+256*256*2 + ind));
        }

        int *triangles = (int *)face_data.triangles.data();

        for(int i=0; i<video_width*video_height; i++) {
            depth_buffer[i] = -999999.0;
        }

        memset(face_mask, 0, video_width*video_height*sizeof(uint8_t));

        // new_image ==> (rgb, rgb, rgb, ...)
        _render_colors_core(new_image, face_mask, verticesTemp, triangles, texture_color, depth_buffer, ntri, video_height, video_width, 3);

        vector<cv::Mat> xc;
        split(targetMat, xc);

        cv::Mat maskMat = cv::Mat(targetMat.rows, targetMat.cols, CV_8U, face_mask);
        cv::Rect rect = cv::boundingRect(maskMat);

        count = 0;
        for(int i=0; i<video_width*video_height; i++) {
            output_image[count++] = uint8_t( *(xc[2].data+i)*(255-face_mask[i]) + new_image[i*3+2]*face_mask[i] );
            output_image[count++] = uint8_t( *(xc[1].data+i)*(255-face_mask[i]) + new_image[i*3+1]*face_mask[i] );
            output_image[count++] = uint8_t( *(xc[0].data+i)*(255-face_mask[i]) + new_image[i*3]*face_mask[i] );
        }

        cv::Mat ooo = cv::Mat(targetMat.rows, targetMat.cols, CV_8UC3, output_image);
        cv::Mat outMat;

        cv::seamlessClone(ooo, targetMat, maskMat, cv::Point(int(rect.x+rect.width/2), int(rect.y+rect.height/2)), outMat, cv::MIXED_CLONE);
        memcpy(targetData, outMat.data, video_width*video_height*3);
        return faceCount;
    }
    return 0;
    
}

- (void)viewDidLoad {
    [super viewDidLoad];
    // Do any additional setup after loading the view, typically from a nib.
    [button setTitle:@"Processing" forState:UIControlStateNormal];
    [button setEnabled:NO];
    
    const char *movFile = [[[NSBundle mainBundle] pathForResource:@"rmdmy_cut" ofType:@"mp4"] UTF8String];
    NSArray *docPath = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsPath = [docPath objectAtIndex:0];
    outputFile = [documentsPath stringByAppendingPathComponent:@"output.mp4"];
    
    int ret = initModule(movFile, [outputFile UTF8String], &nb_frames, &video_width, &video_height);
    if(ret<0) {
        NSLog(@"FFmpeg initial failed!");
        return;
    }
    
    /////////////mtcnn-ncnn
    char *path = (char *)[[[NSBundle mainBundle] resourcePath] UTF8String];
    mtcnn.init(path);
    mtcnn.SetMinFace(100);
    
    irModel = [[prnet alloc] init];
    
    inputData = (double *)malloc(sizeof(double)*256*256*3);
    
    float *pos = (float *)malloc(256*256*3*sizeof(float));
    verticesTemp = (float *)malloc(256*256*3*sizeof(float));
    posTemp = (float *)malloc(256*256*3*sizeof(float));
    
    depth_buffer = (float *)malloc(video_width*video_height*sizeof(float));
    face_mask = (uint8_t *)malloc(video_width*video_height*sizeof(uint8_t));
    new_image = (float *)malloc(video_width*video_height*3*sizeof(float));
    output_image = (uint8_t *)malloc(video_width*video_height*3*sizeof(uint8_t));
    
    memset(new_image, 0, video_width*video_height*3*sizeof(float));
    memset(face_mask, 0, video_width*video_height*sizeof(uint8_t));
    memset(output_image, 0, video_width*video_height*3*sizeof(uint8_t));
    
    LoadFaceData([[[NSBundle mainBundle] resourcePath] UTF8String], &face_data);
    
    void *handler = (__bridge void *)self;
    NSThread *thread = [[NSThread alloc] initWithBlock:^{
    
        cv::Mat imgMat = cv::imread([[[NSBundle mainBundle] pathForResource:@"3" ofType:@"jpg"] UTF8String]);
        
        int faceCount = [self process: imgMat: pos: NO];
        if(faceCount>0) {
            
            cv::Mat ref_pos1 = cv::Mat(256,256, CV_32F, pos);
            cv::Mat ref_pos2 = cv::Mat(256,256, CV_32F, pos + 256*256);
            
            cv::Mat posMat;
            vector<cv::Mat> posMats;
            posMats.push_back(ref_pos1);
            posMats.push_back(ref_pos2);
            cv::merge(posMats, posMat);
            
            imgMat.convertTo(imgMat, CV_32FC3, 1/256.0);
            
            cv::Mat remapMat;
            cv::remap(imgMat, remapMat, posMat, cv::Mat(), cv::INTER_NEAREST);
            
            texture_color = (float *)malloc(face_data.face_indices.size()*sizeof(float)*3);
            int count = 0;
            vector<cv::Mat> xc2;
            split(remapMat, xc2);
            
            for (int i=0; i<face_data.face_indices.size(); i++) {
                int ind = face_data.face_indices[i];
                texture_color[count++] = *((float *)xc2[0].data+ ind);
                texture_color[count++] = *((float *)xc2[1].data+ ind);
                texture_color[count++] = *((float *)xc2[2].data+ ind);
            }
        }
        
        pushCycle(handler);
    }];
    [thread start];
}


- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    // Dispose of any resources that can be recreated.
}


- (IBAction)clickAction:(id)sender {
    
    NSURL *url = [NSURL URLWithString:outputFile];
    [IJKVideoViewController presentFromViewController:self withTitle:@"test" URL:url completion:nil];
}
@end
