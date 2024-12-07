# HLS/M3U8 Download CLI (V lang)

A cli written in the V language to download m3u8 manifest based video and combine them in a mp4 video using fflmpeg

### Dependencies 
```bash
    # there are some things needed for successful compilation
    
    # on ubuntu/debian based systems 
    apt install libcurl openssl libsqlite

    # on RHEL/fedora
    dnf install libcurl-devel openssl-devel libsqlite3x-devel

    # on arch based systems these are installed by default (in most cases)

```

### V Dependencies 

```bash
    # this is nessecary because the builtin http module in V is 
    # very slow this is a libcurl wrapper and drastically 
    # improves network request made using http

    v install ttytm.vibe
```


### Compilation

```bash
    # compile the src code

    # for single threaded binary     
    v ./src/main.v 

    # for multithreading use 
    v ./src/multi_main.v 

```
lastly, you'll need a [statically compiled binary of ffmpeg](https://johnvansickle.com/ffmpeg/) or else the video merge would fail until ffmpeg successfully merges it together, extract it and add the ffmpeg binary to the root directory where your code is executing or wherever you decide to run the graalvm binary at