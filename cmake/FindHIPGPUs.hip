#include <stdio.h>
#include <hip/hip_runtime.h>

int main() {
    int deviceCount, device;
    int gpuDeviceCount = 0;
    printf("0");

    struct hipDeviceProp_t properties;
    hipError_t cudaResultCode = hipGetDeviceCount(&deviceCount);
    if (cudaResultCode != hipSuccess) 
        deviceCount = 0;

    // machines with no GPUs can still report one emulation device
    for (device = 0; device < deviceCount; ++device) {
        hipGetDeviceProperties(&properties, device);
        if (properties.major != 9999) // 9999 means emulation only
            ++gpuDeviceCount;
    }
    printf("%i\n", gpuDeviceCount);

    // don't return the number of gpus in case there are other errors
    return gpuDeviceCount > 0 ? 0:1;
}
