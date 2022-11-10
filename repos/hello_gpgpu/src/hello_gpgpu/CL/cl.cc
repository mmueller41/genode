#define CL_TARGET_OPENCL_VERSION 100
#include "cl.h"
#include <gpgpu/gpgpu.h>
#pragma GCC diagnostic ignored "-Wunused-parameter"

#ifdef __cplusplus
extern "C" {
#endif

#define CL_MAX_KERNEL_ARGS 32
struct _cl_mem
{
    struct buffer_config bc;
    void* virt_vm; // virt addr that can be used by vm
    bool ocl_allocated;
};
struct _cl_command_queue
{
    struct kernel_config* kc;
    struct _cl_command_queue* next;
};
struct _cl_program
{
    uint8_t* binary;
    size_t size;
};

/* Genode */
static cl_genode* g_cl_genode;

extern CL_API_ENTRY void CL_API_CALL
clInitGenode(cl_genode& clg)
{
    // save global object
    g_cl_genode = &clg;
}

/* Platform API */
CL_API_ENTRY cl_int CL_API_CALL
clGetPlatformIDs(cl_uint          num_entries,
                 cl_platform_id * platforms,
                 cl_uint *        num_platforms)
{
    *platforms = 0;
    *num_platforms = 1;
    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL
clGetPlatformInfo(cl_platform_id   platform,
                  cl_platform_info param_name,
                  size_t           param_value_size,
                  void *           param_value,
                  size_t *         param_value_size_ret)
{
    if(platform != 0)
        return CL_INVALID_VALUE;

    static char name[] = "UOS-INTEL-GPGPU";
    static char ver[] = "1.0";
    static char vendor[] = "Uni Osnabrueck mld";

    switch (param_name)
    {
    case CL_PLATFORM_NAME:
    {
        char* val = (char*)param_value;
        for(size_t i = 0; i < sizeof(name) && i < param_value_size; i++)
            val[i] = name[i];
        break;
    }
    
    case CL_PLATFORM_VERSION:
    {
        char* val = (char*)param_value;
        for(size_t i = 0; i < sizeof(ver) && i < param_value_size; i++)
            val[i] = ver[i];
        break;
    }

    case CL_PLATFORM_VENDOR:
    {
        char* val = (char*)param_value;
        for(size_t i = 0; i < sizeof(vendor) && i < param_value_size; i++)
            val[i] = vendor[i];
        break;
    }

    default:
        return CL_INVALID_VALUE;
        break;
    }
    return CL_SUCCESS;
}

/* Device APIs */
CL_API_ENTRY cl_int CL_API_CALL
clGetDeviceIDs(cl_platform_id   platform,
               cl_device_type   device_type,
               cl_uint          num_entries,
               cl_device_id *   devices,
               cl_uint *        num_devices)
{
    if(device_type != CL_DEVICE_TYPE_GPU)
        return CL_INVALID_VALUE;

    *devices = 0;
    *num_devices = 1;
    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL
clGetDeviceInfo(cl_device_id    device,
                cl_device_info  param_name,
                size_t          param_value_size,
                void *          param_value,
                size_t *        param_value_size_ret)
{
    if(device != 0)
        return CL_INVALID_VALUE;

    static char name[] = "Intel GEN9 GPU";
    static char ver[] = "1.0";
    static char vendor[] = "Uni Osnabrueck mld";

    switch (param_name)
    {
    case CL_DEVICE_NAME:
    {
        char* val = (char*)param_value;
        for(size_t i = 0; i < sizeof(name) && i < param_value_size; i++)
            val[i] = name[i];
        break;
    }
    
    case CL_DRIVER_VERSION:
    case CL_DEVICE_VERSION:
    {
        char* val = (char*)param_value;
        for(size_t i = 0; i < sizeof(ver) && i < param_value_size; i++)
            val[i] = ver[i];
        break;
    }

    case CL_DEVICE_VENDOR:
    {
        char* val = (char*)param_value;
        for(size_t i = 0; i < sizeof(vendor) && i < param_value_size; i++)
            val[i] = vendor[i];
        break;
    }

    default:
        return CL_INVALID_VALUE;
        break;
    }
    return CL_SUCCESS;
}

#ifdef CL_VERSION_1_2

CL_API_ENTRY cl_int CL_API_CALL
clCreateSubDevices(cl_device_id                         in_device,
                   const cl_device_partition_property * properties,
                   cl_uint                              num_devices,
                   cl_device_id *                       out_devices,
                   cl_uint *                            num_devices_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clRetainDevice(cl_device_id device)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clReleaseDevice(cl_device_id device)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

#ifdef CL_VERSION_2_1

CL_API_ENTRY cl_int CL_API_CALL
clSetDefaultDeviceCommandQueue(cl_context           context,
                               cl_device_id         device,
                               cl_command_queue     command_queue)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clGetDeviceAndHostTimer(cl_device_id    device,
                        cl_ulong*       device_timestamp,
                        cl_ulong*       host_timestamp)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clGetHostTimer(cl_device_id device,
               cl_ulong *   host_timestamp)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

/* Context APIs */
CL_API_ENTRY cl_context CL_API_CALL
clCreateContext(const cl_context_properties * properties,
                cl_uint              num_devices,
                const cl_device_id * devices,
                void (CL_CALLBACK * pfn_notify)(const char * errinfo,
                                                const void * private_info,
                                                size_t       cb,
                                                void *       user_data),
                void *               user_data,
                cl_int *             errcode_ret)
{
    if(num_devices != 1 || *devices != 0)
    {
        *errcode_ret |= CL_INVALID_VALUE;
        return NULL;
    }

    *errcode_ret |= CL_SUCCESS;
    return NULL;
}

CL_API_ENTRY cl_context CL_API_CALL
clCreateContextFromType(const cl_context_properties * properties,
                        cl_device_type      device_type,
                        void (CL_CALLBACK * pfn_notify)(const char * errinfo,
                                                        const void * private_info,
                                                        size_t       cb,
                                                        void *       user_data),
                        void *              user_data,
                        cl_int *            errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

CL_API_ENTRY cl_int CL_API_CALL
clRetainContext(cl_context context)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clReleaseContext(cl_context context)
{
    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL
clGetContextInfo(cl_context         context,
                 cl_context_info    param_name,
                 size_t             param_value_size,
                 void *             param_value,
                 size_t *           param_value_size_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#ifdef CL_VERSION_3_0

CL_API_ENTRY cl_int CL_API_CALL
clSetContextDestructorCallback(cl_context         context,
                               void (CL_CALLBACK* pfn_notify)(cl_context context,
                                                              void* user_data),
                               void*              user_data)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

/* Command Queue APIs */

#ifdef CL_VERSION_2_0

CL_API_ENTRY cl_command_queue CL_API_CALL
clCreateCommandQueueWithProperties(cl_context               context,
                                   cl_device_id             device,
                                   const cl_queue_properties *    properties,
                                   cl_int *                 errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

#endif

CL_API_ENTRY cl_int CL_API_CALL
clRetainCommandQueue(cl_command_queue command_queue)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clReleaseCommandQueue(cl_command_queue command_queue)
{
    cl_command_queue cmd = command_queue;
    while(cmd != NULL)
    {
        cl_command_queue next = cmd->next;
        for(int i = 0; i < cmd->kc->buffCount; i++)
        {
            if(cmd->kc->buffConfigs[i].non_pointer_type)
            {
                g_cl_genode->free(cmd->kc->buffConfigs[i].buffer);
            }
        }
        g_cl_genode->free(cmd->kc->buffConfigs);
        g_cl_genode->free(cmd->kc);
        g_cl_genode->free(cmd);
        cmd = next;
    }
    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL
clGetCommandQueueInfo(cl_command_queue      command_queue,
                      cl_command_queue_info param_name,
                      size_t                param_value_size,
                      void *                param_value,
                      size_t *              param_value_size_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

/* Memory Object APIs */
CL_API_ENTRY cl_mem CL_API_CALL
clCreateBuffer(cl_context   context,
               cl_mem_flags flags,
               size_t       size,
               void *       host_ptr,
               cl_int *     errcode_ret)
{
    cl_mem clmem = (cl_mem)g_cl_genode->alloc(sizeof(struct _cl_mem));
    if(host_ptr == NULL)
    {
        host_ptr = g_cl_genode->aligned_alloc(0x1000, size);
        clmem->ocl_allocated = true;
    }
    else
    {
        clmem->ocl_allocated = false;
        Genode::error("[OCL] Memory mapping is currently unsupported!");
    }

    clmem->virt_vm = host_ptr;
    clmem->bc.buffer = host_ptr;
    clmem->bc.buffer_size = (uint32_t)size;
    clmem->bc.non_pointer_type = false;

    *errcode_ret |= CL_SUCCESS;
    return clmem;
}

#ifdef CL_VERSION_1_1

CL_API_ENTRY cl_mem CL_API_CALL
clCreateSubBuffer(cl_mem                   buffer,
                  cl_mem_flags             flags,
                  cl_buffer_create_type    buffer_create_type,
                  const void *             buffer_create_info,
                  cl_int *                 errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

#endif

#ifdef CL_VERSION_1_2

CL_API_ENTRY cl_mem CL_API_CALL
clCreateImage(cl_context              context,
              cl_mem_flags            flags,
              const cl_image_format * image_format,
              const cl_image_desc *   image_desc,
              void *                  host_ptr,
              cl_int *                errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

#endif

#ifdef CL_VERSION_2_0

CL_API_ENTRY cl_mem CL_API_CALL
clCreatePipe(cl_context                 context,
             cl_mem_flags               flags,
             cl_uint                    pipe_packet_size,
             cl_uint                    pipe_max_packets,
             const cl_pipe_properties * properties,
             cl_int *                   errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

#endif

#ifdef CL_VERSION_3_0

CL_API_ENTRY cl_mem CL_API_CALL
clCreateBufferWithProperties(cl_context                context,
                             const cl_mem_properties * properties,
                             cl_mem_flags              flags,
                             size_t                    size,
                             void *                    host_ptr,
                             cl_int *                  errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

CL_API_ENTRY cl_mem CL_API_CALL
clCreateImageWithProperties(cl_context                context,
                            const cl_mem_properties * properties,
                            cl_mem_flags              flags,
                            const cl_image_format *   image_format,
                            const cl_image_desc *     image_desc,
                            void *                    host_ptr,
                            cl_int *                  errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

#endif

CL_API_ENTRY cl_int CL_API_CALL
clRetainMemObject(cl_mem memobj)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clReleaseMemObject(cl_mem memobj)
{
    if(memobj->ocl_allocated && !memobj->bc.non_pointer_type)
    {
        g_cl_genode->free(memobj->virt_vm);
    }
    g_cl_genode->free(memobj);
    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL
clGetSupportedImageFormats(cl_context           context,
                           cl_mem_flags         flags,
                           cl_mem_object_type   image_type,
                           cl_uint              num_entries,
                           cl_image_format *    image_formats,
                           cl_uint *            num_image_formats)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clGetMemObjectInfo(cl_mem           memobj,
                   cl_mem_info      param_name,
                   size_t           param_value_size,
                   void *           param_value,
                   size_t *         param_value_size_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clGetImageInfo(cl_mem           image,
               cl_image_info    param_name,
               size_t           param_value_size,
               void *           param_value,
               size_t *         param_value_size_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#ifdef CL_VERSION_2_0

CL_API_ENTRY cl_int CL_API_CALL
clGetPipeInfo(cl_mem           pipe,
              cl_pipe_info     param_name,
              size_t           param_value_size,
              void *           param_value,
              size_t *         param_value_size_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

#ifdef CL_VERSION_1_1

CL_API_ENTRY cl_int CL_API_CALL
clSetMemObjectDestructorCallback(cl_mem memobj,
                                 void (CL_CALLBACK * pfn_notify)(cl_mem memobj,
                                                                 void * user_data),
                                 void * user_data)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

/* SVM Allocation APIs */

#ifdef CL_VERSION_2_0

CL_API_ENTRY void * CL_API_CALL
clSVMAlloc(cl_context       context,
           cl_svm_mem_flags flags,
           size_t           size,
           cl_uint          alignment)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

CL_API_ENTRY void CL_API_CALL
clSVMFree(cl_context        context,
          void *            svm_pointer)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
}

#endif

/* Sampler APIs */

#ifdef CL_VERSION_2_0

CL_API_ENTRY cl_sampler CL_API_CALL
clCreateSamplerWithProperties(cl_context                     context,
                              const cl_sampler_properties *  sampler_properties,
                              cl_int *                       errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

#endif

CL_API_ENTRY cl_int CL_API_CALL
clRetainSampler(cl_sampler sampler)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clReleaseSampler(cl_sampler sampler)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clGetSamplerInfo(cl_sampler         sampler,
                 cl_sampler_info    param_name,
                 size_t             param_value_size,
                 void *             param_value,
                 size_t *           param_value_size_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

/* Program Object APIs */
CL_API_ENTRY cl_program CL_API_CALL
clCreateProgramWithSource(cl_context        context,
                          cl_uint           count,
                          const char **     strings,
                          const size_t *    lengths,
                          cl_int *          errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

CL_API_ENTRY cl_program CL_API_CALL
clCreateProgramWithBinary(cl_context                     context,
                          cl_uint                        num_devices,
                          const cl_device_id *           device_list,
                          const size_t *                 lengths,
                          const unsigned char **         binaries,
                          cl_int *                       binary_status,
                          cl_int *                       errcode_ret)
{
    if(*device_list != 0x0)
    {
        *errcode_ret |= CL_INVALID_VALUE;
        return NULL;
    }

    cl_program p = (cl_program)g_cl_genode->alloc(sizeof(struct _cl_program));
    p->binary = (uint8_t*)binaries[0];
    p->size = lengths[0];

    *errcode_ret |= CL_SUCCESS;
    return p;
}

#ifdef CL_VERSION_1_2

CL_API_ENTRY cl_program CL_API_CALL
clCreateProgramWithBuiltInKernels(cl_context            context,
                                  cl_uint               num_devices,
                                  const cl_device_id *  device_list,
                                  const char *          kernel_names,
                                  cl_int *              errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

#endif

#ifdef CL_VERSION_2_1

CL_API_ENTRY cl_program CL_API_CALL
clCreateProgramWithIL(cl_context    context,
                     const void*    il,
                     size_t         length,
                     cl_int*        errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

#endif

CL_API_ENTRY cl_int CL_API_CALL
clRetainProgram(cl_program program)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clReleaseProgram(cl_program program)
{
    g_cl_genode->free(program);
    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL
clBuildProgram(cl_program           program,
               cl_uint              num_devices,
               const cl_device_id * device_list,
               const char *         options,
               void (CL_CALLBACK *  pfn_notify)(cl_program program,
                                                void * user_data),
               void *               user_data)
{
    return CL_SUCCESS;
}

#ifdef CL_VERSION_1_2

CL_API_ENTRY cl_int CL_API_CALL
clCompileProgram(cl_program           program,
                 cl_uint              num_devices,
                 const cl_device_id * device_list,
                 const char *         options,
                 cl_uint              num_input_headers,
                 const cl_program *   input_headers,
                 const char **        header_include_names,
                 void (CL_CALLBACK *  pfn_notify)(cl_program program,
                                                  void * user_data),
                 void *               user_data)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_program CL_API_CALL
clLinkProgram(cl_context           context,
              cl_uint              num_devices,
              const cl_device_id * device_list,
              const char *         options,
              cl_uint              num_input_programs,
              const cl_program *   input_programs,
              void (CL_CALLBACK *  pfn_notify)(cl_program program,
                                               void * user_data),
              void *               user_data,
              cl_int *             errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

#endif

#ifdef CL_VERSION_2_2

CL_API_ENTRY CL_API_PREFIX__VERSION_2_2_DEPRECATED cl_int CL_API_CALL
clSetProgramReleaseCallback(cl_program          program,
                            void (CL_CALLBACK * pfn_notify)(cl_program program,
                                                            void * user_data),
                            void *              user_data)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clSetProgramSpecializationConstant(cl_program  program,
                                   cl_uint     spec_id,
                                   size_t      spec_size,
                                   const void* spec_value)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

#ifdef CL_VERSION_1_2

CL_API_ENTRY cl_int CL_API_CALL
clUnloadPlatformCompiler(cl_platform_id platform)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

CL_API_ENTRY cl_int CL_API_CALL
clGetProgramInfo(cl_program         program,
                 cl_program_info    param_name,
                 size_t             param_value_size,
                 void *             param_value,
                 size_t *           param_value_size_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clGetProgramBuildInfo(cl_program            program,
                      cl_device_id          device,
                      cl_program_build_info param_name,
                      size_t                param_value_size,
                      void *                param_value,
                      size_t *              param_value_size_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

/* Kernel Object APIs */
CL_API_ENTRY cl_kernel CL_API_CALL
clCreateKernel(cl_program      program,
               const char *    kernel_name,
               cl_int *        errcode_ret)
{
    // create kernel
    struct kernel_config* kc = new(g_cl_genode->getAlloc()) kernel_config();
    
    // we can not just set the binary, because its not in shared mem => copy it
    kc->binary = (uint8_t*)g_cl_genode->alloc(program->size);
    uint8_t* bin = (uint8_t*)program->binary;
    for(size_t i = 0; i < program->size; i++)
    {
        kc->binary[i] = bin[i];
    }

    // preallocated 32 buff configs;
    kc->buffConfigs = new(g_cl_genode->getAlloc()) buffer_config[CL_MAX_KERNEL_ARGS];

    // get name size
    size_t size = 0;
    for(; kernel_name[size] != '\0'; size++);
    size++; // add '\0'
    kc->kernelName = (char*)g_cl_genode->alloc(size * sizeof(char));
    for(size_t i = 0; i < size; i++)
    {
        kc->kernelName[i] = kernel_name[i];
    }

    *errcode_ret |= CL_SUCCESS;
    return (cl_kernel)kc;
}

CL_API_ENTRY cl_int CL_API_CALL
clCreateKernelsInProgram(cl_program     program,
                         cl_uint        num_kernels,
                         cl_kernel *    kernels,
                         cl_uint *      num_kernels_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#ifdef CL_VERSION_2_1

CL_API_ENTRY cl_kernel CL_API_CALL
clCloneKernel(cl_kernel     source_kernel,
              cl_int*       errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

#endif

CL_API_ENTRY cl_int CL_API_CALL
clRetainKernel(cl_kernel    kernel)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clReleaseKernel(cl_kernel   kernel)
{
    struct kernel_config* kc = (struct kernel_config*)kernel;
    g_cl_genode->free(kc->binary);
    for(int i = 0; i < kc->buffCount; i++)
    {
        if(kc->buffConfigs[i].non_pointer_type)
        {
            g_cl_genode->free(kc->buffConfigs[i].buffer);
        }
    }
    g_cl_genode->free(kc->buffConfigs);
    g_cl_genode->free(kc->kernelName);
    g_cl_genode->free(kc);
    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL
clSetKernelArg(cl_kernel    kernel,
               cl_uint      arg_index,
               size_t       arg_size,
               const void * arg_value)
{
    if(arg_index > 31) // preallocated buffConfigs size
        return CL_INVALID_ARG_INDEX;

    struct kernel_config* kc = (struct kernel_config*)kernel;

    if(arg_size == sizeof(cl_mem))
    {
        cl_mem* clmem = (cl_mem*)arg_value;
        kc->buffConfigs[arg_index] = (*clmem)->bc;
    }
    else
    {
        struct buffer_config& bc = kc->buffConfigs[arg_index];
        
        // set buffer config
        bc.buffer = g_cl_genode->alloc(arg_size); // alloc shared mem
        bc.buffer_size = (uint32_t)arg_size;
        bc.non_pointer_type = true;

        // copy value to shared mem
        uint8_t* src = (uint8_t*)arg_value;
        uint8_t* dst = (uint8_t*)bc.buffer;
        for(size_t i = 0; i < arg_size; i++)
            dst[i] = src[i];
    }
    
    if(kc->buffCount < (arg_index + 1))
        kc->buffCount = (uint8_t)(arg_index + 1);

    return CL_SUCCESS;
}

#ifdef CL_VERSION_2_0

CL_API_ENTRY cl_int CL_API_CALL
clSetKernelArgSVMPointer(cl_kernel    kernel,
                         cl_uint      arg_index,
                         const void * arg_value)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clSetKernelExecInfo(cl_kernel            kernel,
                    cl_kernel_exec_info  param_name,
                    size_t               param_value_size,
                    const void *         param_value)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

CL_API_ENTRY cl_int CL_API_CALL
clGetKernelInfo(cl_kernel       kernel,
                cl_kernel_info  param_name,
                size_t          param_value_size,
                void *          param_value,
                size_t *        param_value_size_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#ifdef CL_VERSION_1_2

CL_API_ENTRY cl_int CL_API_CALL
clGetKernelArgInfo(cl_kernel       kernel,
                   cl_uint         arg_indx,
                   cl_kernel_arg_info  param_name,
                   size_t          param_value_size,
                   void *          param_value,
                   size_t *        param_value_size_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

CL_API_ENTRY cl_int CL_API_CALL
clGetKernelWorkGroupInfo(cl_kernel                  kernel,
                         cl_device_id               device,
                         cl_kernel_work_group_info  param_name,
                         size_t                     param_value_size,
                         void *                     param_value,
                         size_t *                   param_value_size_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#ifdef CL_VERSION_2_1

CL_API_ENTRY cl_int CL_API_CALL
clGetKernelSubGroupInfo(cl_kernel                   kernel,
                        cl_device_id                device,
                        cl_kernel_sub_group_info    param_name,
                        size_t                      input_value_size,
                        const void*                 input_value,
                        size_t                      param_value_size,
                        void*                       param_value,
                        size_t*                     param_value_size_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

/* Event Object APIs */
CL_API_ENTRY cl_int CL_API_CALL
clWaitForEvents(cl_uint             num_events,
                const cl_event *    event_list)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clGetEventInfo(cl_event         event,
               cl_event_info    param_name,
               size_t           param_value_size,
               void *           param_value,
               size_t *         param_value_size_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#ifdef CL_VERSION_1_1

CL_API_ENTRY cl_event CL_API_CALL
clCreateUserEvent(cl_context    context,
                  cl_int *      errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

#endif

CL_API_ENTRY cl_int CL_API_CALL
clRetainEvent(cl_event event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clReleaseEvent(cl_event event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#ifdef CL_VERSION_1_1

CL_API_ENTRY cl_int CL_API_CALL
clSetUserEventStatus(cl_event   event,
                     cl_int     execution_status)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clSetEventCallback(cl_event    event,
                   cl_int      command_exec_callback_type,
                   void (CL_CALLBACK * pfn_notify)(cl_event event,
                                                   cl_int   event_command_status,
                                                   void *   user_data),
                   void *      user_data)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

/* Profiling APIs */
CL_API_ENTRY cl_int CL_API_CALL
clGetEventProfilingInfo(cl_event            event,
                        cl_profiling_info   param_name,
                        size_t              param_value_size,
                        void *              param_value,
                        size_t *            param_value_size_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

/* Flush and Finish APIs */
CL_API_ENTRY cl_int CL_API_CALL
clFlush(cl_command_queue command_queue)
{
    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL
clFinish(cl_command_queue command_queue)
{
    cl_command_queue cmd = command_queue;

    do
    {
        // wait for it if there is something in the queue
        if(cmd->kc != NULL)
        {
            g_cl_genode->wait(cmd->kc);
        }

        // get the next one
        cmd = cmd->next;
    }
    while(cmd != NULL);

    return CL_SUCCESS;
}

/* Enqueued Commands APIs */
CL_API_ENTRY cl_int CL_API_CALL
clEnqueueReadBuffer(cl_command_queue    command_queue,
                    cl_mem              buffer,
                    cl_bool             blocking_read,
                    size_t              offset,
                    size_t              size,
                    void *              ptr,
                    cl_uint             num_events_in_wait_list,
                    const cl_event *    event_wait_list,
                    cl_event *          event)
{
    if(blocking_read == false)
    {
        return CL_INVALID_VALUE;
    }

    uint8_t* src = (uint8_t*)buffer->virt_vm;
    uint8_t* dst = (uint8_t*)ptr;
    for(size_t i = 0; i < size; i++)
    {
        dst[i] = src[i];
    }

    return CL_SUCCESS;
}

#ifdef CL_VERSION_1_1

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueReadBufferRect(cl_command_queue    command_queue,
                        cl_mem              buffer,
                        cl_bool             blocking_read,
                        const size_t *      buffer_origin,
                        const size_t *      host_origin,
                        const size_t *      region,
                        size_t              buffer_row_pitch,
                        size_t              buffer_slice_pitch,
                        size_t              host_row_pitch,
                        size_t              host_slice_pitch,
                        void *              ptr,
                        cl_uint             num_events_in_wait_list,
                        const cl_event *    event_wait_list,
                        cl_event *          event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueWriteBuffer(cl_command_queue   command_queue,
                     cl_mem             buffer,
                     cl_bool            blocking_write,
                     size_t             offset,
                     size_t             size,
                     const void *       ptr,
                     cl_uint            num_events_in_wait_list,
                     const cl_event *   event_wait_list,
                     cl_event *         event)
{
    if(blocking_write == false)
    {
        return CL_INVALID_VALUE;
    }

    uint8_t* src = (uint8_t*)ptr;
    uint8_t* dst = (uint8_t*)buffer->virt_vm;
    for(size_t i = 0; i < size; i++)
    {
        dst[i] = src[i];
    }

    return CL_SUCCESS;
}

#ifdef CL_VERSION_1_1

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueWriteBufferRect(cl_command_queue    command_queue,
                         cl_mem              buffer,
                         cl_bool             blocking_write,
                         const size_t *      buffer_origin,
                         const size_t *      host_origin,
                         const size_t *      region,
                         size_t              buffer_row_pitch,
                         size_t              buffer_slice_pitch,
                         size_t              host_row_pitch,
                         size_t              host_slice_pitch,
                         const void *        ptr,
                         cl_uint             num_events_in_wait_list,
                         const cl_event *    event_wait_list,
                         cl_event *          event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

#ifdef CL_VERSION_1_2

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueFillBuffer(cl_command_queue   command_queue,
                    cl_mem             buffer,
                    const void *       pattern,
                    size_t             pattern_size,
                    size_t             offset,
                    size_t             size,
                    cl_uint            num_events_in_wait_list,
                    const cl_event *   event_wait_list,
                    cl_event *         event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueCopyBuffer(cl_command_queue    command_queue,
                    cl_mem              src_buffer,
                    cl_mem              dst_buffer,
                    size_t              src_offset,
                    size_t              dst_offset,
                    size_t              size,
                    cl_uint             num_events_in_wait_list,
                    const cl_event *    event_wait_list,
                    cl_event *          event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#ifdef CL_VERSION_1_1

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueCopyBufferRect(cl_command_queue    command_queue,
                        cl_mem              src_buffer,
                        cl_mem              dst_buffer,
                        const size_t *      src_origin,
                        const size_t *      dst_origin,
                        const size_t *      region,
                        size_t              src_row_pitch,
                        size_t              src_slice_pitch,
                        size_t              dst_row_pitch,
                        size_t              dst_slice_pitch,
                        cl_uint             num_events_in_wait_list,
                        const cl_event *    event_wait_list,
                        cl_event *          event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueReadImage(cl_command_queue     command_queue,
                   cl_mem               image,
                   cl_bool              blocking_read,
                   const size_t *       origin,
                   const size_t *       region,
                   size_t               row_pitch,
                   size_t               slice_pitch,
                   void *               ptr,
                   cl_uint              num_events_in_wait_list,
                   const cl_event *     event_wait_list,
                   cl_event *           event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueWriteImage(cl_command_queue    command_queue,
                    cl_mem              image,
                    cl_bool             blocking_write,
                    const size_t *      origin,
                    const size_t *      region,
                    size_t              input_row_pitch,
                    size_t              input_slice_pitch,
                    const void *        ptr,
                    cl_uint             num_events_in_wait_list,
                    const cl_event *    event_wait_list,
                    cl_event *          event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#ifdef CL_VERSION_1_2

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueFillImage(cl_command_queue   command_queue,
                   cl_mem             image,
                   const void *       fill_color,
                   const size_t *     origin,
                   const size_t *     region,
                   cl_uint            num_events_in_wait_list,
                   const cl_event *   event_wait_list,
                   cl_event *         event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueCopyImage(cl_command_queue     command_queue,
                   cl_mem               src_image,
                   cl_mem               dst_image,
                   const size_t *       src_origin,
                   const size_t *       dst_origin,
                   const size_t *       region,
                   cl_uint              num_events_in_wait_list,
                   const cl_event *     event_wait_list,
                   cl_event *           event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueCopyImageToBuffer(cl_command_queue command_queue,
                           cl_mem           src_image,
                           cl_mem           dst_buffer,
                           const size_t *   src_origin,
                           const size_t *   region,
                           size_t           dst_offset,
                           cl_uint          num_events_in_wait_list,
                           const cl_event * event_wait_list,
                           cl_event *       event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueCopyBufferToImage(cl_command_queue command_queue,
                           cl_mem           src_buffer,
                           cl_mem           dst_image,
                           size_t           src_offset,
                           const size_t *   dst_origin,
                           const size_t *   region,
                           cl_uint          num_events_in_wait_list,
                           const cl_event * event_wait_list,
                           cl_event *       event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY void * CL_API_CALL
clEnqueueMapBuffer(cl_command_queue command_queue,
                   cl_mem           buffer,
                   cl_bool          blocking_map,
                   cl_map_flags     map_flags,
                   size_t           offset,
                   size_t           size,
                   cl_uint          num_events_in_wait_list,
                   const cl_event * event_wait_list,
                   cl_event *       event,
                   cl_int *         errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

CL_API_ENTRY void * CL_API_CALL
clEnqueueMapImage(cl_command_queue  command_queue,
                  cl_mem            image,
                  cl_bool           blocking_map,
                  cl_map_flags      map_flags,
                  const size_t *    origin,
                  const size_t *    region,
                  size_t *          image_row_pitch,
                  size_t *          image_slice_pitch,
                  cl_uint           num_events_in_wait_list,
                  const cl_event *  event_wait_list,
                  cl_event *        event,
                  cl_int *          errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueUnmapMemObject(cl_command_queue command_queue,
                        cl_mem           memobj,
                        void *           mapped_ptr,
                        cl_uint          num_events_in_wait_list,
                        const cl_event * event_wait_list,
                        cl_event *       event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#ifdef CL_VERSION_1_2

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueMigrateMemObjects(cl_command_queue       command_queue,
                           cl_uint                num_mem_objects,
                           const cl_mem *         mem_objects,
                           cl_mem_migration_flags flags,
                           cl_uint                num_events_in_wait_list,
                           const cl_event *       event_wait_list,
                           cl_event *             event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueNDRangeKernel(cl_command_queue command_queue,
                       cl_kernel        kernel,
                       cl_uint          work_dim,
                       const size_t *   global_work_offset,
                       const size_t *   global_work_size,
                       const size_t *   local_work_size,
                       cl_uint          num_events_in_wait_list,
                       const cl_event * event_wait_list,
                       cl_event *       event)
{
    struct kernel_config* kc = (struct kernel_config*)kernel;
    for(cl_uint i = 0; i < work_dim; i++)
    {
        kc->range[i] = (uint32_t)global_work_size[i];
        if(local_work_size != NULL)
        {
            kc->workgroupsize[i] = (uint32_t)local_work_size[i];
        }
    }

    // create copy of kernel (vm and driver should not modify the same kernel)
    struct kernel_config* kcopy = new(g_cl_genode->getAlloc()) kernel_config();
    *kcopy = *kc;

    // also copy buff configs
    kcopy->buffConfigs = new(g_cl_genode->getAlloc()) buffer_config[CL_MAX_KERNEL_ARGS];
    for(int i = 0; i < kc->buffCount; i++)
    {
        kcopy->buffConfigs[i] = kc->buffConfigs[i];
    }

    // skip to end of queue
    cl_command_queue cmd = command_queue;
    for(;cmd->next != NULL; cmd = cmd->next);

    // enqueue
    if(cmd->kc == NULL)
    {
        cmd->kc = kcopy;
    }
    else // or extend queue
    {
        cl_command_queue n = (cl_command_queue)g_cl_genode->alloc(sizeof(struct _cl_command_queue));
        n->next = NULL;
        n->kc = kcopy;

        cmd->next = n;
    }

    g_cl_genode->enqueue_task(kcopy);
    return CL_SUCCESS;
}

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueNativeKernel(cl_command_queue  command_queue,
                      void (CL_CALLBACK * user_func)(void *),
                      void *            args,
                      size_t            cb_args,
                      cl_uint           num_mem_objects,
                      const cl_mem *    mem_list,
                      const void **     args_mem_loc,
                      cl_uint           num_events_in_wait_list,
                      const cl_event *  event_wait_list,
                      cl_event *        event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#ifdef CL_VERSION_1_2

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueMarkerWithWaitList(cl_command_queue  command_queue,
                            cl_uint           num_events_in_wait_list,
                            const cl_event *  event_wait_list,
                            cl_event *        event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueBarrierWithWaitList(cl_command_queue  command_queue,
                             cl_uint           num_events_in_wait_list,
                             const cl_event *  event_wait_list,
                             cl_event *        event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

#ifdef CL_VERSION_2_0

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueSVMFree(cl_command_queue  command_queue,
                 cl_uint           num_svm_pointers,
                 void *            svm_pointers[],
                 void (CL_CALLBACK * pfn_free_func)(cl_command_queue queue,
                                                    cl_uint          num_svm_pointers,
                                                    void *           svm_pointers[],
                                                    void *           user_data),
                 void *            user_data,
                 cl_uint           num_events_in_wait_list,
                 const cl_event *  event_wait_list,
                 cl_event *        event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueSVMMemcpy(cl_command_queue  command_queue,
                   cl_bool           blocking_copy,
                   void *            dst_ptr,
                   const void *      src_ptr,
                   size_t            size,
                   cl_uint           num_events_in_wait_list,
                   const cl_event *  event_wait_list,
                   cl_event *        event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueSVMMemFill(cl_command_queue  command_queue,
                    void *            svm_ptr,
                    const void *      pattern,
                    size_t            pattern_size,
                    size_t            size,
                    cl_uint           num_events_in_wait_list,
                    const cl_event *  event_wait_list,
                    cl_event *        event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueSVMMap(cl_command_queue  command_queue,
                cl_bool           blocking_map,
                cl_map_flags      flags,
                void *            svm_ptr,
                size_t            size,
                cl_uint           num_events_in_wait_list,
                const cl_event *  event_wait_list,
                cl_event *        event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueSVMUnmap(cl_command_queue  command_queue,
                  void *            svm_ptr,
                  cl_uint           num_events_in_wait_list,
                  const cl_event *  event_wait_list,
                  cl_event *        event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

#ifdef CL_VERSION_2_1

CL_API_ENTRY cl_int CL_API_CALL
clEnqueueSVMMigrateMem(cl_command_queue         command_queue,
                       cl_uint                  num_svm_pointers,
                       const void **            svm_pointers,
                       const size_t *           sizes,
                       cl_mem_migration_flags   flags,
                       cl_uint                  num_events_in_wait_list,
                       const cl_event *         event_wait_list,
                       cl_event *               event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#endif

#ifdef CL_VERSION_1_2

/* Extension function access
 *
 * Returns the extension function address for the given function name,
 * or NULL if a valid function can not be found.  The client must
 * check to make sure the address is not NULL, before using or
 * calling the returned function address.
 */
CL_API_ENTRY void * CL_API_CALL
clGetExtensionFunctionAddressForPlatform(cl_platform_id platform,
                                         const char *   func_name)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

#endif

#ifdef CL_USE_DEPRECATED_OPENCL_1_0_APIS
    /*
     *  WARNING:
     *     This API introduces mutable state into the OpenCL implementation. It has been REMOVED
     *  to better facilitate thread safety.  The 1.0 API is not thread safe. It is not tested by the
     *  OpenCL 1.1 conformance test, and consequently may not work or may not work dependably.
     *  It is likely to be non-performant. Use of this API is not advised. Use at your own risk.
     *
     *  Software developers previously relying on this API are instructed to set the command queue
     *  properties when creating the queue, instead.
     */
    CL_API_ENTRY cl_int CL_API_CALL
    clSetCommandQueueProperty(cl_command_queue              command_queue,
                              cl_command_queue_properties   properties,
                              cl_bool                       enable,
                              cl_command_queue_properties * old_properties) CL_API_SUFFIX__VERSION_1_0_DEPRECATED;
#endif /* CL_USE_DEPRECATED_OPENCL_1_0_APIS */

/* Deprecated OpenCL 1.1 APIs */
CL_API_ENTRY CL_API_PREFIX__VERSION_1_1_DEPRECATED cl_mem CL_API_CALL
clCreateImage2D(cl_context              context,
                cl_mem_flags            flags,
                const cl_image_format * image_format,
                size_t                  image_width,
                size_t                  image_height,
                size_t                  image_row_pitch,
                void *                  host_ptr,
                cl_int *                errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

CL_API_ENTRY CL_API_PREFIX__VERSION_1_1_DEPRECATED cl_mem CL_API_CALL
clCreateImage3D(cl_context              context,
                cl_mem_flags            flags,
                const cl_image_format * image_format,
                size_t                  image_width,
                size_t                  image_height,
                size_t                  image_depth,
                size_t                  image_row_pitch,
                size_t                  image_slice_pitch,
                void *                  host_ptr,
                cl_int *                errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

CL_API_ENTRY CL_API_PREFIX__VERSION_1_1_DEPRECATED cl_int CL_API_CALL
clEnqueueMarker(cl_command_queue    command_queue,
                cl_event *          event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY CL_API_PREFIX__VERSION_1_1_DEPRECATED cl_int CL_API_CALL
clEnqueueWaitForEvents(cl_command_queue  command_queue,
                        cl_uint          num_events,
                        const cl_event * event_list)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY CL_API_PREFIX__VERSION_1_1_DEPRECATED cl_int CL_API_CALL
clEnqueueBarrier(cl_command_queue command_queue)
{
    return CL_SUCCESS;
}

CL_API_ENTRY CL_API_PREFIX__VERSION_1_1_DEPRECATED cl_int CL_API_CALL
clUnloadCompiler(void)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

CL_API_ENTRY CL_API_PREFIX__VERSION_1_1_DEPRECATED void * CL_API_CALL
clGetExtensionFunctionAddress(const char * func_name)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

/* Deprecated OpenCL 2.0 APIs */
CL_API_ENTRY CL_API_PREFIX__VERSION_1_2_DEPRECATED cl_command_queue CL_API_CALL
clCreateCommandQueue(cl_context                     context,
                     cl_device_id                   device,
                     cl_command_queue_properties    properties,
                     cl_int *                       errcode_ret)
{
    if(device != 0)
    {
        *errcode_ret |= CL_INVALID_VALUE;
        return NULL;
    }

    *errcode_ret |= CL_SUCCESS;
    cl_command_queue clcmdqueue = (cl_command_queue)g_cl_genode->alloc(sizeof(struct _cl_command_queue));
    clcmdqueue->kc = NULL;
    clcmdqueue->next = NULL;
    return clcmdqueue;
}

CL_API_ENTRY CL_API_PREFIX__VERSION_1_2_DEPRECATED cl_sampler CL_API_CALL
clCreateSampler(cl_context          context,
                cl_bool             normalized_coords,
                cl_addressing_mode  addressing_mode,
                cl_filter_mode      filter_mode,
                cl_int *            errcode_ret)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return NULL;
}

CL_API_ENTRY CL_API_PREFIX__VERSION_1_2_DEPRECATED cl_int CL_API_CALL
clEnqueueTask(cl_command_queue  command_queue,
              cl_kernel         kernel,
              cl_uint           num_events_in_wait_list,
              const cl_event *  event_wait_list,
              cl_event *        event)
{
    Genode::error("[OCL] func ", __func__, " is not implemented!");
    return CL_INVALID_VALUE;
}

#ifdef __cplusplus
}
#endif

#pragma GCC diagnostic pop
