# HLS/M3U8 Download CLI

A cli written in java to download m3u8 manifest based video and combine them in a mp4 video

### Compilation (javac)

```bash
    # compile the src code
    # the compilation requires the
    # org.json.JSONObject package
    
    javac -cp json-20240303.jar src/*.java
```

### Running (java)
```bash
    # running the class file
    java -cp json-20240303.jar:./src Main https://bitdash-a.akamaihd.net/content/sintel/hls/video/500kbit.m3u8 yourDesiredFileName 
```

## An alternative way to compile it would be to use [Graalvm](https://www.graalvm.org/) or [scala-native](http://www.scala-native.org/)

### Compilation (graalvm)
```bash
    # compile the src code
    javac -cp json-20240303.jar src/*.java
    
    # installing graalvm gives you the native-image command 
    native-image -cp . json-20240303.jar:./src Main --enable-sbom -march=native --strict-image-heap --no-fallback
```

lastly, you'll need a [statically compiled binary of ffmpeg](https://johnvansickle.com/ffmpeg/) or else the video merge would fail until ffmpeg successfully merges it together, extract it and add the ffmpeg binary to the root directory where your code is executing or wherever you decide to run the graalvm binary at