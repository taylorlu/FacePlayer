//
//  mediaPipeline.h
//  FacePlayer
//
//  Created by LuDong on 2019/1/18.
//  Copyright © 2019年 LuDong. All rights reserved.
//

#ifndef mediaPipeline_h
#define mediaPipeline_h

int initModule(const char *filename, const char *outputFile, uint64_t *nb_frames, int *w, int *h);
void *pushCycle(void *opaque);
int RenderFace(void *opaque, uint8_t *targetData, int frameCount);
void renderFinish(void *opaque);

#endif /* mediaPipeline_h */
