
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#include <sys/time.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <jpeglib.h>
#include "config.h"
#include "pointer.h"
#include "imgconv.h"
#include "vpxif.h"

FILE *fp;

unsigned int get_usec() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int) tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

void output_jpeg(unsigned int width, unsigned int height, unsigned char *rgbdata, char *filename, int quality) {
    FILE *fp = fopen(filename, "wb");
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    jpeg_stdio_dest(&cinfo, fp);
    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, 1);
    jpeg_start_compress(&cinfo, 1);

    JSAMPROW row_pointer[1];
    unsigned char *pBuf = (unsigned char *) malloc(3 * width);
    row_pointer[0] = pBuf;
    while (cinfo.next_scanline < cinfo.image_height) {
        memcpy(pBuf, rgbdata + 3 * cinfo.next_scanline * width, 3 * width);
        jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }
    free(pBuf);
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(fp);
}


int capture_desktop(unsigned int *width, unsigned int *height, unsigned char **yuvdata) {
    Window desktop;
    Display *disp;
    XImage *img;

    int screen_width, screen_height;

    disp = XOpenDisplay(NULL);      // Connect to a local display

    if (disp == NULL) {
        return -1;
    }
    desktop = RootWindow(disp, 0);  // Point to the root window
    if (desktop == 0) {
        return -1;
    }
    screen_width = DisplayWidth(disp, 0);
    screen_height = DisplayHeight(disp, 0);
    screen_width = SCREEN_WIDTH;
    screen_height = SCREEN_HEIGHT;
    img = XGetImage(disp, desktop, SCREEN_LEFT, SCREEN_TOP, screen_width, screen_height, AllPlanes, ZPixmap);

    unsigned int size = sizeof(unsigned char) * img->width * img->height * 1.5 * 3;
    unsigned char *rgbdata = (unsigned char *) malloc(size);
    *yuvdata = (unsigned char *) malloc(size);

    rgba2rgb(img->width, img->height, (unsigned char *) img->data, rgbdata);
    rgb2yuv420p(img->width, img->height, rgbdata, *yuvdata);
    free(rgbdata);

    *width = img->width;
    *height = img->height;

    XDestroyImage(img);
    XCloseDisplay(disp);
    return 0;
}

void callback(unsigned int size, unsigned char *vpxdata) {
    fwrite(vpxdata, size, 1, fp);
}

int main(int argc, char *argv[]) {
    unsigned int width, height;
    unsigned char *yuvdata, *vpxdata;
    int i = 0;
    
    vpxif_init(25, SCREEN_WIDTH, SCREEN_HEIGHT);
    FILE *yuv = fopen("/tmp/a.yuv", "wb");
    fp = fopen("/tmp/a.webm", "wb");
    while (i < 50) {
        printf("[Time] %u\n", get_usec());
        capture_desktop(&width, &height, &yuvdata);
        vpx_img_update(yuvdata, &vpxdata, callback);
        fwrite(yuvdata, width * height * 3 / 2, 1, yuv);
        free(yuvdata);
        usleep(10000);
        i++;
    }
    vpxif_finish_up(&vpxdata, callback);
    fclose(fp);
    fclose(yuv);
    return 0;
}

