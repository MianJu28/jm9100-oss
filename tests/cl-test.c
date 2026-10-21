/* Real OpenCL kernel execution test on MWV207 via rusticl */
#include <CL/cl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(void) {
    cl_platform_id plat;
    clGetPlatformIDs(1, &plat, NULL);
    cl_device_id dev;
    clGetDeviceIDs(plat, CL_DEVICE_TYPE_GPU, 1, &dev, NULL);
    cl_context ctx = clCreateContext(NULL, 1, &dev, NULL, NULL, NULL);
    cl_command_queue q = clCreateCommandQueue(ctx, dev, 0, NULL);

    const char *src = 
    "__kernel void vecadd(__global const float *a, __global const float *b,\n"
    "                      __global float *c, const unsigned n) {\n"
    "    int i = get_global_id(0);\n"
    "    if (i < n) c[i] = a[i] * b[i] + 1.0f;\n"
    "}\n";
    cl_program prog = clCreateProgramWithSource(ctx, 1, &src, NULL, NULL);
    cl_int r = clBuildProgram(prog, 1, &dev, NULL, NULL, NULL);
    if (r != CL_SUCCESS) {
        char log[4096]; clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, sizeof log, log, NULL);
        printf("build failed: %s\n", log); return 1;
    }
    cl_kernel k = clCreateKernel(prog, "vecadd", NULL);

    const unsigned n = 65536;
    float *a = malloc(n*4), *b = malloc(n*4), *c = calloc(n, 4);
    for (unsigned i = 0; i < n; i++) { a[i] = i * 0.5f; b[i] = 2.0f; }

    cl_mem ma = clCreateBuffer(ctx, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR, n*4, a, NULL);
    cl_mem mb = clCreateBuffer(ctx, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR, n*4, b, NULL);
    cl_mem mc = clCreateBuffer(ctx, CL_MEM_WRITE_ONLY, n*4, NULL, NULL);

    clSetKernelArg(k, 0, sizeof(cl_mem), &ma);
    clSetKernelArg(k, 1, sizeof(cl_mem), &mb);
    clSetKernelArg(k, 2, sizeof(cl_mem), &mc);
    clSetKernelArg(k, 3, sizeof(unsigned), &n);

    size_t gws = n;
    clEnqueueNDRangeKernel(q, k, 1, NULL, &gws, NULL, 0, NULL, NULL);
    clFinish(q);
    clEnqueueReadBuffer(q, mc, CL_TRUE, 0, n*4, c, 0, NULL, NULL);

    int bad = 0;
    for (unsigned i = 0; i < n; i++)
        if (c[i] != a[i]*b[i] + 1.0f) { bad++; if (bad<4) printf("mismatch @%u: %f != %f\n", i, c[i], a[i]*b[i]+1.0f); }
    printf(bad ? "OPENCL COMPUTE: FAIL (%d mismatches)\n" : "OPENCL COMPUTE: PASS (65536-element vector kernel, 0 mismatches)\n", bad);

    clReleaseMemObject(ma); clReleaseMemObject(mb); clReleaseMemObject(mc);
    clReleaseKernel(k); clReleaseProgram(prog); clReleaseCommandQueue(q); clReleaseContext(ctx);
    return bad ? 1 : 0;
}
