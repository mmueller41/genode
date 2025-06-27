/**********************************************************************
Copyright (c) 2014,BUAA COMPILER LAB;
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
- Neither the name of the BUAA COMPILER LAB; nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************/

// standard utilities and system includes
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define OCV
#ifdef OCV
#include <opencv2/opencv.hpp>
#else
#include "frac_320_240.h"
#endif // OCF

#define CL_TARGET_OPENCL_VERSION 300
#include "../../CL/cl.h"

#include <time.h>
#include <unistd.h>

using namespace std;
#define LINUX
#ifdef LINUX
#include <sys/time.h>
#endif // LINUX
#include <math.h>
///////////////////////////////////////////////////////////

// Display the process of each step
#define OUTINFO

// Profile the result of each step to a file
// #define profile

// #define SLM

///////////////////////////////////////////////////////////

#ifdef SLM
#warning no SLM support
#include "SURF_kernel.h"
#else
#include "SURF_noSLM_kernel.h"
#endif // SLM

namespace ns_OpenSurf {

#ifdef OCV
typedef cv::Mat Image;
#else  // OCV
namespace fake_cv
{
    class Mat
    {
    public:
        int rows;
        int cols;
        int step;
        float *data;
        template <typename T>
        T *ptr(int off)
        {
            return (T *)&data[off * sizeof(T)];
        }
    };

    Mat imread(const char *img)
    {
        Mat m;
        m.rows = height;
        m.cols = width;
        m.step = width * sizeof(float);

        const size_t size = m.rows * m.cols;
        m.data = (float*)malloc(size * sizeof(float));

        for (size_t i = 0; i < size; ++i)
        {
            unsigned int px[3];
            HEADER_PIXEL(img, px);
            const unsigned int g = 0.298936021293775 * px[0] + 0.587043074451121 * px[1] + 0.114020904255103 * px[2];
            m.data[i] = g / 251.; // 255.
        }

        return m;
    }
}

typedef fake_cv::Mat Image;
#endif // OCF

// image & step & filter size map

float co[10] = {1, 1, 1, 1, 0.5, 0.5, 0.25, 0.25, 0.125, 0.125};
int step[10] = {2, 2, 2, 2, 4, 4, 8, 8, 16, 16};
int filter[10] = {9, 15, 21, 27, 39, 51, 75, 99, 147, 195};

// OpenCL variables
cl_context context;
cl_command_queue clqueue;
cl_int ciErrNum = CL_SUCCESS;
cl_program cpProgram;
cl_kernel ckRowIntegral, ckColIntegral;
cl_kernel ckBuildResponseLayer, ckIsExtremum, ckGetOrientation, ckdesReady, ckcomputeDes, cknormalDes;

cl_platform_id platform = NULL;
cl_device_id device = NULL;
cl_ulong start, end;
cl_mem intImage;
cl_mem d_Input, d_Output;
cl_mem responses, laplacian;
cl_mem isExtremum;
cl_mem mipts;
cl_mem orientation;
cl_mem xs, ys, gauss_s2;
cl_mem rrx, rry;
cl_mem des;
cl_mem mid, ndes;

///////////////////////////////////////////////////////////
double cRow, cCol, cInt, cBui, cExt, cOut, cMov, cRnum, cOri, cDes, cnDes, cCom;
cl_event RowEvent, ColEvent, BuiEvent, ExtEvent, WriteOut, WriteMipts, OriEvent, DesEvent, nDesEvent, comEvent;
///////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////
size_t szParmDataBytes;
size_t szKernelLength;
Image source;
Image img;
///////////////////////////////////////////////////////////

Image *Integral(Image &img);
void integralHost(Image &source, Image &intImage);
/////////////////////////////////////////////////////////////////
#ifdef OUTINFO
#define SHOWERR(x)                      \
    do                                  \
    {                                   \
        if (ciErrNum != CL_SUCCESS)     \
        {                               \
            printf("\nerr:\t");         \
            printf(#x);                 \
            printf("#:%d\n", ciErrNum); \
            printf("@%d\n", __LINE__);  \
            printf("\n");               \
            exit(1);                    \
        }                               \
        else                            \
        {                               \
            printf("done!\t");          \
            printf(#x);                 \
            printf("\n");               \
        }                               \
    } while (0)
#else
#define SHOWERR(x)
#endif

#ifdef OUTINFO
#define SHOWINFO(x)         \
    do                      \
    {                       \
        printf("\nnow:\t"); \
        printf(#x);         \
        printf("\n");       \
    } while (0)
#else
#define SHOWINFO(x)
#endif

#define CLEANUP()                              \
    do                                         \
    {                                          \
        if (cpProgram)                         \
            clReleaseProgram(cpProgram);       \
        if (clqueue)                           \
            clReleaseCommandQueue(clqueue);    \
        if (context)                           \
            clReleaseContext(context);         \
        clReleaseKernel(ckBuildResponseLayer); \
        clReleaseKernel(ckIsExtremum);         \
        clReleaseKernel(ckGetOrientation);     \
        clReleaseKernel(ckdesReady);           \
        clReleaseKernel(ckcomputeDes);         \
        clReleaseKernel(cknormalDes);          \
        clReleaseMemObject(intImage);          \
        clReleaseMemObject(responses);         \
        clReleaseMemObject(laplacian);         \
        clReleaseMemObject(isExtremum);        \
        clReleaseMemObject(orientation);       \
        clReleaseMemObject(mipts);             \
        clReleaseMemObject(xs);                \
        clReleaseMemObject(ys);                \
        clReleaseMemObject(gauss_s2);          \
        clReleaseMemObject(des);               \
        clReleaseMemObject(mid);               \
        clReleaseMemObject(ndes);              \
        clReleaseEvent(RowEvent);              \
        clReleaseEvent(ColEvent);              \
        clReleaseEvent(BuiEvent);              \
        clReleaseEvent(ExtEvent);              \
        clReleaseEvent(WriteOut);              \
        clReleaseEvent(WriteMipts);            \
        clReleaseEvent(OriEvent);              \
        clReleaseEvent(DesEvent);              \
        clReleaseEvent(nDesEvent);             \
        clReleaseEvent(comEvent);              \
    } while (0)
///////////////////////////////////////////////////////////

// Returns profiling time
double executionTime(cl_event &event)
{
    cl_ulong start, end;

    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
    clGetEventProfilingInfo(event, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);

    return (double)1.0e-6 * (end - start); // convert nanoseconds to min seconds on return
}

// Round Up Division function
size_t shrRoundUp(int group_size, int global_size)
{
    int r = global_size % group_size;
    if (r == 0)
    {
        return global_size;
    }
    else
    {
        return global_size + group_size - r;
    }
}

#ifdef OCV
cv::Mat getGray(const cv::Mat &img)
{
    cv::Mat gray8;
    cv::Mat gray32;

    if (img.channels() == 1)
    {
        gray8 = img.clone();
    }
    else
    {
        cv::cvtColor(img, gray8, cv::COLOR_BGR2GRAY);
    }
    gray8.convertTo(gray32, CV_32F, 1.0 / 255.0, 0);

    return gray32;
}
#else  // OCV
fake_cv::Mat getGray(const fake_cv::Mat &img)
{
    return img;
}
#endif // OCV

// Program main
int main(int argc, char **argv)
{
#ifdef OCV
    if (argc < 2)
    {
        std::cout << "Usage: " << argv[0] << " img" << std::endl;
        return -1;
    }
#endif // OCV

    //////////////////////////////////////////////////////////////////////////////
    // Init OpenCL devices
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////

    cl_uint status;

    // Discover and initialize the platform
    cl_uint numPlatforms;
    //std::string platformVendor;
    status = clGetPlatformIDs(0, NULL, &numPlatforms);
    // printf("numPlatforms:%d\n",numPlatforms);
    if (0 < numPlatforms)
    {
        cl_platform_id *platforms = (cl_platform_id*)malloc(numPlatforms * sizeof(cl_platform_id));
        status = clGetPlatformIDs(numPlatforms, platforms, NULL);
        char platformName[100];
        for (unsigned i = 0; i < numPlatforms; ++i)
        {
            status = clGetPlatformInfo(platforms[i],
                                       CL_PLATFORM_VENDOR,
                                       sizeof(platformName),
                                       platformName,
                                       NULL);

            platform = platforms[i];
            //platformVendor.assign(platformName);
        }
        //std::cout << "Platform found : " << platformName << "\n";
        free(platforms);
    }
    status = clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, NULL);

    // create a context
    context = clCreateContext(NULL, 1, &device, NULL, NULL, NULL);
    // create a command queue
    clqueue = clCreateCommandQueue(context, device, CL_QUEUE_PROFILING_ENABLE, NULL);
        // Create and compile the program
#ifdef SLM
    const size_t kernel_size = SURF_Gen9core_gen_len;
    const unsigned char* kernel_bin = SURF_Gen9core_gen;
#else // SLM
    const size_t kernel_size = SURF_noSLM_Gen9core_gen_len;
    const unsigned char* kernel_bin = SURF_noSLM_Gen9core_gen;
#endif // SLM
    cpProgram = clCreateProgramWithBinary(context, 1, &device, &kernel_size, &kernel_bin, NULL, NULL);
    status = clBuildProgram(cpProgram, 1, &device, NULL, NULL, NULL);

#ifdef OUTINFO
    if (status != CL_SUCCESS)
    {
        size_t log_size;
        clGetProgramBuildInfo(cpProgram, device, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
        char *log = (char *)malloc(log_size);
        clGetProgramBuildInfo(cpProgram, device, CL_PROGRAM_BUILD_LOG, log_size, log, NULL);
        printf("Error!! %s\n", log);
    }
#endif

    /*SHOWINFO(clBuildProgram);
            ciErrNum = clBuildProgram(cpProgram,0,NULL,"-cl-mad-enable",NULL,NULL);
            if (ciErrNum != CL_SUCCESS)
            {
                    printf("err!\t");
                    printf("clBuildProgram");
                    printf("\n");

                    //oclLogBuildInfo(cpProgram, oclGetFirstDev(context));
                    //oclLogPtx(cpProgram, oclGetFirstDev(context), "SURF.ptx");
                    exit(1);

            }
    */

    /////////////////////////  Create kernel   ///////////////////////////
#ifdef LINUX
    struct timeval tCreate1, tCreate2;
    gettimeofday(&tCreate1, NULL);
#else
    clock_t tCreate1 = clock();
#endif

    SHOWINFO(clCreateKernel);
    ckRowIntegral = clCreateKernel(cpProgram, "rowIntegral", &ciErrNum);
    SHOWERR(clCreateKernel\t\t\tckRowIntegral);

    SHOWINFO(clCreateKernel);
    ckColIntegral = clCreateKernel(cpProgram, "colIntegral", &ciErrNum);
    SHOWERR(clCreateKernel\t\t\tckBuildResponseLayer);

    SHOWINFO(clCreateKernel);
    ckBuildResponseLayer = clCreateKernel(cpProgram, "BuildResponseLayer", &ciErrNum);
    SHOWERR(clCreateKernel\t\t\tckBuildResponseLayer);

    SHOWINFO(clCreateKernel);
    ckIsExtremum = clCreateKernel(cpProgram, "IsExtremum", &ciErrNum);
    SHOWERR(clCreateKernel\t\tckIsExtremum);

    SHOWINFO(clCreateKernel);
    ckGetOrientation = clCreateKernel(cpProgram, "GetOrientation", &ciErrNum);
    SHOWERR(clCreateKernel\t\tckGetOrientation);

    SHOWINFO(clCreateKernel);
    ckdesReady = clCreateKernel(cpProgram, "desReady", &ciErrNum);
    SHOWERR(clCreateKernel);

    SHOWINFO(clCreateKernel);
    ckcomputeDes = clCreateKernel(cpProgram, "computeDes", &ciErrNum);
    SHOWERR(clCreateKernel);

    SHOWINFO(clCreateKernel);
    cknormalDes = clCreateKernel(cpProgram, "normalDes", &ciErrNum);
    SHOWERR(clCreateKernel);

#ifdef LINUX
    gettimeofday(&tCreate2, NULL);
    double tCr = (tCreate2.tv_sec - tCreate1.tv_sec) * 1000 + (tCreate2.tv_usec - tCreate1.tv_usec) / (float)1000;

#else
    clock_t tCreate2 = clock();
    double tCr = (double)(tCreate2 - tCreate1) / CLOCKS_PER_SEC * 1000;
#endif

    //////////////////////////////////////////////////////////////////////
    // source      =  cvLoadImage(".\\frac_320_240.jpg");
#ifdef OCV
    source = cv::imread(argv[1]);
#else
    source = fake_cv::imread(header_data);
#endif // OCV
    // convert the image to single channel 32f
    img = getGray(source);

    int height = img.rows;
    int width = img.cols;
    int stride = img.step / sizeof(float);
    float *data = (float *)img.ptr<float>(0);
    int imgSize = height * width;

#ifdef profile
#ifdef OCV
    Image intImgHost = Image(img.size(), CV_32F, 1);
#else  // OCV
    Image intImgHost;
    intImgHost.rows = img.rows;
    intImgHost.cols = img.cols;
    intImgHost.step = sizeof(float);
    intImgHost.data = (float*) malloc(img.cols * img.rows * sizeof(float));
#endif // OCV

    struct timeval tInt1, tInt2;
    gettimeofday(&tInt1, NULL);

    // Computes the integral image with cpu function
    integralHost(source, intImgHost);

    gettimeofday(&tInt2, NULL);
    double tInt = (tInt2.tv_sec - tInt1.tv_sec) * 1000 + (tInt2.tv_usec - tInt1.tv_usec) / (float)1000;

    float *idata = (float *)img.ptr<float>(0);
    printf("cpu_imgdata.dat:\n");
    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
            printf("%f\t", *idata++);
        printf("\n");
    }
#ifndef OCV
    free(intImgHost.data);
#endif // OCV

#endif

    //////////////////////////////////////////////////////////////////
#ifdef LINUX
    struct timeval tStart, tEnd;
    gettimeofday(&tStart, NULL);
#else
    clock_t tStart = clock();
#endif
    /////////////////////////////////////////////////////////////////

    unsigned int blockSize = 256; // 512; // max size of the thread blocks
    unsigned int sharedMemSize = 2 * blockSize;

    /*
    #ifdef profile
    unsigned int numBlocks =
        max(1, (int)ceil((float)imgSize / (2.f * blockSize)));
    printf("imgSize is %d, blockSize is %d, numBlocks is %d\n", imgSize, blockSize, numBlocks);
    #endif
    */

    //d_Input = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, imgSize * sizeof(float), data, &ciErrNum);
    d_Input = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, imgSize * sizeof(float), NULL, &ciErrNum);
    ciErrNum = clEnqueueWriteBuffer(clqueue, d_Input, CL_FALSE, 0, imgSize * sizeof(float), (int *)data, 0, NULL, NULL);
    //        oclCheckError(ciErrNum, CL_SUCCESS);
    d_Output = clCreateBuffer(context, CL_MEM_READ_WRITE, imgSize * sizeof(float), NULL, &ciErrNum);
    //        oclCheckError(ciErrNum, CL_SUCCESS);
    intImage = clCreateBuffer(context, CL_MEM_READ_WRITE, imgSize * sizeof(float), NULL, &ciErrNum);
    //        oclCheckError(ciErrNum, CL_SUCCESS);


#ifdef profile
    int N = imgSize;
    printf("in.dat:\n");

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
            printf("%f\t", data[i * width + j]);
        printf("\n");
    }
#endif

    // RowIntegral kernel: First Compute prefix sum in rows of the source image
    size_t rowLocalWorkSize, rowGlobalWorkSize;

    rowLocalWorkSize = blockSize;
    rowGlobalWorkSize = height * blockSize;

    ciErrNum = clSetKernelArg(ckRowIntegral, 0, sizeof(cl_mem), (void *)&d_Input);
    ciErrNum |= clSetKernelArg(ckRowIntegral, 1, sizeof(cl_mem), (void *)&d_Output);
#ifdef SLM
    ciErrNum |= clSetKernelArg(ckRowIntegral, 2, sharedMemSize * sizeof(float), NULL);
    ciErrNum |= clSetKernelArg(ckRowIntegral, 3, sizeof(int), (void *)&width);
#else // SLM
    ciErrNum |= clSetKernelArg(ckRowIntegral, 2, sizeof(int), (void *)&width);
#endif // SLM

    /*
    #ifdef profile
    printf("rowGlobalWorkSize is %d, rowLocalWorkSize is %d\n", rowGlobalWorkSize,  rowLocalWorkSize);
    #endif
    */

    ciErrNum = clEnqueueNDRangeKernel(clqueue,
                                      ckRowIntegral,
                                      1, NULL,
                                      &rowGlobalWorkSize,
                                      &rowLocalWorkSize,
                                      0, NULL, &RowEvent);
    //        oclCheckError(ciErrNum, CL_SUCCESS);

    clFinish(clqueue);
    //clWaitForEvents(1, &RowEvent);
    cl_ulong start, end;
    clGetEventProfilingInfo(RowEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
    clGetEventProfilingInfo(RowEvent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);

    // cRow = executionTime(RowEvent);
    cRow = (end - start) * 1e-6;

#ifdef profile
    float *h_OutputGPU = (float *)malloc(imgSize * sizeof(float));
    ciErrNum = clEnqueueReadBuffer(clqueue, d_Output, CL_TRUE, 0, N * sizeof(float), h_OutputGPU, 0, NULL, NULL);
    // oclCheckError(ciErrNum, CL_SUCCESS);

    printf("Rout.dat");

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
            printf("%f\t", h_OutputGPU[i * width + j]);
        printf("\n");
    }
    free(h_OutputGPU);
#endif

    // RowIntegral kernel: Then Compute prefix sum in colunms after RowIntegral kernel
    size_t colLocalWorkSize, colGlobalWorkSize;
    colLocalWorkSize = blockSize;
    colGlobalWorkSize = width * blockSize;

    ciErrNum = clSetKernelArg(ckColIntegral, 0, sizeof(cl_mem), (void *)&d_Output);
    ciErrNum |= clSetKernelArg(ckColIntegral, 1, sizeof(cl_mem), (void *)&intImage);
#ifdef SLM
    ciErrNum |= clSetKernelArg(ckColIntegral, 2, sharedMemSize * sizeof(float), NULL);
    ciErrNum |= clSetKernelArg(ckColIntegral, 3, sizeof(int), (void *)&height);
    ciErrNum |= clSetKernelArg(ckColIntegral, 4, sizeof(int), (void *)&width);
#else // SLM
    ciErrNum |= clSetKernelArg(ckColIntegral, 2, sizeof(int), (void *)&height);
    ciErrNum |= clSetKernelArg(ckColIntegral, 3, sizeof(int), (void *)&width);
#endif // SLM

    /*
    #ifdef profile
    printf("colGlobalWorkSize is %d, colLocalWorkSize is %d\n", colGlobalWorkSize,  colLocalWorkSize);
    #endif
    */

    ciErrNum = clEnqueueNDRangeKernel(clqueue, ckColIntegral, 1, NULL,
                                      &colGlobalWorkSize, &colLocalWorkSize, 0, NULL, &ColEvent);
    //        oclCheckError(ciErrNum, CL_SUCCESS);

    clFinish(clqueue);
    //clWaitForEvents(1, &ColEvent);

    clGetEventProfilingInfo(ColEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
    clGetEventProfilingInfo(ColEvent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);

    // cCol = executionTime(ColEvent);
    cCol = (end - start) * 1e-6;

#ifdef profile
    float *h_ImgputGPU = (float *)malloc(imgSize * sizeof(float));
    ciErrNum = clEnqueueReadBuffer(clqueue, intImage, CL_TRUE, 0, N * sizeof(float), h_ImgputGPU, 0, NULL, NULL);
    // oclCheckError(ciErrNum, CL_SUCCESS);

    printf("intImage.dat:\n");

    for (int i = 0; i < height; i++)
    {
        for (int j = 0; j < width; j++)
            printf("%f\t", h_ImgputGPU[i * width + j]);
        printf("\n");
    }
    free(h_ImgputGPU);
#endif

    // BuildResponeselayer kernel: Calculate DoH responses for the layers
    int h = height / 2;
    int w = width / 2;
    int s = 2;

    SHOWINFO(clCreateBuffer);
    responses = clCreateBuffer(context,
                               CL_MEM_READ_WRITE,
                               10 * w * h * sizeof(cl_float),
                               NULL,
                               &ciErrNum);
    SHOWERR(clCreateBuffer\t\t\tresponses);

    laplacian = clCreateBuffer(context,
                               CL_MEM_READ_WRITE,
                               10 * w * h * sizeof(cl_float),
                               NULL,
                               &ciErrNum);
    SHOWERR(clCreateBuffer\t\t\tlaplacian);

    size_t szBuildLocalWorkSize = 64;
    size_t szBuildGlobalWorkSize = shrRoundUp((int)szBuildLocalWorkSize, h * w); // rounded up to the nearest multiple of the LocalWorkSize

    int k = 0;
    clSetKernelArg(ckBuildResponseLayer, k++, sizeof(cl_mem), (void *)&responses);
    clSetKernelArg(ckBuildResponseLayer, k++, sizeof(cl_mem), (void *)&laplacian);
    clSetKernelArg(ckBuildResponseLayer, k++, sizeof(cl_mem), (void *)&intImage);
    clSetKernelArg(ckBuildResponseLayer, k++, sizeof(int), (void *)&h);
    clSetKernelArg(ckBuildResponseLayer, k++, sizeof(int), (void *)&w);
    clSetKernelArg(ckBuildResponseLayer, k++, sizeof(int), (void *)&s);
    clSetKernelArg(ckBuildResponseLayer, k++, sizeof(int), (void *)&stride);

    ciErrNum = clEnqueueNDRangeKernel(clqueue,
                                      ckBuildResponseLayer,
                                      1, NULL,
                                      &szBuildGlobalWorkSize,
                                      &szBuildLocalWorkSize,
                                      0, NULL, &BuiEvent);
    SHOWERR(clEnqueueNDRangeKernel\t\tckBuildResponseLayer);

    clFinish(clqueue);
    //clWaitForEvents(1, &BuiEvent);
    clGetEventProfilingInfo(BuiEvent, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
    clGetEventProfilingInfo(BuiEvent, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);

    // cBui = executionTime(BuiEvent);
    cBui = (end - start) * 1e-6;

#ifdef profile
    float *hostResponses = (float *)malloc(10 * h * w * sizeof(float));
    float *hostLaplacian = (float *)malloc(10 * h * w * sizeof(float));

    SHOWINFO(clEnqueueReadBuffer);
    ciErrNum = clEnqueueReadBuffer(clqueue, responses, CL_TRUE, 0,
                                   10 * h * w * sizeof(cl_float), (float *)hostResponses, 0, NULL, NULL);
    SHOWERR(clEnqueueReadBuffer\t\thostResponses < -- -responses);

    ciErrNum = clEnqueueReadBuffer(clqueue, laplacian, CL_TRUE, 0,
                                   10 * h * w * sizeof(cl_float), (float *)hostLaplacian, 0, NULL, NULL);
    SHOWERR(clEnqueueReadBuffer\t\thostLaplacian < -- -laplacian);

    printf("responses.dat:\n");
    for (int dptr = 0; dptr < 10 * h * w; dptr++)
    {
        if (dptr % 10 == 0 && dptr != 0)
        {
            printf("\n");
        }
        printf("%f ", hostResponses[dptr]);
    }

    printf("laplacian.dat:\n");
    for (int dptr = 0; dptr < 10 * h * w; dptr++)
    {
        if (dptr % 10 == 0 && dptr != 0)
        {
            printf("\n");
        }
        printf("%f ", hostLaplacian[dptr]);
    }

    free(hostResponses);
    free(hostLaplacian);
#endif

    // IsExtremum kernel: Non Maximal Suppression function
    struct _isExtremum
    {
        int x, y;
        float scale;
        int lap;
    };

    SHOWINFO(clCreateBuffer);
    isExtremum = clCreateBuffer(context,
                                CL_MEM_READ_WRITE,
                                8 * w * h * sizeof(struct _isExtremum),
                                NULL,
                                &ciErrNum);
    SHOWERR(clCreateBuffer);

#define ExtBlockSize 8

    SHOWINFO(clCreateBuffer);
    cl_mem cnum = clCreateBuffer(context,
                                 CL_MEM_READ_WRITE,
                                 sizeof(int),
                                 NULL,
                                 &ciErrNum);
    SHOWERR(clCreateBuffer\t\t\tcnum);

    int hnum[] = {0};
    ciErrNum = clEnqueueWriteBuffer(clqueue, cnum, CL_FALSE, 0, sizeof(int), (int *)hnum, 0, NULL, NULL);

    size_t hh = shrRoundUp((size_t)ExtBlockSize, h);
    size_t ww = shrRoundUp((size_t)ExtBlockSize, w);
    size_t szExtGlobalWorkSize[] = {hh, ww};
    size_t szExtLocalWorkSize[] = {ExtBlockSize, ExtBlockSize};

    clSetKernelArg(ckIsExtremum, 0, sizeof(cl_mem), (void *)&responses);
    clSetKernelArg(ckIsExtremum, 1, sizeof(cl_mem), (void *)&laplacian);
    clSetKernelArg(ckIsExtremum, 2, sizeof(cl_mem), (void *)&isExtremum);
    clSetKernelArg(ckIsExtremum, 3, sizeof(int), (void *)&h);
    clSetKernelArg(ckIsExtremum, 4, sizeof(int), (void *)&w);
    clSetKernelArg(ckIsExtremum, 5, sizeof(cl_mem), (void *)&cnum);

    SHOWINFO(clEnqueueNDRangeKernel);
    ciErrNum = clEnqueueNDRangeKernel(clqueue,
                                      ckIsExtremum,
                                      2, NULL,
                                      szExtGlobalWorkSize,
                                      szExtLocalWorkSize,
                                      0, NULL, &ExtEvent);
    SHOWERR(clEnqueueNDRangeKernel\t\tckIsExtremum);

    clFinish(clqueue);
    //clWaitForEvents(1, &ExtEvent);
    cExt = executionTime(ExtEvent);

    SHOWINFO(clEnqueueReadBuffer);
    ciErrNum = clEnqueueReadBuffer(clqueue,
                                   cnum,
                                   CL_TRUE, 0,
                                   sizeof(int),
                                   (int *)hnum,
                                   0, NULL, NULL);
    SHOWERR(clEnqueueReadBuffer\t\tnum < -- -cnum);

    // cmn is the number of interest point
    int cmn = hnum[0];

#ifdef profile
    struct _isExtremum *hostExtLocation = (struct _isExtremum *)malloc(8 * w * h * sizeof(struct _isExtremum));
    if (hostExtLocation == NULL)
    {
        printf("\nhostExtLocation fail!!!\n");
    }

    memset(hostExtLocation, 0, 8 * w * h * sizeof(struct _isExtremum));

    SHOWINFO(clEnqueueReadBuffer);
    ciErrNum = clEnqueueReadBuffer(clqueue, isExtremum, CL_TRUE, 0,
                                   8 * h * w * sizeof(struct _isExtremum), (struct _isExtremum *)hostExtLocation, 0, NULL, &WriteOut);
    SHOWERR(clEnqueueReadBuffer\t\thostExtLocation < -- -extLocation);

    clFinish(clqueue);
    //clWaitForEvents(1, &WriteOut);
    cOut = executionTime(WriteOut);

    printf("extre.dat:\n");
    for (int pi = 0; pi < 8 * w * h; pi++)
    {
        printf("%d\t%d\t%.6f\t%d\n", hostExtLocation[pi].x, hostExtLocation[pi].y,
                hostExtLocation[pi].scale, hostExtLocation[pi].lap);
    }

    free(hostExtLocation);

#endif

// GetOrientation kernel: Assign the supplied Ipoint an orientation
#define ORI_BLOCK 42
    SHOWINFO(clCreateBuffer);
    orientation = clCreateBuffer(context,
                                 CL_MEM_READ_WRITE,
                                 cmn * sizeof(float),
                                 NULL,
                                 &ciErrNum);
    SHOWERR(clCreateBuffer);

    /*
            cl_mem test = clCreateBuffer(context,
                                         CL_MEM_READ_WRITE,
                                         cmn * ORI_BLOCK * sizeof(float),
                                         NULL,
                                         &ciErrNum);
    */
    size_t szGetOriLocalWorkSize = ORI_BLOCK;
    size_t szGetOriGlobalWorkSize = cmn * ORI_BLOCK; // rounded up to the nearest multiple of the LocalWorkSize

    clSetKernelArg(ckGetOrientation, 0, sizeof(cl_mem), (void *)&isExtremum);
    clSetKernelArg(ckGetOrientation, 1, sizeof(cl_mem), (void *)&intImage);
    clSetKernelArg(ckGetOrientation, 2, sizeof(int), (void *)&h);
    clSetKernelArg(ckGetOrientation, 3, sizeof(int), (void *)&w);
    clSetKernelArg(ckGetOrientation, 4, sizeof(int), (void *)&stride);
    clSetKernelArg(ckGetOrientation, 5, sizeof(cl_mem), (void *)&orientation);
    clSetKernelArg(ckGetOrientation, 6, sizeof(int), (void *)&cmn);
#ifdef SLM
    clSetKernelArg(ckGetOrientation, 7, 109 * sizeof(float), 0);
    clSetKernelArg(ckGetOrientation, 8, 109 * sizeof(float), 0);
    clSetKernelArg(ckGetOrientation, 9, 109 * sizeof(float), 0);
    clSetKernelArg(ckGetOrientation, 10, 48 * sizeof(float), 0);
    clSetKernelArg(ckGetOrientation, 11, 48 * sizeof(float), 0);
#endif // SLM
    //        clSetKernelArg(ckGetOrientation, 12,sizeof(cl_mem), (void *)&test);

    SHOWINFO(clEnqueueNDRangeKernel);
    ciErrNum = clEnqueueNDRangeKernel(clqueue,
                                      ckGetOrientation,
                                      1, NULL,
                                      &szGetOriGlobalWorkSize,
                                      &szGetOriLocalWorkSize,
                                      0, NULL, &OriEvent);
    SHOWERR(clEnqueueNDRangeKernel\t\tckGetOrientation);

    clFinish(clqueue);
    //clWaitForEvents(1, &OriEvent);
    cOri = executionTime(OriEvent);

#ifdef profile
    float *hostOri = (float *)malloc(cmn * sizeof(float));

    SHOWINFO(clEnqueueReadBuffer);
    ciErrNum = clEnqueueReadBuffer(clqueue, orientation, CL_TRUE, 0, cmn * sizeof(float), (float *)hostOri, 0, NULL, NULL);
    SHOWERR(clEnqueueReadBuffer\t\thostOri < -- -ori test);

    printf("ori.dat:\n");
    for (int ori = 0; ori < cmn; ori++)
    {
        if (ori % 10 == 0 && ori != 0)
        {
            printf("\n");
        }
        printf("%.6f  ", hostOri[ori]);
    }
    free(hostOri);

    /*
    float* hostTest = (float*)malloc(cmn * ORI_BLOCK * sizeof(float));
    ciErrNum = clEnqueueReadBuffer(clqueue, test, CL_TRUE, 0, cmn * ORI_BLOCK * sizeof(float), (float *)hostTest, 0, NULL, NULL);
    FILE* tesfp = fopen("tes.dat", "w");
    for(int ori = 0; ori < cmn * ORI_BLOCK; ori++)
    {
            if(ori % 7 == 0 && ori != 0)
            {
                    fprintf(tesfp, "\n");
            }
            fprintf(tesfp, "%.6f\t", hostTest[ori]);

    }
    fclose(tesfp);
    free(hostTest);
    */

#endif

    // ckdesReady kernel: compute xs,ys, gauss_s2 to get ready for compute descriptor
    xs = clCreateBuffer(context,
                        CL_MEM_READ_WRITE,
                        16 * cmn * sizeof(int),
                        NULL,
                        &ciErrNum);
    ys = clCreateBuffer(context,
                        CL_MEM_READ_WRITE,
                        16 * cmn * sizeof(int),
                        NULL,
                        &ciErrNum);
    gauss_s2 = clCreateBuffer(context,
                              CL_MEM_READ_WRITE,
                              16 * cmn * sizeof(float),
                              NULL,
                              &ciErrNum);
#define BLOCK_SZ 4

    size_t szdesReadyGlobalWorkSize[] = {(size_t)cmn * 4, 4};
    size_t szdesReadyLocalWorkSize[] = {BLOCK_SZ, BLOCK_SZ};

    clSetKernelArg(ckdesReady, 0, sizeof(cl_mem), (void *)&isExtremum);
    clSetKernelArg(ckdesReady, 1, sizeof(cl_mem), (void *)&orientation);
    clSetKernelArg(ckdesReady, 2, sizeof(cl_mem), (void *)&xs);
    clSetKernelArg(ckdesReady, 3, sizeof(cl_mem), (void *)&ys);
    clSetKernelArg(ckdesReady, 4, sizeof(cl_mem), (void *)&gauss_s2);

    SHOWINFO(clEnqueueNDRangeKernel);
    ciErrNum = clEnqueueNDRangeKernel(clqueue,
                                      ckdesReady,
                                      2, NULL,
                                      szdesReadyGlobalWorkSize,
                                      szdesReadyLocalWorkSize,
                                      0, NULL, &DesEvent);
    SHOWERR(clEnqueueNDRangeKernel\t\tckdesReady);

    clFinish(clqueue);
    //clWaitForEvents(1, &DesEvent);
    cDes = executionTime(DesEvent);

#ifdef profile
    int *hostXs = (int *)malloc(16 * cmn * sizeof(int));
    int *hostYs = (int *)malloc(16 * cmn * sizeof(int));
    float *hostGau = (float *)malloc(16 * cmn * sizeof(float));
    SHOWINFO(clEnqueueReadBuffer);
    ciErrNum = clEnqueueReadBuffer(clqueue, xs, CL_TRUE, 0, 16 * cmn * sizeof(int), (int *)hostXs, 0, NULL, NULL);
    ciErrNum = clEnqueueReadBuffer(clqueue, ys, CL_TRUE, 0, 16 * cmn * sizeof(int), (int *)hostYs, 0, NULL, NULL);
    ciErrNum = clEnqueueReadBuffer(clqueue, gauss_s2, CL_TRUE, 0, 16 * cmn * sizeof(float), (float *)hostGau, 0, NULL, NULL);
    SHOWERR(clEnqueueReadBuffer\t\thostDes < -- -Xs Ys gauss_s2);

    printf("xs.dat:\n");
    int sc;
    for (sc = 0; sc < 16 * cmn; sc++)
    {
        if (sc % 8 == 0 && sc != 0)
        {
            printf("\n");
        }
        printf("%d\t", hostXs[sc]);
    }

    printf("ys.dat:\n");
    for (sc = 0; sc < 16 * cmn; sc++)
    {
        if (sc % 8 == 0 && sc != 0)
        {
            printf("\n");
        }
        printf("%d\t", hostYs[sc]);
    }

    printf("gauss.dat:\n");
    for (sc = 0; sc < 16 * cmn; sc++)
    {
        if (sc % 8 == 0 && sc != 0)
        {
            printf("\n");
        }
        printf("%.6f\t", hostGau[sc]);
    }

    free(hostXs);
    free(hostYs);
    free(hostGau);
#endif

    // GetDescriptor kernel: Calculate descriptor for the interest points
    des = clCreateBuffer(context,
                         CL_MEM_READ_WRITE,
                         16 * 4 * cmn * sizeof(float),
                         NULL,
                         &ciErrNum);

    /*
    rry = clCreateBuffer(context,
                       CL_MEM_READ_WRITE,
                       16 * 81 * cmn * sizeof(float),
                       NULL,
                       &ciErrNum);
    rrx = clCreateBuffer(context,
                       CL_MEM_READ_WRITE,
                       16 * 81 * cmn * sizeof(float),
                       NULL,
                       &ciErrNum);
    */

#define DES_BLOCK 9
    int localWorkSize = DES_BLOCK * DES_BLOCK;
    int group = cmn * 16;
    size_t szcomputeDesLocalWorkSize[] = {DES_BLOCK, DES_BLOCK};
    size_t xx = shrRoundUp((int)DES_BLOCK, cmn * 16 * 9); // rounded up to the nearest multiple of the LocalWorkSize
    size_t yy = shrRoundUp((int)DES_BLOCK, 9);            // rounded up to the nearest multiple of the LocalWorkSize
    size_t szcomputeDesGlobalWorkSize[] = {xx, yy};

    clSetKernelArg(ckcomputeDes, 0, sizeof(cl_mem), (void *)&isExtremum);
    clSetKernelArg(ckcomputeDes, 1, sizeof(cl_mem), (void *)&orientation);
    clSetKernelArg(ckcomputeDes, 2, sizeof(cl_mem), (void *)&intImage);
    clSetKernelArg(ckcomputeDes, 3, sizeof(cl_mem), (void *)&xs);
    clSetKernelArg(ckcomputeDes, 4, sizeof(cl_mem), (void *)&ys);
    clSetKernelArg(ckcomputeDes, 5, sizeof(int), (void *)&h);
    clSetKernelArg(ckcomputeDes, 6, sizeof(int), (void *)&w);
    clSetKernelArg(ckcomputeDes, 7, sizeof(int), (void *)&stride);
    // clSetKernelArg(ckcomputeDes, 8, sizeof(cl_mem),    (void *)&rrx);
    // clSetKernelArg(ckcomputeDes, 9, sizeof(cl_mem),    (void *)&rry);
#ifdef SLM
    clSetKernelArg(ckcomputeDes, 8, localWorkSize * sizeof(float), 0);
    clSetKernelArg(ckcomputeDes, 9, localWorkSize * sizeof(float), 0);
    clSetKernelArg(ckcomputeDes, 10, sizeof(cl_mem), (void *)&gauss_s2);
    clSetKernelArg(ckcomputeDes, 11, sizeof(cl_mem), (void *)&des);
    clSetKernelArg(ckcomputeDes, 12, sizeof(int), (void *)&group);
#else // SLM
    clSetKernelArg(ckcomputeDes, 8, sizeof(cl_mem), (void *)&gauss_s2);
    clSetKernelArg(ckcomputeDes, 9, sizeof(cl_mem), (void *)&des);
    clSetKernelArg(ckcomputeDes, 10, sizeof(int), (void *)&group);
#endif // SLM

    SHOWINFO(clEnqueueNDRangeKernel);
    ciErrNum = clEnqueueNDRangeKernel(clqueue,
                                      ckcomputeDes,
                                      2, NULL,
                                      szcomputeDesGlobalWorkSize,
                                      szcomputeDesLocalWorkSize,
                                      0, NULL, &comEvent);
    SHOWERR(clEnqueueNDRangeKernel\t\tckcomputeDes);

    clFinish(clqueue);
    //clWaitForEvents(1, &comEvent);
    cCom = executionTime(comEvent);

#ifdef profile
    /*
            float* hostrrx = (float*)malloc(16 * 81 * cmn * sizeof(float));
            float* hostrry = (float*)malloc(16 * 81 * cmn * sizeof(float));
            SHOWINFO(clEnqueueReadBuffer);
            ciErrNum = clEnqueueReadBuffer(clqueue, rrx, CL_TRUE, 0, 16 * 81 * cmn * sizeof(float), (float *)hostrrx, 0, NULL, NULL);
            ciErrNum = clEnqueueReadBuffer(clqueue, rry, CL_TRUE, 0, 16 * 81 * cmn * sizeof(float), (float *)hostrry, 0, NULL, NULL);
            SHOWERR(clEnqueueReadBuffer\t\thostDes <--- rrx rry);


            FILE* rrxfp = fopen("rrx.dat", "w");
            FILE* rryfp = fopen("rry.dat", "w");
            int  rrc;
            for(rrc= 0; rrc< 16 * 81 * cmn; rrc++)
            {
                    if(rrc% 8 == 0 && rrc != 0)
                    {
                            fprintf(rrxfp, "\n");
                            fprintf(rryfp, "\n");
                    }
                    fprintf(rrxfp, "%.6f\t", hostrrx[ rrc]);
                    fprintf(rryfp, "%.6f\t", hostrry[ rrc]);
            }
            fclose(rrxfp);
            fclose(rryfp);

            free(hostrrx);
            free(hostrry);
    */
    float *hostDes = (float *)malloc(64 * cmn * sizeof(float));
    SHOWINFO(clEnqueueReadBuffer);
    ciErrNum = clEnqueueReadBuffer(clqueue, des, CL_TRUE, 0, 64 * cmn * sizeof(float), (float *)hostDes, 0, NULL, NULL);
    SHOWERR(clEnqueueReadBuffer\t\thostDes < -- -des);

    printf("des.dat:\n");
    int desc;
    for (desc = 0; desc < 64 * cmn; desc++)
    {
        if (desc % 8 == 0 && desc != 0)
        {
            printf("\n");
        }
        printf("%.6f\t", hostDes[desc]);
    }
    free(hostDes);
#endif

    // cknormalDes kernel: normalize descriptors
    ndes = clCreateBuffer(context,
                          CL_MEM_READ_WRITE,
                          64 * cmn * sizeof(float),
                          NULL,
                          &ciErrNum);
/*
mid = clCreateBuffer(context,
                   CL_MEM_READ_WRITE,
                   cmn * sizeof(float),
                   NULL,
                   &ciErrNum);
*/
#define NDES_BLOCK 64

    size_t szNdesLocalWorkSize = NDES_BLOCK;
    size_t szNdesGlobalWorkSize = cmn * 64;

    clSetKernelArg(cknormalDes, 0, sizeof(cl_mem), (void *)&des);
    clSetKernelArg(cknormalDes, 1, sizeof(cl_mem), (void *)&ndes);
#ifdef SLM
    clSetKernelArg(cknormalDes, 2, NDES_BLOCK * sizeof(float), 0);
#endif // SLM

    SHOWINFO(clEnqueueNDRangeKernel);
    ciErrNum = clEnqueueNDRangeKernel(clqueue,
                                      cknormalDes,
                                      1, NULL,
                                      &szNdesGlobalWorkSize,
                                      &szNdesLocalWorkSize,
                                      0, NULL, &nDesEvent);
    SHOWERR(clEnqueueNDRangeKernel\t\tcknormalDes);

    clFinish(clqueue);
    //clWaitForEvents(1, &nDesEvent);
    cnDes = executionTime(nDesEvent);

#ifdef profile
    float *hostnDes = (float *)malloc(64 * cmn * sizeof(float));
    SHOWINFO(clEnqueueReadBuffer);
    ciErrNum = clEnqueueReadBuffer(clqueue, ndes, CL_TRUE, 0,
                                   64 * cmn * sizeof(float), (float *)hostnDes, 0, NULL, NULL);
    SHOWERR(clEnqueueReadBuffer\t\thostnDes < -- -ndes);

    printf("ndes.dat:\n");
    int ndesc;
    for (ndesc = 0; ndesc < 64 * cmn; ndesc++)
    {
        if (ndesc % 8 == 0 && ndesc != 0)
        {
            printf("\n");
        }
        printf("%.6f\t", hostnDes[ndesc]);
    }
    free(hostnDes);
#endif

    ///////////////////////////////////////////////////
    // Calculate the total OpenCL function execution time
#ifdef LINUX
    gettimeofday(&tEnd, NULL);
    double totalTime = (tEnd.tv_sec - tStart.tv_sec) * 1000 + (tEnd.tv_usec - tStart.tv_usec) / (float)1000;
#else
    clock_t tEnd = clock();
    double totalTime = (double)(tEnd - tStart) / CLOCKS_PER_SEC * 1000;
#endif

    SHOWINFO(cleanup);
    CLEANUP();
    ciErrNum = CL_SUCCESS;
    SHOWERR(cleanup);

    // cvReleaseImage(&img);
#ifndef OCV
    free(img.data);
#endif // OCV

    // Print the number of interest points
    printf("\nOpenCL SURF found:\t %d interest points\n", cmn);
    printf("create all kernel:\t %.3f ms\n", tCr);
#ifdef profile
    printf("Cpu Integral time:\t %.3f ms\n", tInt);
#endif
    printf("Row intImage time:\t %.3f ms\n", cRow);
    printf("Col intImage time:\t %.3f ms\n", cCol);
    printf("BuildResponse time:\t %.3f ms\n", cBui);
    printf("Found i_piont time:\t %.3f ms\n", cExt);
#ifdef profile
    printf("Write out i_point time:\t %.3f ms\n", cOut);
#endif
    printf("GetOritation time:\t %.3f ms\n", cOri);
    printf("Ready for des time:\t %.3f ms\n", cDes);
    printf("Compute Desc time:\t %.3f ms\n", cCom);
    printf("Normal des time:\t %.3f ms\n", cnDes);

    printf("=====================================\n");
    printf("total time:\t\t %.3f ms\n", totalTime);

    return 0;
}

// compute integral image function
void integralHost(Image &source, Image &intImageHost)
{
    Image img = getGray(source);

    int height = img.rows;
    int width = img.cols;
    int step = img.step / sizeof(float);
    float *data = (float *)img.ptr<float>(0);
    float *i_data = (float *)intImageHost.ptr<float>(0);
    float rs = 0.0f;
    for (int j = 0; j < width; j++)
    {
        rs += data[j];
        i_data[j] = rs;
    }

    for (int i = 1; i < height; ++i)
    {
        rs = 0.0f;

        for (int j = 0; j < width; ++j)
        {
            rs += data[i * step + j];
            i_data[i * step + j] = rs + i_data[(i - 1) * step + j];
        }
    }

    // cvReleaseImage(&img);
}

}