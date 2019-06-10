# Face converter in video

* The original repos https://github.com/taylorlu/FaceConverter
* extent face converter in video by using FFmpeg and ijkPlayer

### 1. Compile FFmpeg/x264, ijkplayer framework for iOS
Clone from https://github.com/bilibili/ijkplayer

Modify the following shell files:</br>
`init-ios.sh`, `config/module-lite.sh`, `ios/compile-ffmpeg.sh`, `ios/tools/do-compile-ffmpeg.sh`

Optional disable armv7/armv7s/i386

Enable x264 encoder, we need to remux to mp4/flv, x264 was commonly used

    export COMMON_FF_CFG_FLAGS="$COMMON_FF_CFG_FLAGS --enable-encoder=libx264"
    export COMMON_FF_CFG_FLAGS="$COMMON_FF_CFG_FLAGS --enable-gpl"
    export COMMON_FF_CFG_FLAGS="$COMMON_FF_CFG_FLAGS --enable-libx264"
    
Comment ssl since we have no need to support url network

    # SSL_LIBS="libcrypto libssl"

Integrate x264 encoder in FFmpeg

    X264=/Users/ludong/Desktop/FFmpeg/thin-x264/arm64
    if [ "$X264" ]
    then
      FFMPEG_CFLAGS="$FFMPEG_CFLAGS -I$X264/include"
      FFMPEG_LDFLAGS="$FFMPEG_LDFLAGS -L$X264/lib"
    fi
    --extra-ldflags="$FFMPEG_LDFLAGS -lx264 $FFMPEG_DEP_LIBS"

After that, it will generate `IJKMediaFramework.framework`, `libx264.a`

### 2. The other dependent libraries:
  * OpenCV: deal with image and matrix
  * ncnn: detect face
  * prnet: face converter
  
### 3. Running on iOS

