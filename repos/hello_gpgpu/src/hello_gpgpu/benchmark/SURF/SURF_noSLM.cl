/**********************************************************************
Copyright (c) 2014,BUAA COMPILER LAB;
All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
- Neither the name of the BUAA COMPILER LAB; nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
********************************************************************/


#define MIN(a,b) ((a) < (b)? (a) : (b))

float MAX(float a, float b)
{
	if(isgreater(a, b) == 1)
		return a;
	return b;
}

#define OCTAVES	    4
#define INTERVALS   4 
#define thresh	    0.0004

inline float BoxIntegral(__global float* img, int step, int height, int width, int row, int col, int rows, int cols)
{
  __global float *data = img;
 
  int r1 = MIN(row,          height) - 1;
  int c1 = MIN(col,          width)  - 1;
  int r2 = MIN(row + rows,   height) - 1;
  int c2 = MIN(col + cols,   width)  - 1;
 
  float A = 0.0, B = 0.0, C = 0.0, D = 0.0;
  if (r1 >= 0 && c1 >= 0) A = data[r1 * step + c1];
  if (r1 >= 0 && c2 >= 0) B = data[r1 * step + c2];
  if (r2 >= 0 && c1 >= 0) C = data[r2 * step + c1];
  if (r2 >= 0 && c2 >= 0) D = data[r2 * step + c2];
  return MAX(0.0, A - B - C + D); 
}

__constant float pi = 3.14159f;
__constant float co[10]   = {1, 1, 1, 1, 0.5, 0.5, 0.25, 0.25, 0.125, 0.125};
__constant int filter[10] = {9, 15, 21, 27, 39, 51, 75, 99, 147, 195};
__constant int filter_map[8][3] = {0, 1, 2, 1, 2, 3, 1, 3, 4, 3, 4, 5, 
			  3, 5, 6, 5, 6, 7, 5, 7, 8, 7, 8, 9}; 

__constant float gauss25 [7][7] = {
  0.02350693969273,0.01849121369071,0.01239503121241,0.00708015417522,0.00344628101733,0.00142945847484,0.00050524879060,
  0.02169964028389,0.01706954162243,0.01144205592615,0.00653580605408,0.00318131834134,0.00131955648461,0.00046640341759,
  0.01706954162243,0.01342737701584,0.00900063997939,0.00514124713667,0.00250251364222,0.00103799989504,0.00036688592278,
  0.01144205592615,0.00900063997939,0.00603330940534,0.00344628101733,0.00167748505986,0.00069579213743,0.00024593098864,
  0.00653580605408,0.00514124713667,0.00344628101733,0.00196854695367,0.00095819467066,0.00039744277546,0.00014047800980,
  0.00318131834134,0.00250251364222,0.00167748505986,0.00095819467066,0.00046640341759,0.00019345616757,0.00006837798818,
  0.00131955648461,0.00103799989504,0.00069579213743,0.00039744277546,0.00019345616757,0.00008024231247,0.00002836202103
};

#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

inline int fRound(float flt)
{
  return (int) floor(flt+0.5f);
}

inline float gaussian1(int x, int y, float sig)
{
  return (1.0f/(2.0f*pi*sig*sig)) * exp( -(x*x+y*y)/(2.0f*sig*sig));
}

inline float gaussian2(float x, float y, float sig)
{
  return 1.0f/(2.0f*pi*sig*sig) * exp( -(x*x+y*y)/(2.0f*sig*sig));
}

inline float haarX(__global float* img, int step, int height, int width, int row, int column, int sc)
{
  return BoxIntegral(img, step, height, width, row-sc/2, column, sc, sc/2) 
    -BoxIntegral(img, step, height, width, row-sc/2, column-sc/2, sc, sc/2);
}

inline float haarY(__global float* img, int step, int height, int width, int row, int column, int sc)
{
  return BoxIntegral(img, step, height, width, row, column-sc/2, sc/2, sc) 
    -BoxIntegral(img, step, height, width, row-sc/2, column-sc/2, sc/2, sc);
}

float getAngle(float X, float Y)
{
  if(X >= 0 && Y >= 0)
    return atan(Y/X);

  if(X < 0 && Y >= 0)
    return pi - atan(-Y/X);

  if(X < 0 && Y < 0)
    return pi + atan(Y/X);

  if(X >= 0 && Y < 0)
    return 2*pi - atan(-Y/X);

  return 0;
}

inline float getResponse2(__global float* responses, unsigned int row, unsigned int column, int the, int h, int w)
  {
        return responses[the * h * w + (int)(row * co[the] * w) + column];
  }
 
inline float getResponse3(__global float* responses, unsigned int row, unsigned int column, int src, int the, int h, int w)
{
        int scale = (int)(co[the] / co[src] );
        return responses[the * h * w + (int)((scale * row) * co[the] * w + (scale * column))];
}
 
inline  float getLaplacian2(__global float* laplacian, unsigned int row, unsigned int column, int the, int h, int w)
  {
    return laplacian[the * h * w + (int)(row * co[the] * w) + column];
  }
 
inline float getLaplacian3(__global float* laplacian, unsigned int row, unsigned int column, int src, int the, int h, int w)
  {
    int scale = (int)(co[the]/ co[src]);
 
    return laplacian[the * h * w + (int)((scale * row) * co[the] * w + (scale * column))];
  }

__kernel void Integral(__global float* source,
		       __global float* intImage,
                       uint height,
		       uint width,
		       uint stride)
{
  float rs = 0.0f;
  for(int j=0; j<width; j++) 
  {
    rs += source[j]; 
    intImage[j] = rs;
  }

  for(int i=1; i<height; ++i) 
  {
    rs = 0.0f;
    for(int j=0; j<width; ++j) 
    {
      rs += source[i*stride+j]; 
      intImage[i*stride+j] = rs + intImage[(i-1)*stride+j];
    }
  }
}

__kernel void rowIntegral(
    __global float* src,
    __global float* dst,
    uint w)
{
    int row = get_global_id(0);
    float sum = 0.f;

    for (int i = 0; i < w; ++i) {
        sum += src[row * w + i];
        dst[row * w + i] = sum;
    }
}

__kernel void colIntegral(
    __global float* src,
    __global float* dst,
    uint h,
    uint w
)
{
    int col = get_global_id(0);
    if (col >= w) return;

    float sum = 0.f;
    for (int row = 0; row < h; ++row) {
        sum += src[row * w + col];
        dst[row * w + col] = sum;
    }
}

__kernel void BuildResponseLayer(__global float* responses,
                                 __global float* laplacian,
                                 __global float* img,
                                 uint h,
                                 uint w,
                                 uint s,
                                 uint stride				
				)
{
  int n, ar, ac, r, c;
  float Dxx, Dyy, Dxy;
  __local float loca[10];

  int devb, devl, devw;
  float inverse_area;
  int imgh = 2 * h,imgw = 2 * w;
  int x = h*w; 
	
  int gid = get_global_id(0);
  if (gid >= x)
    {
	return;
    }

  int x1 = x/4;
  int x2 = x/16;
  int x3 = x/64;

  for (n = 0; n < 10; n++){
    if (n < 4)
    {
    	ar = gid / w;
	ac = gid % w;
 	r = ar * 2;
	c = ac * 2;
	 
	devb = (filter[n] - 1) / 2 + 1;                  // border for this filter
	devl = filter[n] / 3;                            // lobe for this filter (filter size / 3)
	devw = filter[n];                             // filter size
	inverse_area = 1.000000 / (devw * devw);               // normalisation factor
     
    }
    if((n == 4) || (n == 5))
    {
	if (gid < x1)
	{
	  ar = gid / (w/2);
	  ac = gid % (w/2);
	  r = ar * 4;
 	  c = ac * 4;
	
          devb = (filter[n] - 1) / 2 + 1;                  // border for this filter
          devl = filter[n] / 3;                            // lobe for this filter (filter size / 3)
          devw = filter[n];                             // filter size
          inverse_area = 1.000000 / (devw * devw);               // normalisation factor
	
	}
	else
	  return;
    }
    if((n == 6) || (n == 7))
    {
        if (gid < x2)
        {
          ar = gid / (w/4);
          ac = gid % (w/4);
          r = ar * 8;
          c = ac * 8;

          devb = (filter[n] - 1) / 2 + 1;                  // border for this filter
          devl = filter[n] / 3;                            // lobe for this filter (filter size / 3)
          devw = filter[n];                             // filter size
          inverse_area = 1.000000 / (devw * devw);               // normalisation factor

        }
	else
	  return;   
    }
    if((n == 8) || (n == 9))
    {
        if (gid < x3)
        {
          ar = gid / (w/8);
          ac = gid % (w/8);
          r = ar * 16;
          c = ac * 16;

          devb = (filter[n] - 1) / 2 + 1;                  // border for this filter
          devl = filter[n] / 3;                            // lobe for this filter (filter size / 3)
          devw = filter[n];                             // filter size
          inverse_area = 1.000000 / (devw * devw);               // normalisation factor
        }
	else
	  return;
    }
    Dxx = BoxIntegral(img, stride, imgh ,imgw, r - devl + 1, c - devb, 2*devl - 1, devw)
          - BoxIntegral(img, stride, imgh, imgw, r - devl + 1, c - devl / 2, 2*devl - 1, devl)*3;
    Dyy = BoxIntegral(img, stride, imgh, imgw, r - devb, c - devl + 1, devw, 2*devl - 1)
          - BoxIntegral(img, stride, imgh, imgw, r - devl / 2, c - devl + 1, devl, 2*devl - 1)*3;
    Dxy = + BoxIntegral(img, stride, imgh, imgw, r - devl, c + 1, devl, devl)
            + BoxIntegral(img, stride, imgh, imgw, r + 1, c - devl, devl, devl)
            - BoxIntegral(img, stride, imgh, imgw, r - devl, c - devl, devl, devl)
            - BoxIntegral(img, stride, imgh, imgw, r + 1, c + 1, devl, devl);

    // Normalise the filter responses with respect to their size
    Dxx *= inverse_area;
    Dyy *= inverse_area;
    Dxy *= inverse_area;


    // Get the determinant of hessian response & laplacian sign
    responses[n * h * w + gid] = (Dxx * Dyy - 0.810000 * Dxy * Dxy);
    laplacian[n * h * w + gid] = (Dxx + Dyy >= 0 ? 1 : 0);

  }
}

struct _isExtremum{
        int x,y;
        float scale ;
        int lap;
};


__kernel void InterExtremum(__global float* responses,
                   __global float* laplacian,
		   __global struct _isExtremum* ext,
                   uint r, uint c, uint t, uint m, uint b,
                   uint h,
                   uint w,
		   __global int* cnum
                   )
{
    int step[10]   = {2, 2, 2, 2, 4, 4, 8, 8, 16, 16};
    float x[3] = {0, 0, 0};

    float dx;
    float dy;
    float ds;

    dx = (getResponse3(responses, r, c + 1, t, m, h, w) - getResponse3(responses, r, c - 1, t, m, h, w)) / 2.0;
    dy = (getResponse3(responses, r + 1, c, t, m, h, w) - getResponse3(responses, r - 1, c, t, m, h, w)) / 2.0;
    ds = (getResponse2(responses, r, c, t, h, w) - getResponse3(responses, r, c, t, b, h, w)) / 2.0;

    float v, dxx, dyy, dss, dxy, dxs, dys;

    v = getResponse3(responses, r, c, t, m, h, w);
    dxx = getResponse3(responses, r, c + 1, t, m, h, w) + getResponse3(responses, r, c - 1, t, m, h, w) - 2 * v;
    dyy = getResponse3(responses, r + 1, c, t, m, h, w) + getResponse3(responses, r - 1, c, t, m, h, w) - 2 * v;
    dss = getResponse2(responses, r, c, t, h, w) + getResponse3(responses, r, c, t, b, h, w) - 2 * v;
    dxy = ( getResponse3(responses, r + 1, c + 1, t, m, h, w) - getResponse3(responses, r + 1, c - 1, t, m, h, w) -
          getResponse3(responses, r - 1, c + 1, t, m, h, w) + getResponse3(responses, r - 1, c - 1, t, m, h, w) ) / 4.0;
    dxs = ( getResponse2(responses, r, c + 1, t, h, w) - getResponse2(responses, r, c - 1, t, h, w) -
          getResponse3(responses, r, c + 1, t, b, h, w) + getResponse3(responses, r, c - 1, t, b, h, w) ) / 4.0;
    dys = ( getResponse2(responses, r + 1, c, t, h, w) - getResponse2(responses, r - 1, c, t, h, w) -
          getResponse3(responses, r + 1, c, t, b, h, w) + getResponse3(responses, r - 1, c, t, b, h, w) ) / 4.0;

    float A11 = dyy*dss - dys*dys;  float A12 = dxs*dys - dxy*dss;   float A13 = dxy*dys - dyy*dxs;
    float A22 = dxx*dss - dxs*dxs;  float A23 = dxs*dxy - dxx*dys;
    float A33 = dxx*dyy - dxy*dxy;

    float A = dxx*A11 + dxy*A12 + dxs*A13;
    float dA;

    if(A!=0)
        {
          dA = 1/A;
          A11 = dA*A11; A12 = dA*A12; A13 = dA*A13;
          A22 = dA*A22; A23 = dA*A23;
          A33 = dA*A33;

          x[0] = A11*dx + A12*dy + A13*ds;
          x[1] = A12*dx + A22*dy + A23*ds;
          x[2] = A13*dx + A23*dy + A33*ds;
        }


    if( fabs( x[2] ) < 0.5f  &&  fabs( x[1] ) < 0.5f  &&  fabs( x[0] ) < 0.5f )
        {	
		 int index = atom_add(&cnum[0], 1);
		/*
                 ext[(m-1)*h*w + r*w + c].x = fRound(((c - x[0]) * step[t]));
                 ext[(m-1)*h*w + r*w + c].y = fRound(((r - x[1]) * step[t]));
                 ext[(m-1)*h*w + r*w + c].scale =  ((0.1333f) * (filter[m] - x[2] * (filter[m] - filter[b]))) ;
                 ext[(m-1)*h*w + r*w + c].lap = (int)(getLaplacian3(laplacian, r ,c, t, m, h, w)); 
        	*/
                 ext[index].x = fRound(((c - x[0]) * step[t]));
                 ext[index].y = fRound(((r - x[1]) * step[t]));
                 ext[index].scale =  ((0.1333f) * (filter[m] - x[2] * (filter[m] - filter[b]))) ;
                 ext[index].lap = (int)(getLaplacian3(laplacian, r ,c, t, m, h, w)); 
	}

}



__kernel void IsExtremum(__global float* responses,
			 __global float* laplacian,
                        __global struct _isExtremum* isExtremum,
                        uint h,
                        uint w,
			__global int*  cnum
			)
{

  int step[10]   = {2, 2, 2, 2, 4, 4, 8, 8, 16, 16};
  int r = get_global_id(0);
  int c = get_global_id(1);
  

  for (int i = 0; i < 8; i++)
  {
     int b = filter_map[i][0];
     int m = filter_map[i][1];
     int t = filter_map[i][2];
    
     int layerBorder = (filter[t] + 1) / (2 * step[t]);
     
     if ((r <= layerBorder) || (r >= (co[t] * h - layerBorder)) || (c <= layerBorder) || (c >= (co[t] * w - layerBorder)))
           continue;

     // check the candidate point in the middle layer is above thresh
     float candidate = getResponse3(responses, r, c, t, m, h, w);
     
     if (candidate < thresh)
           continue;
     
     int k = 1;
     for (int rr = -1; rr <=1; ++rr){
          for (int cc = -1; cc <=1; ++cc){
                 // if any response in 3x3x3 is greater candidate not maximum
                float tcan = getResponse2(responses, r+rr, c+cc, t, h, w);
                float mcan = getResponse3(responses, r+rr, c+cc, t, m, h, w);
                float bcan = getResponse3(responses, r+rr, c+cc, t, b, h, w);
                if( (tcan >= candidate) || ((rr != 0 && cc != 0) && (mcan >= candidate)) || (bcan >= candidate))
                  k = 0;
          }
     }
     if (k == 0)
	continue;

/*   isExtremum[i*h*w + r*w + c].x = r;
     isExtremum[i*h*w + r*w + c].y = c;
     isExtremum[i*h*w + r*w + c].scale = i+1;
     isExtremum[i*h*w + r*w + c].lap = (int)(getLaplacian3(laplacian, r ,c, t, m, h, w));
*/
     InterExtremum(responses, laplacian, isExtremum, r, c, t, m, b, h, w, cnum);
     	
   }
   
} 

__constant int i109[] = {
  -5,-5,-5,-5,-5,-5,-5,-4,-4,-4,-4,-4,-4,-4,-4,-4,
  -3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-2,-2,-2,-2,-2,
  -2,-2,-2,-2,-2,-2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
  -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2,
   2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4,
   4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5
};

__constant int j109[] = {
  -3,-2,-1, 0, 1, 2, 3,-4,-3,-2,-1, 0, 1, 2, 3, 4,
  -5,-4,-3,-2,-1, 0, 1, 2, 3, 4, 5,-5,-4,-3,-2,-1,
   0, 1, 2, 3, 4, 5,-5,-4,-3,-2,-1, 0, 1, 2, 3, 4,
   5,-5,-4,-3,-2,-1, 0, 1, 2, 3, 4, 5,-5,-4,-3,-2,
  -1, 0, 1, 2, 3, 4, 5,-5,-4,-3,-2,-1, 0, 1, 2, 3,
   4, 5,-5,-4,-3,-2,-1, 0, 1, 2, 3, 4, 5,-4,-3,-2,
  -1, 0, 1, 2, 3, 4,-3,-2,-1, 0, 1, 2, 3
};

__constant float gauss_lin[] = {
  0.000958195,	0.00167749,	0.00250251,	0.00318132,	0.00250251,	0.00167749,	0.000958195,	0.000958195,	
  0.00196855,	0.00344628,	0.00514125,	0.00653581,	0.00514125,	0.00344628,	0.00196855,	0.000958195,	
  0.000695792,	0.00167749,	0.00344628,	0.00603331,	0.00900064,	0.0114421,	0.00900064,	0.00603331,	
  0.00344628,	0.00167749,	0.000695792,	0.001038,	0.00250251,	0.00514125,	0.00900064,	0.0134274,	
  0.0170695,	0.0134274,	0.00900064,	0.00514125,	0.00250251,	0.001038,	0.00131956,	0.00318132,	
  0.00653581,	0.0114421,	0.0170695,	0.0216996,	0.0170695,	0.0114421,	0.00653581,	0.00318132,	
  0.00131956,	0.00142946,	0.00344628,	0.00708015,	0.012395,	0.0184912,	0.0235069,	0.0184912,	
  0.012395,	0.00708015,	0.00344628,	0.00142946,	0.00131956,	0.00318132,	0.00653581,	0.0114421,	
  0.0170695,	0.0216996,	0.0170695,	0.0114421,	0.00653581,	0.00318132,	0.00131956,	0.001038,	
  0.00250251,	0.00514125,	0.00900064,	0.0134274,	0.0170695,	0.0134274,	0.00900064,	0.00514125,	
  0.00250251,	0.001038,	0.000695792,	0.00167749,	0.00344628,	0.00603331,	0.00900064,	0.0114421,	
  0.00900064,	0.00603331,	0.00344628,	0.00167749,	0.000695792,	0.000958195,	0.00196855,	0.00344628,	
  0.00514125,	0.00653581,	0.00514125,	0.00344628,	0.00196855,	0.000958195,	0.000958195,	0.00167749,	
  0.00250251,	0.00318132,	0.00250251,	0.00167749,	0.000958195
};	
  
__kernel void GetOrientation(
    __global struct _isExtremum* isExtremum,
    __global float* img,
    uint h,
    uint w,
    uint stride,
    __global float* orientation,
    uint cmn
)
{
    int idx = get_global_id(0);
    if (idx >= cmn) return; // Bounds check: only process valid features

    // ---- Begin original per-feature logic ----

    int s = fRound(isExtremum[idx].scale);
    int r = isExtremum[idx].y;
    int c = isExtremum[idx].x;
    int imgh = 2 * h;
    int imgw = 2 * w;

    // Per-feature private arrays
    float resX[109];
    float resY[109];
    float Ang[109];
    float s_mer[48] = {0}; // At least 48 elements for reduction
    float s_ori[48] = {0};

    // Compute haar responses, populate private arrays
    for(int i = 0; i < 109; ++i) {
        float gauss = gauss_lin[i];
        resX[i] = gauss * haarX(img, stride, imgh, imgw, r + j109[i]*s, c + i109[i]*s, 4*s);
        resY[i] = gauss * haarY(img, stride, imgh, imgw, r + j109[i]*s, c + i109[i]*s, 4*s);
        Ang[i] = getAngle(resX[i], resY[i]);
    }

    // Dominant direction calculation
    for (int lid = 0; lid < 6; ++lid) {
        s_mer[lid+42] = 0.0f;
    }
    for (int lid = 0; lid < 48; ++lid) {
        float sumX = 0.f, sumY = 0.f;
        float ang1 = lid * 0.15f;
        float ang2 = ( ang1 + pi/3.f > 2.f*pi ? ang1-5.f*pi/3.f : ang1 + pi/3.f );

        for (int k = 0; k < 109; ++k) {
            float ang = Ang[k];
            if (ang1 < ang2 && ang1 < ang && ang < ang2)
            {
                sumX += resX[k];
                sumY += resY[k];
            }
            else if (ang2 < ang1 &&
                    ((ang > 0.f && ang < ang2) || (ang > ang1 && ang < 2.f*pi)))
            {
                sumX += resX[k];
                sumY += resY[k];
            }
        }
        s_mer[lid] = sumX*sumX + sumY*sumY;
        s_ori[lid] = getAngle(sumX, sumY);
    }

    // Reduction -- exactly as your reduction, but in a single thread on s_mer/s_ori
    for (int lcout = 24; lcout >= 3; lcout /= 2) {
        for (int lid = 0; lid < lcout; ++lid) {
            if (s_mer[lid] < s_mer[lid + lcout]) {
                s_mer[lid] = s_mer[lid + lcout];
                s_ori[lid] = s_ori[lid + lcout];
            }
        }
    }

    float max = 0.0f, ori = 0.0f;
    for (int i = 0; i < 3; ++i) {
        if (s_mer[i] > max) {
            max = s_mer[i];
            ori = s_ori[i];
        }
    }
    orientation[idx] = ori;
}


__kernel void desReady(__global struct _isExtremum* isExtremum,
                       __global float* orientation,
		       __global int* xs,
		       __global int* ys,
		       __global float* gauss_s2)
{
  int xx, yy;
  int i = 0, ix = 0, j = 0, jx = 0;
  float cx, cy;
 
  int index = get_group_id(0);
  
  xx = (int)(isExtremum[index].x);
  yy = (int)(isExtremum[index].y); 

  float scale = isExtremum[index].scale;
  float ori = orientation[index];
  float co = cos(ori);
  float si = sin(ori);

  int x = get_local_id(0);
  int y = get_local_id(1);

  i = x * 5 - 12;
  j = y * 5 - 12;
  ix = i + 5;
  jx = j + 5;
  cx = x + 0.5;
  cy = y + 0.5;
  
  xs[index * 16 + x * 4 + y] = fRound(xx + ( -jx*scale*si + ix*scale*co));
  ys[index * 16 + x * 4 + y] = fRound(yy + ( jx*scale*co + ix*scale*si));
  gauss_s2[index * 16 + x * 4 + y] = gaussian2(cx-2.0f,cy-2.0f,1.5f);
}


__constant int i[] = {-12, -12, -12, -12, -7, -7, -7, -7, -2, -2, -2, -2, 3, 3, 3, 3};
__constant int j[] = {-12, -7, -2, 3, -12, -7, -2, 3, -12, -7, -2, 3, -12, -7, -2, 3};


__kernel void computeDes(
    __global struct _isExtremum* isExtremum,
    __global float* orientation,
    __global float* img,
    __global int* xs,
    __global int* ys,
    uint h,
    uint w,
    uint stride,
    __global float* gauss_s2,
    __global float* des,
    uint group
)
{
    int index = get_global_id(0);
    if (index >= group) return;

    // --- Initialize feature ---
    int imgh = h << 1;
    int imgw = w << 1;
    int oriindex = (int)(index >> 4);
    float scale = isExtremum[oriindex].scale;
    int xx = (int)(isExtremum[oriindex].x);
    int yy = (int)(isExtremum[oriindex].y);
    float ori = orientation[oriindex];
    float co = cos(ori), si = sin(ori);
    int aa = index % 16;
    int k0 = i[aa];
    int l0 = j[aa];

    // --- Prepare workspace for all 9x9 positions ---
    float redx[81], redy[81];

    // --- Compute all sample points: each "x,y" in 0..8, 0..8 yields 81 samples ---
    int idx = 0;
    for (int x = 0; x < 9; ++x) {
        for (int y = 0; y < 9; ++y, ++idx) {
            int k = k0 + x;
            int l = l0 + y;

            // Rotated/sample coordinates
            int sample_x = fRound(xx + (-l*scale*si + k*scale*co));
            int sample_y = fRound(yy + ( l*scale*co + k*scale*si));
            float gauss_s1 = gaussian1(xs[index] - sample_x, ys[index] - sample_y, 2.5f * scale);
            float rx = haarX(img, stride, imgh, imgw, sample_y, sample_x, 2*fRound(scale));
            float ry = haarY(img, stride, imgh, imgw, sample_y, sample_x, 2*fRound(scale));
            redx[idx] = gauss_s1*(-rx*si + ry*co);
            redy[idx] = gauss_s1*(rx*co + ry*si);
        }
    }

    // --- Perform reduction ---
    float sumx = 0.f, sumy = 0.f, sumrx = 0.f, sumry = 0.f;
    for (int t = 0; t < 81; ++t) {
        sumx += redx[t];
        sumy += redy[t];
        sumrx += fabs(redx[t]);
        sumry += fabs(redy[t]);
    }

    float gs2 = gauss_s2[index];
    int mindex = index << 2;
    des[mindex + 0] = sumx * gs2;
    des[mindex + 1] = sumy * gs2;
    des[mindex + 2] = sumrx * gs2;
    des[mindex + 3] = sumry * gs2;
}

__kernel void normalDes(
    __global float* des,
    __global float* ndes
) {
    uint vec_stride = get_local_size(0);
    int vec_id = get_global_id(0);

    // Compute norm for this feature/descriptor vector
    float x = 0.0f;
    for (uint t = 0; t < vec_stride; ++t) {
        float v = des[vec_id * vec_stride + t];
        x += v * v;
    }
    float sx = sqrt(x);

    // Normalize this descriptor vector
    for (uint t = 0; t < vec_stride; ++t) {
        float v = des[vec_id * vec_stride + t];
        ndes[vec_id * vec_stride + t] = v / sx;
    }
}